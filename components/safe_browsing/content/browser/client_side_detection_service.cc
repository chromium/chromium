// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

#include <algorithm>
#include <memory>

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
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model_optimization_guide.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
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
    optimization_guide::OptimizationGuideModelProvider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : delegate_(std::move(delegate)) {
  // delegate and prefs can be null in unit tests.
  if (!delegate_ || !delegate_->GetPrefs()) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide) &&
      opt_guide && background_task_runner) {
    client_side_phishing_model_optimization_guide_ =
        std::make_unique<ClientSidePhishingModelOptimizationGuide>(
            opt_guide, background_task_runner);
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
  // Do an initial check of the prefs.
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() {
  weak_factory_.InvalidateWeakPtrs();
}

void ClientSideDetectionService::Shutdown() {
  url_loader_factory_.reset();
  delegate_.reset();
  enabled_ = false;
  client_side_phishing_model_optimization_guide_.reset();
}

void ClientSideDetectionService::OnPrefsUpdated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool enabled = IsSafeBrowsingEnabled(*delegate_->GetPrefs());
  bool extended_reporting =
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) ||
      IsExtendedReportingEnabled(*delegate_->GetPrefs());
  if (enabled == enabled_ && extended_reporting_ == extended_reporting)
    return;

  enabled_ = enabled;
  extended_reporting_ = extended_reporting;

  if (enabled_) {
    if (!base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      update_model_subscription_ =
          ClientSidePhishingModel::GetInstance()->RegisterCallback(
              base::BindRepeating(
                  &ClientSideDetectionService::SendModelToRenderers,
                  base::Unretained(this)));
    } else {
      if (client_side_phishing_model_optimization_guide_) {
        update_model_subscription_ =
            client_side_phishing_model_optimization_guide_->RegisterCallback(
                base::BindRepeating(
                    &ClientSideDetectionService::SendModelToRenderers,
                    weak_factory_.GetWeakPtr()));
      }
    }
  } else {
    // Invoke pending callbacks with a false verdict.
    for (auto& client_phishing_report : client_phishing_reports_) {
      ClientPhishingReportInfo* info = client_phishing_report.second.get();
      if (!info->callback.is_null())
        std::move(info->callback).Run(info->phishing_url, false);
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
  if (response_body)
    data = std::move(*response_body.get());
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();
  RecordHttpResponseOrErrorCode("SBClientPhishing.NetworkResult",
                                url_loader->NetError(), response_code);

  DCHECK(base::Contains(client_phishing_reports_, url_loader));
  HandlePhishingVerdict(url_loader, url_loader->GetFinalURL(),
                        url_loader->NetError(), response_code, data);
}

void ClientSideDetectionService::SendModelToRenderers() {
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    SetPhishingModel(it.GetCurrentValue());
  }
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> request,
    ClientReportPhishingRequestCallback callback,
    const std::string& access_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!enabled_) {
    if (!callback.is_null())
      std::move(callback).Run(GURL(request->url()), false);
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
              SafeBrowsingEnabled {
                policy_options {mode: MANDATORY}
                SafeBrowsingEnabled: false
              }
            }
          })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  if (!access_token.empty()) {
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

  // Record that we made a request
  AddPhishingReport(base::Time::Now());

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
    int response_code,
    const std::string& data) {
  ClientPhishingResponse response;
  std::unique_ptr<ClientPhishingReportInfo> info =
      std::move(client_phishing_reports_[source]);
  client_phishing_reports_.erase(source);

  bool is_phishing = false;
  if (net_error == net::OK && net::HTTP_OK == response_code &&
      response.ParseFromString(data)) {
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

  if (!info->callback.is_null())
    std::move(info->callback).Run(info->phishing_url, is_phishing);
}

bool ClientSideDetectionService::IsInCache(const GURL& url) {
  UpdateCache();

  return cache_.find(url) != cache_.end();
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

bool ClientSideDetectionService::OverPhishingReportLimit() {
  return GetPhishingNumReports() > kMaxReportsPerInterval;
}

int ClientSideDetectionService::GetPhishingNumReports() {
  return phishing_report_times_.size();
}

void ClientSideDetectionService::AddPhishingReport(base::Time timestamp) {
  phishing_report_times_.push_back(timestamp);

  base::Time cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);

  // Erase items older than cutoff because we will never care about them again.
  while (!phishing_report_times_.empty() &&
         phishing_report_times_.front() < cutoff) {
    phishing_report_times_.pop_front();
  }

  if (!delegate_ || !delegate_->GetPrefs())
    return;

  base::Value::List time_list;
  for (const base::Time& report_time : phishing_report_times_)
    time_list.Append(base::Value(report_time.ToDoubleT()));
  delegate_->GetPrefs()->SetList(prefs::kSafeBrowsingCsdPingTimestamps,
                                 std::move(time_list));
}

void ClientSideDetectionService::LoadPhishingReportTimesFromPrefs() {
  if (!delegate_ || !delegate_->GetPrefs())
    return;

  phishing_report_times_.clear();
  for (const base::Value& timestamp :
       delegate_->GetPrefs()->GetList(prefs::kSafeBrowsingCsdPingTimestamps)) {
    phishing_report_times_.push_back(
        base::Time::FromDoubleT(timestamp.GetDouble()));
  }
}

// static
GURL ClientSideDetectionService::GetClientReportUrl(
    const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));

  return url;
}

const std::string& ClientSideDetectionService::GetModelStr() {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return client_side_phishing_model_optimization_guide_->GetModelStr();
  }

  return ClientSidePhishingModel::GetInstance()->GetModelStr();
}

CSDModelType ClientSideDetectionService::GetModelType() {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return static_cast<CSDModelType>(
        client_side_phishing_model_optimization_guide_->GetModelType());
  }

  return ClientSidePhishingModel::GetInstance()->GetModelType();
}

base::ReadOnlySharedMemoryRegion
ClientSideDetectionService::GetModelSharedMemoryRegion() {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return client_side_phishing_model_optimization_guide_
        ->GetModelSharedMemoryRegion();
  }

  return ClientSidePhishingModel::GetInstance()->GetModelSharedMemoryRegion();
}

const base::File& ClientSideDetectionService::GetVisualTfLiteModel() {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return client_side_phishing_model_optimization_guide_
        ->GetVisualTfLiteModel();
  }

  return ClientSidePhishingModel::GetInstance()->GetVisualTfLiteModel();
}

void ClientSideDetectionService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void ClientSideDetectionService::OnRenderProcessHostCreated(
    content::RenderProcessHost* rph) {
  SetPhishingModel(rph);
}

void ClientSideDetectionService::SetPhishingModel(
    content::RenderProcessHost* rph) {
  if (!rph->GetChannel())
    return;
  mojo::AssociatedRemote<mojom::PhishingModelSetter> model_setter;
  rph->GetChannel()->GetRemoteAssociatedInterface(&model_setter);
  switch (GetModelType()) {
    case CSDModelType::kNone:
      return;
    case CSDModelType::kProtobuf:
      model_setter->SetPhishingModel(GetModelStr(),
                                     GetVisualTfLiteModel().Duplicate());
      return;
    case CSDModelType::kFlatbuffer:
      model_setter->SetPhishingFlatBufferModel(
          GetModelSharedMemoryRegion(), GetVisualTfLiteModel().Duplicate());
      return;
  }
}

base::WeakPtr<ClientSideDetectionService>
ClientSideDetectionService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool ClientSideDetectionService::IsModelAvailable() {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return client_side_phishing_model_optimization_guide_ &&
           client_side_phishing_model_optimization_guide_->IsEnabled();
  } else {
    return ClientSidePhishingModel::GetInstance()->IsEnabled();
  }
}

// IN-TEST
void ClientSideDetectionService::SetModelAndVisualTfLiteForTesting(
    const base::FilePath& model,
    const base::FilePath& visual_tf_lite) {
  client_side_phishing_model_optimization_guide_
      ->SetModelAndVisualTfLiteForTesting(  // IN-TEST
          model, visual_tf_lite);
}

}  // namespace safe_browsing
