// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

#include <algorithm>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

const int ClientSideDetectionService::kReportsIntervalDays = 1;
const int ClientSideDetectionService::kMaxReportsPerInterval = 3;
const int ClientSideDetectionService::kNegativeCacheIntervalDays = 1;
const int ClientSideDetectionService::kPositiveCacheIntervalMinutes = 30;

const char ClientSideDetectionService::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";

struct ClientSideDetectionService::ClientPhishingReportInfo {
  std::unique_ptr<network::SimpleURLLoader> loader;
  ClientReportPhishingRequestCallback callback;
  GURL phishing_url;
};

ClientSideDetectionService::CacheState::CacheState(bool phish, base::Time time)
    : is_phishing(phish), timestamp(time) {}

ClientSideDetectionService::ClientSideDetectionService(
    std::unique_ptr<Delegate> delegate,
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : delegate_(std::move(delegate)) {
  // delegate and prefs can be null in unit tests.
  if (!delegate_ || !delegate_->GetPrefs()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kClientSideDetectionKillswitch) &&
      opt_guide) {
    client_side_phishing_model_ =
        std::make_unique<ClientSidePhishingModel>(opt_guide);
  }

  url_loader_factory_ = delegate_->GetSafeBrowsingURLLoaderFactory();

  pref_change_registrar_.Init(delegate_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(&ClientSideDetectionService::OnPrefsUpdated,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&ClientSideDetectionService::OnPrefsUpdated,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingScoutReportingEnabled,
      base::BindRepeating(&ClientSideDetectionService::OnPrefsUpdated,
                          base::Unretained(this)));

  // Load the report times from preferences.
  LoadPhishingReportTimesFromPrefs();

  //  Do an initial check of the prefs.
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() {
  weak_factory_.InvalidateWeakPtrs();
}

void ClientSideDetectionService::Shutdown() {
  url_loader_factory_.reset();
  delegate_.reset();
  enabled_ = false;
  client_side_phishing_model_.reset();
}

void ClientSideDetectionService::OnPrefsUpdated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool enabled = IsSafeBrowsingEnabled(*delegate_->GetPrefs());
  bool extended_reporting =
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) ||
      IsExtendedReportingEnabled(*delegate_->GetPrefs());
  if (enabled == enabled_ && extended_reporting_ == extended_reporting) {
    return;
  }

  enabled_ = enabled;
  extended_reporting_ = extended_reporting;

  if (enabled_ && client_side_phishing_model_) {
    update_model_subscription_ = client_side_phishing_model_->RegisterCallback(
        base::BindRepeating(&ClientSideDetectionService::SendModelToRenderers,
                            weak_factory_.GetWeakPtr()));
    if (IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
      client_side_phishing_model_->SubscribeToImageEmbedderOptimizationGuide();
    }
  } else {
    // Invoke pending callbacks with a false verdict.
    for (auto& client_phishing_report : client_phishing_reports_) {
      ClientPhishingReportInfo* info = client_phishing_report.second.get();
      if (!info->callback.is_null()) {
        std::move(info->callback).Run(info->phishing_url, false, std::nullopt);
      }
    }
    client_phishing_reports_.clear();
    cache_.clear();
  }

  SendModelToRenderers();  // always refresh the renderer state
}

void ClientSideDetectionService::SendClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> verdict,
    ClientReportPhishingRequestCallback callback,
    const std::string& access_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientSideDetectionService::StartClientReportPhishingRequest,
          weak_factory_.GetWeakPtr(), std::move(verdict), std::move(callback),
          access_token));
}

bool ClientSideDetectionService::IsPrivateIPAddress(
    const net::IPAddress& address) const {
  return !address.IsPubliclyRoutable();
}

bool ClientSideDetectionService::IsLocalResource(
    const net::IPAddress& address) const {
  return !address.IsValid();
}

void ClientSideDetectionService::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    base::Time start_time,
    std::unique_ptr<std::string> response_body) {
  base::UmaHistogramTimes("SBClientPhishing.NetworkRequestDuration",
                          base::Time::Now() - start_time);

  std::string data;
  if (response_body) {
    data = std::move(*response_body.get());
  }
  std::optional<net::HttpStatusCode> response_code = std::nullopt;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = static_cast<net::HttpStatusCode>(
        url_loader->ResponseInfo()->headers->response_code());
  }
  if (response_code.has_value()) {
    RecordHttpResponseOrErrorCode("SBClientPhishing.NetworkResult",
                                  url_loader->NetError(),
                                  response_code.value());
  }

  DCHECK(base::Contains(client_phishing_reports_, url_loader));
  HandlePhishingVerdict(url_loader, url_loader->GetFinalURL(),
                        url_loader->NetError(), response_code, data);
}

void ClientSideDetectionService::SendModelToRenderers() {
  // We will not send models to the renderer process if the feature is disabled.
  // This is because the feature can be disabled via Finch in a scenario where a
  // bad model is uploaded to the server.
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    if (delegate_->ShouldSendModelToBrowserContext(
            it.GetCurrentValue()->GetBrowserContext())) {
      auto* rph = it.GetCurrentValue();
      if (rph->IsReady()) {
        SetPhishingModel(rph, /*new_renderer_process_host=*/false);
      } else {
        if (rph->IsInitializedAndNotDead() &&
            !observed_render_process_hosts_.IsObservingSource(rph)) {
          observed_render_process_hosts_.AddObservation(rph);
        }
      }
    }
  }
  if (client_side_phishing_model_) {
    trigger_model_version_ =
        client_side_phishing_model_->GetTriggerModelVersion();
  }
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> request,
    ClientReportPhishingRequestCallback callback,
    const std::string& access_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!enabled_) {
    if (!callback.is_null()) {
      std::move(callback).Run(GURL(request->url()), false, std::nullopt);
    }
    return;
  }

  // Record that we made a request. Logged before the request is made
  // to ensure it gets recorded. If this returns false due to being at ping cap
  // or prefs are null, abandon the request.
  if (!AddPhishingReport(base::Time::Now())) {
    if (!callback.is_null()) {
      std::move(callback).Run(GURL(request->url()), false, std::nullopt);
    }
    return;
  }

  std::string request_data;
  request->SerializeToString(&request_data);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "safe_browsing_client_side_phishing_detector", R"(
          semantics {
            sender: "Safe Browsing Client-Side Phishing Detector"
            description:
              "If the client-side phishing detector determines that the "
              "current page contents are similar to phishing pages, it will "
              "send a request to Safe Browsing to ask for a final verdict. If "
              "Safe Browsing agrees the page is dangerous, Chrome will show a "
              "full-page interstitial warning."
            trigger:
              "Whenever the clinet-side detector machine learning model "
              "computes a phishy-ness score above a threshold, after page-load."
            data:
              "Top-level page URL without CGI parameters, boolean and double "
              "features extracted from DOM, such as the number of resources "
              "loaded in the page, if certain likely phishing and social "
              "engineering terms found on the page, etc."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "Safe browsing cookie store"
            setting:
              "Users can enable or disable this feature by toggling 'Protect "
              "you and your device from dangerous sites' in Chrome settings "
              "under Privacy. This feature is enabled by default."
            chrome_policy {
              SafeBrowsingProtectionLevel {
                policy_options {mode: MANDATORY}
                SafeBrowsingProtectionLevel: 0
              }
            }
            chrome_policy {
              SafeBrowsingEnabled {
                policy_options {mode: MANDATORY}
                SafeBrowsingEnabled: false
              }
            }
            deprecated_policies: "SafeBrowsingEnabled"
          })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  if (!access_token.empty()) {
    LogAuthenticatedCookieResets(
        *resource_request,
        SafeBrowsingAuthenticatedEndpoint::kClientSideDetection);
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
  }

  resource_request->url = GetClientReportUrl(kClientReportPhishingUrl);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  loader->AttachStringForUpload(request_data, "application/octet-stream");
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ClientSideDetectionService::OnURLLoaderComplete,
                     base::Unretained(this), loader.get(), base::Time::Now()));

  // Remember which callback and URL correspond to the current fetcher object.
  std::unique_ptr<ClientPhishingReportInfo> info(new ClientPhishingReportInfo);
  auto* loader_ptr = loader.get();
  info->loader = std::move(loader);
  info->callback = std::move(callback);
  info->phishing_url = GURL(request->url());
  client_phishing_reports_[loader_ptr] = std::move(info);

  // The following is to log this ClientPhishingRequest on any open
  // chrome://safe-browsing pages. If no such page is open, the request is
  // dropped and the |request| object deleted.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIInfoSingleton::AddToClientPhishingRequestsSent,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::move(request), access_token));
}

void ClientSideDetectionService::HandlePhishingVerdict(
    network::SimpleURLLoader* source,
    const GURL& url,
    int net_error,
    std::optional<net::HttpStatusCode> response_code,
    const std::string& data) {
  ClientPhishingResponse response;
  std::unique_ptr<ClientPhishingReportInfo> info =
      std::move(client_phishing_reports_[source]);
  client_phishing_reports_.erase(source);

  bool is_phishing = false;
  if (net_error == net::OK && response_code.has_value() &&
      net::HTTP_OK == response_code.value() && response.ParseFromString(data)) {
    // Cache response, possibly flushing an old one.
    cache_[info->phishing_url] =
        base::WrapUnique(new CacheState(response.phishy(), base::Time::Now()));
    is_phishing = response.phishy();
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIInfoSingleton::AddToClientPhishingResponsesReceived,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::make_unique<ClientPhishingResponse>(response)));

  if (!info->callback.is_null()) {
    if (response_code.has_value() && response_code.value() == 0) {
      response_code = std::nullopt;
    }

    std::move(info->callback)
        .Run(info->phishing_url, is_phishing, response_code);
  }
}

bool ClientSideDetectionService::GetValidCachedResult(const GURL& url,
                                                      bool* is_phishing) {
  UpdateCache();

  auto it = cache_.find(url);
  if (it == cache_.end()) {
    return false;
  }

  // We still need to check if the result is valid.
  const CacheState& cache_state = *it->second;
  if (cache_state.is_phishing
          ? cache_state.timestamp >
                base::Time::Now() - base::Minutes(kPositiveCacheIntervalMinutes)
          : cache_state.timestamp >
                base::Time::Now() - base::Days(kNegativeCacheIntervalDays)) {
    *is_phishing = cache_state.is_phishing;
    return true;
  }
  return false;
}

void ClientSideDetectionService::UpdateCache() {
  // Since we limit the number of requests but allow pass-through for cache
  // refreshes, we don't want to remove elements from the cache if they
  // could be used for this purpose even if we will not use the entry to
  // satisfy the request from the cache.
  base::TimeDelta positive_cache_interval =
      std::max(base::Minutes(kPositiveCacheIntervalMinutes),
               base::Days(kReportsIntervalDays));
  base::TimeDelta negative_cache_interval = std::max(
      base::Days(kNegativeCacheIntervalDays), base::Days(kReportsIntervalDays));

  // Remove elements from the cache that will no longer be used.
  for (auto it = cache_.begin(); it != cache_.end();) {
    const CacheState& cache_state = *it->second;
    if (cache_state.is_phishing
            ? cache_state.timestamp >
                  base::Time::Now() - positive_cache_interval
            : cache_state.timestamp >
                  base::Time::Now() - negative_cache_interval) {
      ++it;
    } else {
      cache_.erase(it++);
    }
  }
}

bool ClientSideDetectionService::AtPhishingReportLimit() {
  // Clear the expired timestamps
  const auto cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);
  // Erase items older than cutoff because we will never care about them again.
  while (!phishing_report_times_.empty() &&
         phishing_report_times_.front() < cutoff) {
    phishing_report_times_.pop_front();
  }

  // `delegate_` and prefs can be null in unit tests.
  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit) &&
      (delegate_ && delegate_->GetPrefs()) &&
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
    return GetPhishingNumReports() >=
           kSafeBrowsingDailyPhishingReportsLimitESB.Get();
  }
  return GetPhishingNumReports() >= kMaxReportsPerInterval;
}

int ClientSideDetectionService::GetPhishingNumReports() {
  return phishing_report_times_.size();
}

bool ClientSideDetectionService::AddPhishingReport(base::Time timestamp) {
  // We should not be adding a report when we are at the limit when this
  // function calls, but in case it does, we want to track how far back the
  // last report was prior to the current report and exit the function early.
  // Each classification request is made on the tab level, which may not have
  // had |phishing_report_times_| updated because the service class, that's on
  // the profile level, was processing a different request. Therefore, we check
  // one last time before we log the request.
  if (AtPhishingReportLimit()) {
    base::UmaHistogramMediumTimes("SBClientPhishing.TimeSinceLastReportAtLimit",
                                  timestamp - phishing_report_times_.back());
    return false;
  }

  if (!delegate_ || !delegate_->GetPrefs()) {
    base::UmaHistogramBoolean("SBClientPhishing.AddPhishingReportSuccessful",
                              false);
    return false;
  }

  phishing_report_times_.push_back(timestamp);

  base::Value::List time_list;
  for (const base::Time& report_time : phishing_report_times_) {
    time_list.Append(base::Value(report_time.InSecondsFSinceUnixEpoch()));
  }
  delegate_->GetPrefs()->SetList(prefs::kSafeBrowsingCsdPingTimestamps,
                                 std::move(time_list));
  base::UmaHistogramBoolean("SBClientPhishing.AddPhishingReportSuccessful",
                            true);

  return true;
}

void ClientSideDetectionService::LoadPhishingReportTimesFromPrefs() {
  // delegate and prefs can be null in unit tests.
  if (!delegate_ || !delegate_->GetPrefs()) {
    return;
  }

  phishing_report_times_.clear();
  const auto cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);
  for (const base::Value& timestamp :
       delegate_->GetPrefs()->GetList(prefs::kSafeBrowsingCsdPingTimestamps)) {
    auto time = base::Time::FromSecondsSinceUnixEpoch(timestamp.GetDouble());
    if (time >= cutoff) {
      phishing_report_times_.push_back(time);
    }
  }
}

// static
GURL ClientSideDetectionService::GetClientReportUrl(
    const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    url = url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));
  }

  return url;
}

CSDModelType ClientSideDetectionService::GetModelType() {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetModelType()
             : CSDModelType::kNone;
}

base::ReadOnlySharedMemoryRegion
ClientSideDetectionService::GetModelSharedMemoryRegion() {
  return client_side_phishing_model_->GetModelSharedMemoryRegion();
}

const base::File& ClientSideDetectionService::GetVisualTfLiteModel() {
  return client_side_phishing_model_->GetVisualTfLiteModel();
}

const base::File& ClientSideDetectionService::GetImageEmbeddingModel() {
  // At launch, we will only deploy the Image Embedding Model through
  // OptimizationGuide
  return client_side_phishing_model_->GetImageEmbeddingModel();
}

bool ClientSideDetectionService::
    IsModelMetadataImageEmbeddingVersionMatching() {
  return client_side_phishing_model_ &&
         client_side_phishing_model_
             ->IsModelMetadataImageEmbeddingVersionMatching();
}

void ClientSideDetectionService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void ClientSideDetectionService::OnRenderProcessHostCreated(
    content::RenderProcessHost* rph) {
  if (delegate_->ShouldSendModelToBrowserContext(rph->GetBrowserContext())) {
    // The |rph| is ready, so the model can immediately be send.
    if (rph->IsReady()) {
      SetPhishingModel(rph, /*new_renderer_process_host=*/true);
    } else if (!observed_render_process_hosts_.IsObservingSource(rph)) {
      // Postpone sending the model until the |rph| is ready.
      observed_render_process_hosts_.AddObservation(rph);
    }
  }
}

void ClientSideDetectionService::RenderProcessHostDestroyed(
    content::RenderProcessHost* rph) {
  if (observed_render_process_hosts_.IsObservingSource(rph)) {
    observed_render_process_hosts_.RemoveObservation(rph);
  }
}

void ClientSideDetectionService::RenderProcessReady(
    content::RenderProcessHost* rph) {
  SetPhishingModel(rph, /*new_renderer_process_host=*/true);
  if (observed_render_process_hosts_.IsObservingSource(rph)) {
    observed_render_process_hosts_.RemoveObservation(rph);
  }
}

void ClientSideDetectionService::SetPhishingModel(
    content::RenderProcessHost* rph,
    bool new_renderer_process_host) {
  // We want to check if the trigger model has been sent. If we have received a
  // callback after sending the trigger models before and the models are now
  // unavailable, that means the OptimizationGuide server sent us a null model
  // to signal that a bad model is in disk.
  if (!IsModelAvailable() && !sent_trigger_models_) {
    return;
  }
  if (!rph->GetChannel()) {
    return;
  }

  mojo::AssociatedRemote<mojom::PhishingModelSetter> model_setter;
  rph->GetChannel()->GetRemoteAssociatedInterface(&model_setter);
  if (!IsModelAvailable() && sent_trigger_models_) {
    model_setter->ClearScorer();
    return;
  }

  switch (GetModelType()) {
    case CSDModelType::kNone:
      return;
    case CSDModelType::kFlatbuffer:
      if (delegate_ && delegate_->GetPrefs() &&
          IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
        // The check for image embedding model is important because the
        // OptimizationGuide server can send a null image embedding model to
        // signal there is a bad model in disk. If the image embedding model
        // isn't available because of this, the scorer will be created without
        // the image embedder model, temporarily halting the image embedding
        // process on the renderer.
        if (IsModelMetadataImageEmbeddingVersionMatching() &&
            HasImageEmbeddingModel()) {
          base::UmaHistogramBoolean(
              "SBClientPhishing.ImageEmbeddingModelVersionMatch", true);
          if (!new_renderer_process_host &&
              trigger_model_version_ ==
                  client_side_phishing_model_->GetTriggerModelVersion()) {
            // If the trigger model version remains the same in the same
            // renderer process host, we can just attach the complementary image
            // embedding model to the current scorer.
            model_setter->AttachImageEmbeddingModel(
                GetImageEmbeddingModel().Duplicate());
          } else {
            model_setter->SetImageEmbeddingAndPhishingFlatBufferModel(
                GetModelSharedMemoryRegion(),
                GetVisualTfLiteModel().Duplicate(),
                GetImageEmbeddingModel().Duplicate());
          }
        } else {
          base::UmaHistogramBoolean(
              "SBClientPhishing.ImageEmbeddingModelVersionMatch", false);
          model_setter->SetPhishingFlatBufferModel(
              GetModelSharedMemoryRegion(), GetVisualTfLiteModel().Duplicate());
        }
      } else {
        model_setter->SetPhishingFlatBufferModel(
            GetModelSharedMemoryRegion(), GetVisualTfLiteModel().Duplicate());
      }
      sent_trigger_models_ = true;
      return;
  }
}

const base::flat_map<std::string, TfLiteModelMetadata::Threshold>&
ClientSideDetectionService::GetVisualTfLiteModelThresholds() {
  return client_side_phishing_model_->GetVisualTfLiteModelThresholds();
}

void ClientSideDetectionService::ClassifyPhishingThroughThresholds(
    ClientPhishingRequest* verdict) {
  // This is added so that client_side_detection_host_unittest.cc can pass.
  // Outside of the test, this should never occur because the model should have
  // been available in order to receive the verdict in the first place.
  if (!IsModelAvailable()) {
    return;
  }

  const base::flat_map<std::string, TfLiteModelMetadata::Threshold>&
      label_to_thresholds_map = GetVisualTfLiteModelThresholds();

  if (static_cast<int>(verdict->tflite_model_scores().size()) >
      static_cast<int>(label_to_thresholds_map.size())) {
    // Model is misconfigured, so bail out.
    base::UmaHistogramEnumeration(
        "SBClientPhishing.ClassifyThresholdsResult",
        SBClientDetectionClassifyThresholdsResult::kModelSizeMismatch);
    VLOG(0) << "Model is misconfigured. Size is mismatched. Verdict scores "
               "size is "
            << static_cast<int>(verdict->tflite_model_scores().size())
            << " and model thresholds size is "
            << static_cast<int>(label_to_thresholds_map.size());
    verdict->set_is_phishing(false);
    verdict->set_is_tflite_match(false);
    return;
  }

  for (int i = 0; i < verdict->tflite_model_scores().size(); i++) {
    // Users can have older models that do not have the esb thresholds in their
    // fields, so ESB subscribed users will use the standard thresholds instead
    auto result = label_to_thresholds_map.find(
        verdict->tflite_model_scores().at(i).label());

    if (result == label_to_thresholds_map.end()) {
      // Model is misconfigured, so bail out.
      base::UmaHistogramEnumeration(
          "SBClientPhishing.ClassifyThresholdsResult",
          SBClientDetectionClassifyThresholdsResult::kModelLabelNotFound);
      VLOG(0) << "Model is misconfigured. Unable to match label string to "
                 "threshold map";
      verdict->set_is_phishing(false);
      verdict->set_is_tflite_match(false);
      return;
    }

    const TfLiteModelMetadata::Threshold& thresholds = result->second;

    if (base::FeatureList::IsEnabled(
            kSafeBrowsingPhishingClassificationESBThreshold) &&
        delegate_ && delegate_->GetPrefs() &&
        IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
      if (verdict->tflite_model_scores().at(i).value() >=
          thresholds.esb_threshold()) {
        verdict->set_is_phishing(true);
        verdict->set_is_tflite_match(true);
      }
    } else {
      if (verdict->tflite_model_scores().at(i).value() >=
          thresholds.threshold()) {
        verdict->set_is_phishing(true);
        verdict->set_is_tflite_match(true);
      }
    }
  }

  base::UmaHistogramEnumeration(
      "SBClientPhishing.ClassifyThresholdsResult",
      SBClientDetectionClassifyThresholdsResult::kSuccess);
}

base::WeakPtr<ClientSideDetectionService>
ClientSideDetectionService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool ClientSideDetectionService::IsModelAvailable() {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return false;
  }

  return client_side_phishing_model_ &&
         client_side_phishing_model_->IsEnabled();
}

int ClientSideDetectionService::GetTriggerModelVersion() {
  return trigger_model_version_;
}

bool ClientSideDetectionService::HasImageEmbeddingModel() {
  return client_side_phishing_model_ &&
         client_side_phishing_model_->HasImageEmbeddingModel();
}

bool ClientSideDetectionService::IsSubscribedToImageEmbeddingModelUpdates() {
  return client_side_phishing_model_ &&
         client_side_phishing_model_
             ->IsSubscribedToImageEmbeddingModelUpdates();
}

base::CallbackListSubscription
ClientSideDetectionService::RegisterCallbackForModelUpdates(
    base::RepeatingCallback<void()> callback) {
  return client_side_phishing_model_->RegisterCallback(callback);
}

// IN-TEST
void ClientSideDetectionService::SetModelAndVisualTfLiteForTesting(
    const base::FilePath& model,
    const base::FilePath& visual_tf_lite) {
  client_side_phishing_model_->SetModelAndVisualTfLiteForTesting(  // IN-TEST
      model, visual_tf_lite);
}

}  // namespace safe_browsing
