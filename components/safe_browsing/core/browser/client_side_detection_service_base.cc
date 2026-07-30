// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"

#include <algorithm>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_embedder.h"
#include "url/gurl.h"

namespace safe_browsing {

const int ClientSideDetectionServiceBase::kReportsIntervalDays = 1;
const int ClientSideDetectionServiceBase::kMaxReportsPerInterval = 3;
const int ClientSideDetectionServiceBase::kNegativeCacheIntervalDays = 1;
const int ClientSideDetectionServiceBase::kPositiveCacheIntervalMinutes = 30;
const char ClientSideDetectionServiceBase::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";

ClientSideDetectionServiceBase::CacheState::CacheState(bool phish,
                                                       base::Time time)
    : is_phishing(phish), timestamp(time) {}

ClientSideDetectionServiceBase::ClientSideDetectionServiceBase(
    std::unique_ptr<Delegate> delegate,
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : ClientSideDetectionServiceBase(delegate ? delegate->GetPrefs() : nullptr,
                                     opt_guide) {
  delegate_ = std::move(delegate);
  if (delegate_) {
    url_loader_factory_ = delegate_->GetSafeBrowsingURLLoaderFactory();
  }
}

ClientSideDetectionServiceBase::ClientSideDetectionServiceBase(
    PrefService* prefs,
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : prefs_(prefs) {
  if (!base::FeatureList::IsEnabled(kClientSideDetectionKillswitch) &&
      opt_guide) {
    client_side_phishing_model_ = std::make_unique<ClientSidePhishingModel>(
        opt_guide, base::SequencedTaskRunner::GetCurrentDefault());
  }
  if (prefs_) {
    pref_change_registrar_.Init(prefs_);
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnabled,
        base::BindRepeating(&ClientSideDetectionServiceBase::OnPrefsUpdated,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnhanced,
        base::BindRepeating(&ClientSideDetectionServiceBase::OnPrefsUpdated,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingScoutReportingEnabled,
        base::BindRepeating(&ClientSideDetectionServiceBase::OnPrefsUpdated,
                            base::Unretained(this)));
    // Load the report times from preferences.
    LoadPhishingReportTimesFromPrefs();
  }
}

// static
std::unique_ptr<ClientSideDetectionServiceBase>
ClientSideDetectionServiceBase::CreateForTesting(  // IN-TEST
    PrefService* prefs,
    optimization_guide::OptimizationGuideModelProvider* opt_guide) {
  auto service =
      base::WrapUnique(new ClientSideDetectionServiceBase(prefs, opt_guide));
  // The base class is being constructed on its own, so we need to call
  // `OnPrefsUpdated()` to ensure its internal state is properly initialized
  // rather than relying on a derived class's constructor to handle it.
  service->OnPrefsUpdated();
  return service;
}

ClientSideDetectionServiceBase::~ClientSideDetectionServiceBase() = default;

void ClientSideDetectionServiceBase::Shutdown() {
  url_loader_factory_.reset();
  delegate_.reset();
  SetEnabled(false);
  client_side_phishing_model_.reset();
}

const std::vector<TfLiteModelMetadata::Threshold>&
ClientSideDetectionServiceBase::GetVisualTfLiteModelThresholds() const {
  if (client_side_phishing_model_) {
    return client_side_phishing_model_->GetVisualTfLiteModelThresholds();
  }
  static const base::NoDestructor<std::vector<TfLiteModelMetadata::Threshold>>
      empty_thresholds;
  return *empty_thresholds;
}

const std::vector<TargetEmbedding>&
ClientSideDetectionServiceBase::GetTargetImageEmbeddings() const {
  if (client_side_phishing_model_) {
    return client_side_phishing_model_->GetTargetImageEmbeddings();
  }
  static const base::NoDestructor<std::vector<TargetEmbedding>>
      empty_embeddings;
  return *empty_embeddings;
}

bool ClientSideDetectionServiceBase::IsEnabled() const {
  return enabled_;
}

void ClientSideDetectionServiceBase::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_) {
    ClearCache();
  }
}

void ClientSideDetectionServiceBase::OnPrefsUpdated() {
  // `prefs_` should only ever be null in tests.
  if (!prefs_) {
    return;
  }

  bool enabled = IsSafeBrowsingEnabled(*prefs_);
  bool extended_reporting = IsEnhancedProtectionEnabled(*prefs_) ||
                            IsExtendedReportingEnabled(*prefs_);
  if (enabled == IsEnabled() && extended_reporting_ == extended_reporting) {
    return;
  }

  SetEnabled(enabled);
  extended_reporting_ = extended_reporting;

  if (IsEnabled() && client_side_phishing_model_) {
    if (IsEnhancedProtectionEnabled(*prefs_)) {
      client_side_phishing_model_->SubscribeToImageEmbedderOptimizationGuide();
      if (base::FeatureList::IsEnabled(
              kClientSideDetectionOnlyESBClassification)) {
        client_side_phishing_model_
            ->SubscribeToImageClassifierOptimizationGuide();
      }
    } else {
      UnsubscribeToModelSubscription();
    }
  } else {
    // Invoke pending callbacks with a verdict of false.
    for (auto& client_phishing_report : client_phishing_reports_) {
      ClientPhishingReportInfo* info = client_phishing_report.second.get();
      if (!info->callback.is_null()) {
        std::move(info->callback)
            .Run(info->phishing_url, false, std::nullopt, std::nullopt);
      }
    }
    client_phishing_reports_.clear();
    ClearCache();
    UnsubscribeToModelSubscription();
  }

  OnModelAndServiceStateChanged();
}

void ClientSideDetectionServiceBase::OnModelAndServiceStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsEnabled()) {
    if (!update_model_subscription_) {
      update_model_subscription_ = RegisterCallbackForModelUpdates(
          base::BindRepeating(&ClientSideDetectionServiceBase::OnModelUpdated,
                              weak_factory_.GetWeakPtr()));
    }
  } else {
    update_model_subscription_ = base::CallbackListSubscription();
  }
  OnModelUpdated();
}

void ClientSideDetectionServiceBase::UnsubscribeToModelSubscription() {
  // We will check for the model object below because we also call this function
  // when the model object is not available.
  if (client_side_phishing_model_) {
    client_side_phishing_model_->UnsubscribeToImageEmbedderOptimizationGuide();
    if (base::FeatureList::IsEnabled(
            kClientSideDetectionOnlyESBClassification)) {
      client_side_phishing_model_
          ->UnsubscribeToImageClassifierOptimizationGuide();
    }
  }
}

bool ClientSideDetectionServiceBase::IsPrivateIPAddress(
    const net::IPAddress& address) const {
  return !address.IsPubliclyRoutable();
}

void ClientSideDetectionServiceBase::UpdateCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

bool ClientSideDetectionServiceBase::GetValidCachedResult(const GURL& url,
                                                          bool* is_phishing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

bool ClientSideDetectionServiceBase::AtPhishingReportLimit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clear the expired timestamps
  const auto cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);
  // Erase items older than cutoff because we will never care about them again.
  while (!phishing_report_times_.empty() &&
         phishing_report_times_.front() < cutoff) {
    phishing_report_times_.pop_front();
  }

  // prefs_ can be null in unit tests.
  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit) &&
      prefs_ && IsEnhancedProtectionEnabled(*prefs_)) {
    return GetPhishingNumReports() >=
           kSafeBrowsingDailyPhishingReportsLimitESB.Get();
  }

  return GetPhishingNumReports() >= kMaxReportsPerInterval;
}

int ClientSideDetectionServiceBase::GetPhishingNumReports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return phishing_report_times_.size();
}

bool ClientSideDetectionServiceBase::AddPhishingReport(base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  if (!prefs_) {
    base::UmaHistogramBoolean("SBClientPhishing.AddPhishingReportSuccessful",
                              false);
    return false;
  }

  phishing_report_times_.push_back(timestamp);

  base::ListValue time_list;
  for (const base::Time& report_time : phishing_report_times_) {
    time_list.Append(base::Value(report_time.InSecondsFSinceUnixEpoch()));
  }
  prefs_->SetList(prefs::kSafeBrowsingCsdPingTimestamps, std::move(time_list));
  base::UmaHistogramBoolean("SBClientPhishing.AddPhishingReportSuccessful",
                            true);

  return true;
}

void ClientSideDetectionServiceBase::AddCacheEntry(const GURL& url,
                                                   bool is_phishing,
                                                   base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_[url] = std::make_unique<CacheState>(is_phishing, timestamp);
}

void ClientSideDetectionServiceBase::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_.clear();
}

void ClientSideDetectionServiceBase::LoadPhishingReportTimesFromPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // prefs_ can be null in unit tests.
  if (!prefs_) {
    return;
  }

  phishing_report_times_.clear();
  const auto cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);
  for (const base::Value& timestamp :
       prefs_->GetList(prefs::kSafeBrowsingCsdPingTimestamps)) {
    auto time = base::Time::FromSecondsSinceUnixEpoch(timestamp.GetDouble());
    if (time >= cutoff) {
      phishing_report_times_.push_back(time);
    }
  }
}

void ClientSideDetectionServiceBase::ClassifyPhishingThroughThresholds(
    ClientPhishingRequest* verdict) {
  // This is added so that client_side_detection_host_unittest.cc can pass.
  // Outside of the test, this should never occur because the model should have
  // been available in order to receive the verdict in the first place.
  if (!IsModelAvailable()) {
    return;
  }

  const std::vector<TfLiteModelMetadata::Threshold>& thresholds =
      client_side_phishing_model_->GetVisualTfLiteModelThresholds();

  if (static_cast<int>(verdict->tflite_model_scores().size()) !=
      static_cast<int>(thresholds.size())) {
    // Model is misconfigured, so bail out.
    base::UmaHistogramEnumeration(
        "SBClientPhishing.ClassifyThresholdsResult",
        SBClientDetectionClassifyThresholdsResult::kModelSizeMismatch);
    VLOG(0) << "Model is misconfigured. Size is mismatched. Verdict scores "
               "size is "
            << static_cast<int>(verdict->tflite_model_scores().size())
            << " and model thresholds size is "
            << static_cast<int>(thresholds.size());
    verdict->set_is_phishing(false);
    verdict->set_is_tflite_match(false);
    return;
  }

  for (int i = 0; i < verdict->tflite_model_scores().size(); i++) {
    const TfLiteModelMetadata::Threshold& threshold = thresholds.at(i);

    ClientPhishingRequest::CategoryScore* category =
        verdict->mutable_tflite_model_scores(i);

    category->set_label(threshold.label());

    if (prefs_ && IsEnhancedProtectionEnabled(*prefs_)) {
      if (category->value() >= threshold.esb_threshold()) {
        verdict->set_is_phishing(true);
        verdict->set_is_tflite_match(true);
      }
    } else {
      if (category->value() >= threshold.threshold()) {
        verdict->set_is_phishing(true);
        verdict->set_is_tflite_match(true);
      }
    }
  }

  verdict->set_tflite_model_version(
      client_side_phishing_model_->GetTriggerModelVersion());

  if (!verdict->is_phishing() && verdict->has_image_feature_embedding()) {
    ClassifyThroughEmbeddings(verdict);
  }

  base::UmaHistogramEnumeration(
      "SBClientPhishing.ClassifyThresholdsResult",
      SBClientDetectionClassifyThresholdsResult::kSuccess);
}

void ClientSideDetectionServiceBase::ClassifyThroughEmbeddings(
    ClientPhishingRequest* verdict) {
  auto target_image_embeddings =
      client_side_phishing_model_->GetTargetImageEmbeddings();
  if (target_image_embeddings.empty() ||
      !verdict->has_image_feature_embedding()) {
    return;
  }

  verdict->mutable_image_feature_embedding()->set_embedding_model_version(
      client_side_phishing_model_->GetImageEmbeddingModelVersion());

  // Create a FeatureVector from the ImageFeatureEmbedding.
  tflite::task::vision::FeatureVector feature_vector;
  for (float image_embedding_value :
       verdict->image_feature_embedding().embedding_value()) {
    feature_vector.add_value_float(image_embedding_value);
  }

  // Compare newly made FeatureVector to target image embeddings.
  for (const TargetEmbedding& target_image_embedding :
       target_image_embeddings) {
    tflite::support::StatusOr<double> similarity =
        tflite::task::vision::ImageEmbedder::CosineSimilarity(
            target_image_embedding.embedding, feature_vector);
    if (similarity.ok() &&
        similarity.value() >= target_image_embedding.threshold) {
      verdict->set_is_phishing(true);
      ClientPhishingRequest::EmbeddingMatchMetadata embedding_match_metadata;
      const auto& value_floats = target_image_embedding.embedding.value_float();
      embedding_match_metadata.set_id(
          ClientSidePhishingModel::GetHashFromEmbedding(
              std::vector<float>(value_floats.begin(), value_floats.end())));
      embedding_match_metadata.set_score(similarity.value());
      *verdict->mutable_target_image_embedding_score() =
          embedding_match_metadata;
      break;
    }
  }
}

// static
GURL ClientSideDetectionServiceBase::GetClientReportUrl(
    const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    url = url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));
  }

  return url;
}

void ClientSideDetectionServiceBase::SendClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> verdict,
    ClientReportPhishingRequestCallback callback,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), GURL(verdict->url()),
                                    false, std::nullopt, std::nullopt));
    }
    return;
  }

  // If the report was not requested by the user, record that we made a request.
  // Logged before the request is made to ensure it gets recorded. If this
  // returns false due to being at ping cap or prefs are null, abandon the
  // request.
  if (verdict->client_side_detection_type() !=
          ClientSideDetectionType::USER_REPORT &&
      !AddPhishingReport(base::Time::Now())) {
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), GURL(verdict->url()),
                                    false, std::nullopt, std::nullopt));
    }
    return;
  }

  std::string request_data;
  verdict->SerializeToString(&request_data);

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
              "Whenever the client-side detector machine learning model "
              "computes a phishy-ness score above a threshold, after page-load."
            internal {
              contacts {
                email: "chrome-counter-abuse-alerts@google.com"
              }
            }
            user_data {
              type: ACCESS_TOKEN
              type: SENSITIVE_URL
              type: WEB_CONTENT
            }
            data:
              "Top-level page URL without CGI parameters, boolean and double "
              "features extracted from DOM, such as the number of resources "
              "loaded in the page, if certain likely phishing and social "
              "engineering terms found on the page, etc."
            destination: GOOGLE_OWNED_SERVICE
            last_reviewed: "2026-06-17"
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
    SetAccessToken(resource_request.get(), access_token);
  }

  resource_request->url = GetClientReportUrl(kClientReportPhishingUrl);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  loader->AttachStringForUpload(request_data, "application/octet-stream");
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ClientSideDetectionServiceBase::OnURLLoaderComplete,
                     base::Unretained(this), loader.get(), base::Time::Now()));

  // Remember which callback and URL correspond to the current fetcher object.
  auto info = std::make_unique<ClientPhishingReportInfo>();
  auto* loader_ptr = loader.get();
  info->loader = std::move(loader);
  info->callback = std::move(callback);
  info->phishing_url = GURL(verdict->url());
  client_phishing_reports_[loader_ptr] = std::move(info);

  DidSendClientReportPhishingRequest(std::move(verdict), access_token);
}

CSDModelType ClientSideDetectionServiceBase::GetModelType() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetModelType()
             : CSDModelType::kNone;
}

void ClientSideDetectionServiceBase::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    base::Time start_time,
    std::optional<std::string> response_body) {
  base::UmaHistogramTimes("SBClientPhishing.NetworkRequestDuration",
                          base::Time::Now() - start_time);

  std::optional<net::HttpStatusCode> response_code = std::nullopt;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = static_cast<net::HttpStatusCode>(
        url_loader->ResponseInfo()->headers->response_code());
  }
  RecordHttpResponseOrErrorCode(
      "SBClientPhishing.NetworkResult2", url_loader->NetError(),
      response_code.has_value() ? response_code.value() : 0);

  DCHECK(client_phishing_reports_.contains(url_loader));
  HandlePhishingVerdict(url_loader, url_loader->GetFinalURL(),
                        url_loader->NetError(), response_code,
                        std::move(response_body).value_or(""));
}

void ClientSideDetectionServiceBase::HandlePhishingVerdict(
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
  std::optional<IntelligentScanVerdict> intelligent_scan_verdict = std::nullopt;
  if (net_error == net::OK && response_code.has_value() &&
      net::HTTP_OK == response_code.value() && response.ParseFromString(data)) {
    // Cache response, possibly flushing an old one.
    AddCacheEntry(info->phishing_url, response.phishy(), base::Time::Now());
    is_phishing = response.phishy();
    if (response.has_intelligent_scan_verdict()) {
      intelligent_scan_verdict = response.intelligent_scan_verdict();
    }
  }

  DidReceiveClientPhishingResponse(response);

  if (!info->callback.is_null()) {
    if (response_code.has_value() && response_code.value() == 0) {
      response_code = std::nullopt;
    }

    std::move(info->callback)
        .Run(info->phishing_url, is_phishing, response_code,
             intelligent_scan_verdict);
  }
}

bool ClientSideDetectionServiceBase::IsModelAvailable() const {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return false;
  }
  return client_side_phishing_model_ &&
         client_side_phishing_model_->IsEnabled();
}

bool ClientSideDetectionServiceBase::HasImageEmbeddingModel() const {
  return client_side_phishing_model_ &&
         client_side_phishing_model_->HasImageEmbeddingModel();
}

bool ClientSideDetectionServiceBase::
    IsModelMetadataImageEmbeddingVersionMatching() const {
  return client_side_phishing_model_ &&
         client_side_phishing_model_
             ->IsModelMetadataImageEmbeddingVersionMatching();
}

int ClientSideDetectionServiceBase::GetTriggerModelVersion() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetTriggerModelVersion()
             : 0;
}

int ClientSideDetectionServiceBase::GetImageEmbeddingModelVersion() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetImageEmbeddingModelVersion()
             : 0;
}

bool ClientSideDetectionServiceBase::IsSubscribedToImageEmbeddingModelUpdates()
    const {
  return client_side_phishing_model_ &&
         client_side_phishing_model_
             ->IsSubscribedToImageEmbeddingModelUpdates();
}

bool ClientSideDetectionServiceBase::IsSubscribedToImageClassifierModelUpdates()
    const {
  return client_side_phishing_model_ &&
         client_side_phishing_model_
             ->IsSubscribedToImageClassifierModelUpdates();
}

void ClientSideDetectionServiceBase::SetModelAndVisualTfLiteForTesting(
    const base::FilePath& model,
    const base::FilePath& visual_tf_lite) {
  if (client_side_phishing_model_) {
    client_side_phishing_model_->SetModelAndVisualTfLiteForTesting(  // IN-TEST
        model, visual_tf_lite);
  }
}

void ClientSideDetectionServiceBase::SetTargetImageEmbeddingsForTesting(
    std::vector<TargetEmbedding> target_embeddings) {
  if (client_side_phishing_model_) {
    client_side_phishing_model_->SetTargetImageEmbeddingsForTesting(  // IN-TEST
        std::move(target_embeddings));
  }
}

const base::File& ClientSideDetectionServiceBase::GetVisualTfLiteModel() const {
  if (client_side_phishing_model_) {
    return client_side_phishing_model_->GetVisualTfLiteModel();
  }
  static const base::NoDestructor<base::File> empty_file;
  return *empty_file;
}

const base::File& ClientSideDetectionServiceBase::GetImageEmbeddingModel()
    const {
  if (client_side_phishing_model_) {
    return client_side_phishing_model_->GetImageEmbeddingModel();
  }
  static const base::NoDestructor<base::File> empty_file;
  return *empty_file;
}

int ClientSideDetectionServiceBase::GetClassificationInputWidth() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetClassificationInputWidth()
             : 0;
}

int ClientSideDetectionServiceBase::GetClassificationInputHeight() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetClassificationInputHeight()
             : 0;
}

int ClientSideDetectionServiceBase::GetImageEmbeddingInputWidth() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetImageEmbeddingInputWidth()
             : 0;
}

int ClientSideDetectionServiceBase::GetImageEmbeddingInputHeight() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetImageEmbeddingInputHeight()
             : 0;
}

base::ReadOnlySharedMemoryRegion
ClientSideDetectionServiceBase::GetModelSharedMemoryRegion() const {
  return client_side_phishing_model_
             ? client_side_phishing_model_->GetModelSharedMemoryRegion()
             : base::ReadOnlySharedMemoryRegion();
}

base::CallbackListSubscription
ClientSideDetectionServiceBase::RegisterCallbackForModelUpdates(
    base::RepeatingClosure callback) {
  return client_side_phishing_model_
             ? client_side_phishing_model_->RegisterCallback(callback)
             : base::CallbackListSubscription();
}

}  // namespace safe_browsing
