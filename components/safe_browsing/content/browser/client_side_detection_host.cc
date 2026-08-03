// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/content_unsafe_resource_util.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/credit_card_form_event.h"
#include "components/safe_browsing/core/browser/db/allowlist_checker_client.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/security_interstitials/core/unsafe_resource_locator.h"
#include "components/url_formatter/url_fixer.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/ip_endpoint.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/view_android.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace safe_browsing {

namespace {

// Probability value used to sample pings on CSD allowlist match. For other safe
// browsing countermeasures, we sample at 1 in 100 rate, but in this, we hit the
// allowlist 1000 times more than the rate at which we send a ping due to local
// model verdict. Therefore, we sample at 1 in 100,000 rate instead.
const float kProbabilityForSendingSampleRequest = 0.000001;
// Probability value used to accept the high confidence allowlist match for
// trigger and force request types. More information on why this value was
// chosen can be found at go/crca-cspp-expand-allowlist.
const float kProbabilityForAcceptingHCAllowlistTrigger = 0.9999;
// How long to wait to run the user report callback.
const int kUserReportCallbackTimer = 30;

bool HasDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kCsdDebugFeatureDirectoryFlag);
}

bool ShouldSkipCSDAllowlist() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSkipCSDAllowlistOnPreclassification);
}

std::string_view GetRequestTypeName(
    ClientSideDetectionType client_side_detection_type) {
  switch (client_side_detection_type) {
    case safe_browsing::ClientSideDetectionType::
        CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED:
      return "Unknown";
    case safe_browsing::ClientSideDetectionType::FORCE_REQUEST:
      return "ForceRequest";
    case safe_browsing::ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT:
      return "NotificationPermissionPrompt";
    case safe_browsing::ClientSideDetectionType::TRIGGER_MODELS:
      return "TriggerModel";
    case safe_browsing::ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED:
      return "KeyboardLockRequested";
    case safe_browsing::ClientSideDetectionType::POINTER_LOCK_REQUESTED:
      return "PointerLockRequested";
    case safe_browsing::ClientSideDetectionType::VIBRATION_API:
      return "VibrationApi";
    case safe_browsing::ClientSideDetectionType::FULLSCREEN_API:
      return "FullscreenApi";
    case safe_browsing::ClientSideDetectionType::CLIPBOARD_COPY_API:
      return "ClipboardCopyApi";
    case safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM:
      return "CreditCardForm";
    case safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH:
      return "ImageEmbeddingMatch";
    case safe_browsing::ClientSideDetectionType::USER_REPORT:
      return "UserReport";
    case safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE:
      return "UnfamiliarLoginPage";
  }
}

safe_browsing::mojom::ClientSideDetectionType GetClientSideDetectionMojomType(
    ClientSideDetectionType client_side_detection_type) {
  switch (client_side_detection_type) {
    case safe_browsing::ClientSideDetectionType::FORCE_REQUEST:
      return safe_browsing::mojom::ClientSideDetectionType::kForceRequest;
    case safe_browsing::ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT:
      return safe_browsing::mojom::ClientSideDetectionType::
          kNotificationPermissionPrompt;
    case safe_browsing::ClientSideDetectionType::TRIGGER_MODELS:
      return safe_browsing::mojom::ClientSideDetectionType::kTriggerModels;
    case safe_browsing::ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED:
      return safe_browsing::mojom::ClientSideDetectionType::kKeyboardLock;
    case safe_browsing::ClientSideDetectionType::POINTER_LOCK_REQUESTED:
      return safe_browsing::mojom::ClientSideDetectionType::kPointerLock;
    case safe_browsing::ClientSideDetectionType::VIBRATION_API:
      return safe_browsing::mojom::ClientSideDetectionType::kVibrationApi;
    case safe_browsing::ClientSideDetectionType::FULLSCREEN_API:
      return safe_browsing::mojom::ClientSideDetectionType::kFullscreen;
    case safe_browsing::ClientSideDetectionType::CLIPBOARD_COPY_API:
      return safe_browsing::mojom::ClientSideDetectionType::kClipboardCopyApi;
    case safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM:
      return safe_browsing::mojom::ClientSideDetectionType::kCreditCardForm;
    case safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH:
      return safe_browsing::mojom::ClientSideDetectionType::
          kImageEmbeddingMatch;
    case safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE:
      return safe_browsing::mojom::ClientSideDetectionType::
          kUnfamiliarLoginPage;
    case safe_browsing::ClientSideDetectionType::
        CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED:
    case safe_browsing::ClientSideDetectionType::USER_REPORT:
      return safe_browsing::mojom::ClientSideDetectionType::kUserReport;
    default:
      NOTREACHED();
  }
}

PhishingDetectorResult GetPhishingDetectorResult(
    mojom::PhishingDetectorResult result) {
  switch (result) {
    case mojom::PhishingDetectorResult::SUCCESS:
      return PhishingDetectorResult::CLASSIFICATION_SUCCESS;
    case mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY:
      return PhishingDetectorResult::CLASSIFIER_NOT_READY;
    case mojom::PhishingDetectorResult::CANCELLED:
      return PhishingDetectorResult::CLASSIFICATION_CANCELLED;
    case mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION:
      return PhishingDetectorResult::FORWARD_BACK_TRANSITION;
    case mojom::PhishingDetectorResult::INVALID_SCORE:
      return PhishingDetectorResult::INVALID_SCORE;
    case mojom::PhishingDetectorResult::INVALID_URL_FORMAT_REQUEST:
      return PhishingDetectorResult::INVALID_URL_FORMAT_REQUEST;
    case mojom::PhishingDetectorResult::INVALID_DOCUMENT_LOADER:
      return PhishingDetectorResult::INVALID_DOCUMENT_LOADER;
    case mojom::PhishingDetectorResult::URL_FEATURE_EXTRACTION_FAILED:
      return PhishingDetectorResult::URL_FEATURE_EXTRACTION_FAILED;
    case mojom::PhishingDetectorResult::DOM_EXTRACTION_FAILED:
      return PhishingDetectorResult::DOM_EXTRACTION_FAILED;
    case mojom::PhishingDetectorResult::TERM_EXTRACTION_FAILED:
      return PhishingDetectorResult::TERM_EXTRACTION_FAILED;
    case mojom::PhishingDetectorResult::VISUAL_EXTRACTION_FAILED:
      return PhishingDetectorResult::VISUAL_EXTRACTION_FAILED;
    case mojom::PhishingDetectorResult::CLASSIFICATION_SKIPPED:
      return PhishingDetectorResult::CLASSIFICATION_SKIPPED;
  }
}

void RecordAsyncCheckTriggerForceRequestResult(
    ClientSideDetectionHost::AsyncCheckTriggerForceRequestResult result) {
  base::UmaHistogramEnumeration(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      result);
}


safe_browsing::ThreatSubtype GetThreatSubtype(
    IntelligentScanVerdict intelligent_scan_verdict) {
  switch (intelligent_scan_verdict) {
    case IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1:
      return safe_browsing::ThreatSubtype::SCAM_EXPERIMENT_VERDICT_1;
    case IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_2:
      return safe_browsing::ThreatSubtype::SCAM_EXPERIMENT_VERDICT_2;
    case IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_3:
      return safe_browsing::ThreatSubtype::SCAM_EXPERIMENT_VERDICT_3;
    case IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_4:
      return safe_browsing::ThreatSubtype::SCAM_EXPERIMENT_VERDICT_4;
    case IntelligentScanVerdict::SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT:
      return safe_browsing::ThreatSubtype::
          SCAM_EXPERIMENT_CATCH_ALL_ENFORCEMENT;
    default:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace

typedef base::OnceCallback<void(bool, bool, std::optional<bool>, bool)>
    ShouldClassifyUrlCallback;

// This class is instantiated each time a new toplevel URL loads, and
// asynchronously checks whether the phishing classifier should run
// for this URL.  If so, it notifies the host class by calling the provided
// callback from the UI thread.  Objects of this class  will be destroyed once
// nobody uses it anymore.  If |web_contents|, |csd_service| or |host| go away
// you need to call Cancel().  We keep the |database_manager| alive in a ref
// pointer for as long as it takes.
class ClientSideDetectionHost::ShouldClassifyUrlRequest {
 public:
  ShouldClassifyUrlRequest(
      const GURL& url,
      const network::mojom::URLResponseHead* response_head,
      ShouldClassifyUrlCallback start_phishing_classification,
      WebContents* web_contents,
      base::WeakPtr<ClientSideDetectionServiceBase> csd_service,
      SafeBrowsingDatabaseManager* database_manager,
      ClientSideDetectionType phishing_detection_request_type,
      float probability_for_accepting_hc_allowlist_trigger,
      base::WeakPtr<ClientSideDetectionHost> host)
      : url_(url),
        web_contents_(web_contents),
        csd_service_(csd_service),
        database_manager_(database_manager),
        phishing_detection_request_type_(phishing_detection_request_type),
        probability_for_accepting_hc_allowlist_trigger_(
            probability_for_accepting_hc_allowlist_trigger),
        host_(host),
        start_phishing_classification_cb_(
            std::move(start_phishing_classification)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(web_contents_);
    DCHECK(csd_service_);
    DCHECK(database_manager_.get());
    DCHECK(host_);
    if (response_head) {
      if (response_head->headers) {
        response_head->headers->GetMimeType(&mime_type_);
      }
      remote_endpoint_ = response_head->remote_endpoint;
    }
  }

  ShouldClassifyUrlRequest(const ShouldClassifyUrlRequest&) = delete;
  ShouldClassifyUrlRequest& operator=(const ShouldClassifyUrlRequest&) = delete;

  // The destructor can be called either from the UI or the IO thread.
  ~ShouldClassifyUrlRequest() = default;

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // We start by doing some simple checks that can run on the UI thread.
    if (url_.SchemeIs(content::kChromeUIScheme)) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_CHROME_UI_PAGE);
    }

    if (base::FeatureList::IsEnabled(
            kClientSideDetectionLocalResourceCheckFix)) {
      // safe_browsing::CanGetReputationOfUrl() is another option to be
      // comprehensive, but since IsPrivateIPAddress and SchemeIsHTTPOrHTTPS
      // are checked below, using net::IsLocalhost() is sufficient.
      // TODO: Consider safe_browsing::CanGetReputationOfUrl() in the future to
      // have a consolidated preclassification check result.
      if (url_.SchemeIsFile() || net::IsLocalhost(url_)) {
        DontClassifyForPhishing(
            PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE);
      }
    } else {
      if (csd_service_ && !remote_endpoint_.address().IsValid()) {
        DontClassifyForPhishing(
            PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE);
      }
    }

    bool is_mime_type_unsupported =
        mime_type_ != "text/html" && mime_type_ != "application/xhtml+xml";
    content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
    bool is_error_page = rfh && rfh->IsErrorDocument();
    if (!is_mime_type_unsupported) {
      base::UmaHistogramBoolean(
          "SBClientPhishing.IsErrorDocumentOnSupportedMimeType", is_error_page);
    }

    if (base::FeatureList::IsEnabled(kClientSideDetectionSkipErrorPage) &&
        is_error_page) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_ERROR_DOCUMENT);
    }

    if (is_mime_type_unsupported) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_UNSUPPORTED_MIME_TYPE);
    }

    if (csd_service_ &&
        csd_service_->IsPrivateIPAddress(remote_endpoint_.address())) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP);
    }

    // For phishing we only classify HTTP or HTTPS pages.
    if (!url_.SchemeIsHTTPOrHTTPS()) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_SCHEME_NOT_SUPPORTED);
    }

    // Don't run any classifier if the tab is incognito.
    if (web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_OFF_THE_RECORD);
    }

    // Don't start classification if |url_| is allowlisted by enterprise policy.
    if (host_ && host_->GetPrefs() &&
        IsURLAllowlistedByPolicy(url_, *host_->GetPrefs())) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_ALLOWLISTED_BY_POLICY);
    }

    // If the tab has a delayed warning, ignore this second verdict. We don't
    // want to immediately undelay a page that's already blocked as phishy.
    if (host_ && host_->delegate_->HasSafeBrowsingUserInteractionObserver()) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_HAS_DELAYED_WARNING);
    }

    // We lookup the csd-allowlist before we lookup the cache because
    // a URL may have recently been allowlisted.  If the URL matches
    // the csd-allowlist we won't start phishing classification.
    if (ShouldClassifyForPhishing()) {
      CheckSafeBrowsingDatabase(url_);
    }
  }

  void Cancel(ClientSideDetectionType request_type) {
    // We should only log if the callback has not been answered yet.
    if (ShouldClassifyForPhishing()) {
      base::UmaHistogramExactLinear(
          "SBClientPhishing.PreClassificationCheckCancelActor", request_type,
          ClientSideDetectionType_MAX + 1);
      base::UmaHistogramExactLinear(
          base::StrCat({"SBClientPhishing.PreClassificationCheckCancelActor.",
                        GetRequestTypeName(phishing_detection_request_type_)}),
          request_type, ClientSideDetectionType_MAX + 1);
    }
    Cancel();
  }

  void Cancel() {
    DontClassifyForPhishing(PreClassificationCheckResult::NO_CLASSIFY_CANCEL);
    // Just to make sure we don't do anything bad we reset all these
    // pointers except for the safebrowsing service class which may be
    // accessed by CheckSafeBrowsingDatabase().
    web_contents_ = nullptr;
    csd_service_ = nullptr;
    host_ = nullptr;
  }

  bool ShouldClassifyForPhishing() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return !start_phishing_classification_cb_.is_null();
  }

 private:
  friend class base::RefCountedThreadSafe<
      ClientSideDetectionHost::ShouldClassifyUrlRequest>;

  // This enum is used to track the result of the allowlists we use before we
  // decide to classify. Currently, only the CSD match can halt classification
  // from going forward. These values are persisted to logs. Entries should not
  // be renumbered and numeric values should never be reused.
  enum class ClientSideAllowlistMatchResult {
    kNoMatch = 0,
    kCsdMatch = 1,
    kHighConfidenceMatch = 2,
    kCsdAndHighConfidenceMatch = 3,
    kMaxValue = kCsdAndHighConfidenceMatch
  };

  void DontClassifyForPhishing(PreClassificationCheckResult reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (ShouldClassifyForPhishing()) {
      // Track the first reason why we stopped classifying for phishing.
      ClientSideDetectionHostBase::
          RecordPreClassificationCheckResultWithAndWithoutSuffix(
              reason, phishing_detection_request_type_);
      if (host_ && host_->IsEnhancedProtectionEnabled() &&
          // Cancelation happens when the WebContents is destroyed, but we
          // cannot access ClientSideDetectionFeatureCache at that time.
          !web_contents_->IsBeingDestroyed()) {
        ClientSideDetectionFeatureCache::CreateForWebContents(web_contents_);
        ClientSideDetectionFeatureCache* feature_cache_map =
            ClientSideDetectionFeatureCache::FromWebContents(web_contents_);
        // TODO(andysjlim): Investigate why this is null sometimes.
        LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
            feature_cache_map->GetOrCreateDebuggingMetadataForURL(url_);
        if (debugging_metadata) {
          debugging_metadata->set_preclassification_check_result(reason);
        }
      }
      std::move(start_phishing_classification_cb_)
          .Run(false, send_sample_ping_, std::nullopt,
               !remote_endpoint_.address().IsValid());
    }
    start_phishing_classification_cb_.Reset();
  }

  void CheckSafeBrowsingDatabase(const GURL& url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    PreClassificationCheckResult phishing_reason =
        PreClassificationCheckResult::NO_CLASSIFY_MAX;

    // When doing debug feature dumps, ignore the allowlist.
    if (HasDebugFeatureDirectory()) {
      OnAllowlistCheckDone(url, phishing_reason,
                           /*match_allowlist=*/false);
      return;
    }

    if (!database_manager_.get()) {
      // We cannot check the Safe Browsing allowlists so we stop here
      // for safety.
      OnAllowlistCheckDone(
          url,
          /*phishing_reason=*/
          PreClassificationCheckResult::NO_CLASSIFY_NO_DATABASE_MANAGER,
          /*match_allowlist=*/false);
      return;
    }

    // If we get a suspcious verdict from RTLookupResponse, we should get a
    // second opinion on CSD side, so we skip the allowlist. If we get an
    // explicit request to send a report from the user, we skip the allowlist.
    // We also check the command line flag if the allowlist should be skipped.
    if (phishing_detection_request_type_ ==
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST ||
        phishing_detection_request_type_ ==
            safe_browsing::ClientSideDetectionType::USER_REPORT ||
        ShouldSkipCSDAllowlist()) {
      OnAllowlistCheckDone(url, phishing_reason,
                           /*match_allowlist=*/false);
      return;
    }

    // Query the CSD Allowlist asynchronously. We're already on the IO thread so
    // can call AllowlistCheckerClient directly.
    base::OnceCallback<void(bool)> result_callback =
        base::BindOnce(&ClientSideDetectionHost::ShouldClassifyUrlRequest::
                           OnAllowlistCheckDone,
                       weak_factory_.GetWeakPtr(), url, phishing_reason);
    AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, url,
                                                   std::move(result_callback));
  }

  void OnAllowlistCheckDone(const GURL& url,
                            PreClassificationCheckResult phishing_reason,
                            bool match_allowlist) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // On CSD allowlist match, we still want to send a ping on a rare chance.
    send_sample_ping_ = CanSendSamplePing();
    if (match_allowlist && !send_sample_ping_) {
      phishing_reason =
          PreClassificationCheckResult::NO_CLASSIFY_MATCH_CSD_ALLOWLIST;
    }

    if (phishing_reason !=
        PreClassificationCheckResult::NO_CLASSIFY_NO_DATABASE_MANAGER) {
      switch (phishing_detection_request_type_) {
        case CREDIT_CARD_FORM:
        case CLIPBOARD_COPY_API:
        case UNFAMILIAR_LOGIN_PAGE:
          base::UmaHistogramBoolean(
              base::StrCat(
                  {"SBClientPhishing.MatchCSDAllowlistOn",
                   GetRequestTypeName(phishing_detection_request_type_)}),
              match_allowlist);
          break;
        default:
          break;
      }
      // This check is also for logging purposes although the CSD allowlist
      // could be matched or not checked at all. Once it completes,
      // preclassification check will continue.
      database_manager_->CheckUrlForHighConfidenceAllowlist(
          url,
          base::BindOnce(&ClientSideDetectionHost::ShouldClassifyUrlRequest::
                             OnHighConfidenceAllowlistCheckDone,
                         weak_factory_.GetWeakPtr(), phishing_reason,
                         base::TimeTicks::Now()));
    } else {
      CheckCache(phishing_reason);
    }
  }

  void OnHighConfidenceAllowlistCheckDone(
      PreClassificationCheckResult phishing_reason,
      base::TimeTicks check_start_time,
      bool did_match_high_confidence_allowlist,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details) {
    did_match_high_confidence_allowlist_ = did_match_high_confidence_allowlist;
    UmaHistogramMediumTimes(
        "SBClientPhishing.HighConfidenceAllowlistCheckDuration",
        base::TimeTicks::Now() - check_start_time);

    // TODO(andysjlim): This histogram will be logged to
    // PreClassificationCheckResult through |phishing_reason|, but logged
    // separately now because a new field PreClassificationCheckResult results
    // in a new server data to be sent through debugging metadata.
    ClientSideAllowlistMatchResult match_result =
        GetClientSideAllowlistMatchResult(
            phishing_reason ==
                PreClassificationCheckResult::NO_CLASSIFY_MATCH_CSD_ALLOWLIST,
            did_match_high_confidence_allowlist);

    base::UmaHistogramEnumeration(
        "SBClientPhishing.MatchHighConfidenceAllowlist", match_result);
    base::UmaHistogramEnumeration(
        base::StrCat({"SBClientPhishing.MatchHighConfidenceAllowlist.",
                      GetRequestTypeName(phishing_detection_request_type_)}),
        match_result);

    if (phishing_reason == NO_CLASSIFY_MAX && ShouldAcceptHCAllowlist()) {
      phishing_reason =
          PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST;
    }

    CheckCache(phishing_reason);
  }

  void CheckCache(PreClassificationCheckResult phishing_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (phishing_reason != PreClassificationCheckResult::NO_CLASSIFY_MAX) {
      DontClassifyForPhishing(phishing_reason);
    }
    if (!ShouldClassifyForPhishing()) {
      return;  // No point in doing anything else.
    }

    if (ShouldStopAtPreClassification()) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC);
    }

    // For trigger model requests, if result is cached, we don't want to run
    // classification again. In that case we're just trying to show the warning.
    // If we're dumping features for debugging, ignore the cache.
    bool is_phishing;
    if (phishing_detection_request_type_ ==
            ClientSideDetectionType::TRIGGER_MODELS &&
        !HasDebugFeatureDirectory() && host_ && csd_service_ &&
        csd_service_->GetValidCachedResult(url_, &is_phishing)) {
      // Since we are already on the UI thread, this is safe.
      host_->MaybeShowPhishingWarning(
          /*is_from_cache=*/true, ClientSideDetectionType::TRIGGER_MODELS,
          did_match_high_confidence_allowlist_, url_, is_phishing,
          /*response_code=*/std::nullopt,
          /*intelligent_scan_verdict=*/std::nullopt);
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_RESULT_FROM_CACHE);
    }

    // We want to limit the number of requests, but if we're dumping features
    // for debugging or processing an explicit request for a report from a user,
    // allow us to exceed the report limit.
    if (!HasDebugFeatureDirectory() && csd_service_ &&
        phishing_detection_request_type_ !=
            ClientSideDetectionType::USER_REPORT &&
        csd_service_->AtPhishingReportLimit()) {
      base::UmaHistogramExactLinear("SBClientPhishing.RequestTypeAtReportLimit",
                                    phishing_detection_request_type_,
                                    ClientSideDetectionType_MAX + 1);
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_TOO_MANY_REPORTS);
    }

    // Everything checks out, so start classification.
    // |web_contents_| is safe to call as we will be destructed
    // before it is.
    if (ShouldClassifyForPhishing()) {
      ClientSideDetectionHostBase::
          RecordPreClassificationCheckResultWithAndWithoutSuffix(
              PreClassificationCheckResult::CLASSIFY,
              phishing_detection_request_type_);
      if (host_ && host_->IsEnhancedProtectionEnabled()) {
        ClientSideDetectionFeatureCache::CreateForWebContents(web_contents_);
        ClientSideDetectionFeatureCache* feature_cache_map =
            ClientSideDetectionFeatureCache::FromWebContents(web_contents_);
        feature_cache_map->GetOrCreateDebuggingMetadataForURL(url_)
            ->set_preclassification_check_result(
                PreClassificationCheckResult::CLASSIFY);
      }
      std::move(start_phishing_classification_cb_)
          .Run(true, send_sample_ping_, did_match_high_confidence_allowlist_,
               !remote_endpoint_.address().IsValid());
      // Reset the callback to make sure ShouldClassifyForPhishing()
      // returns false.
      start_phishing_classification_cb_.Reset();
    }
  }

  bool ShouldStopAtPreClassification() {
    switch (phishing_detection_request_type_) {
      case CLIPBOARD_COPY_API:
        return base::RandDouble() >= kCsdClipboardCopyApiSampleRate.Get();
      case CREDIT_CARD_FORM:
        return base::RandDouble() >= kCsdCreditCardFormSampleRate.Get();
      case UNFAMILIAR_LOGIN_PAGE:
        return base::RandDouble() >=
               kCsdProactivePasswordProtectionSampleRate.Get();
      default:
        break;
    }
    return false;
  }

  bool CanSendSamplePing() {
    return phishing_detection_request_type_ ==
               ClientSideDetectionType::TRIGGER_MODELS &&
           host_ && host_->IsEnhancedProtectionEnabled() &&
           base::RandDouble() <= kProbabilityForSendingSampleRequest;
  }

  bool ShouldAcceptHCAllowlist() {
    // It can be inferred that it has value because it was set right before, but
    // check again for sanity.
    if (!did_match_high_confidence_allowlist_.has_value() ||
        !did_match_high_confidence_allowlist_.value()) {
      return false;
    }

    switch (phishing_detection_request_type_) {
      case ClientSideDetectionType::TRIGGER_MODELS:
        return base::RandDouble() <=
               probability_for_accepting_hc_allowlist_trigger_;
      case ClientSideDetectionType::CLIPBOARD_COPY_API:
        return base::RandDouble() < kCsdClipboardCopyApiHCAcceptanceRate.Get();
      case ClientSideDetectionType::CREDIT_CARD_FORM:
        return base::RandDouble() < kCsdCreditCardFormHCAcceptanceRate.Get();
      default:
        break;
    }
    return false;
  }

  ClientSideAllowlistMatchResult GetClientSideAllowlistMatchResult(
      bool match_csd_allowlist,
      bool match_hc_allowlist) {
    if (match_csd_allowlist && match_hc_allowlist) {
      return ClientSideAllowlistMatchResult::kCsdAndHighConfidenceMatch;
    } else if (match_csd_allowlist) {
      return ClientSideAllowlistMatchResult::kCsdMatch;
    } else if (match_hc_allowlist) {
      return ClientSideAllowlistMatchResult::kHighConfidenceMatch;
    } else {
      return ClientSideAllowlistMatchResult::kNoMatch;
    }
  }

  const GURL url_;
  bool send_sample_ping_ = false;
  std::optional<bool> did_match_high_confidence_allowlist_;
  std::string mime_type_;
  net::IPEndPoint remote_endpoint_;
  raw_ptr<WebContents> web_contents_;
  base::WeakPtr<ClientSideDetectionServiceBase> csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  ClientSideDetectionType phishing_detection_request_type_;
  float probability_for_accepting_hc_allowlist_trigger_;
  base::WeakPtr<ClientSideDetectionHost> host_;
  ShouldClassifyUrlCallback start_phishing_classification_cb_;

  base::WeakPtrFactory<ShouldClassifyUrlRequest> weak_factory_{this};
};

// static
const int ClientSideDetectionHost::kMaxHighResScreenshotWidth = 4096;
const int ClientSideDetectionHost::kMaxHighResScreenshotHeight = 2160;

// static
std::unique_ptr<ClientSideDetectionHost> ClientSideDetectionHost::Create(
    content::WebContents* tab,
    std::unique_ptr<Delegate> delegate,
    IntelligentScanDelegate* intelligent_scan_delegate,
    PrefService* pref_service,
    VerdictCacheManager* cache_manager,
    history::HistoryService* history_service,
    base::WeakPtr<ClientSideDetectionService> csd_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    bool is_off_the_record,
    const PrimaryAccountSignedIn& account_signed_in_callback) {
  return base::WrapUnique(new ClientSideDetectionHost(
      tab, std::move(delegate), intelligent_scan_delegate, pref_service,
      cache_manager, history_service, csd_service, std::move(token_fetcher),
      is_off_the_record, account_signed_in_callback));
}

ClientSideDetectionHost::ClientSideDetectionHost(
    WebContents* tab,
    std::unique_ptr<Delegate> delegate,
    IntelligentScanDelegate* intelligent_scan_delegate,
    PrefService* pref_service,
    VerdictCacheManager* cache_manager,
    history::HistoryService* history_service,
    base::WeakPtr<ClientSideDetectionService> csd_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    bool is_off_the_record,
    const PrimaryAccountSignedIn& account_signed_in_callback)
    : ClientSideDetectionHostBase(csd_service,
                                  cache_manager,
                                  intelligent_scan_delegate,
                                  pref_service,
                                  std::move(token_fetcher),
                                  history_service,
                                  is_off_the_record),
      content::WebContentsObserver(tab),
      tab_(tab),
      classification_request_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      delegate_(std::move(delegate)),
      account_signed_in_callback_(account_signed_in_callback),
      probability_for_accepting_hc_allowlist_trigger_(
          kProbabilityForAcceptingHCAllowlistTrigger) {
  DCHECK(tab);
  DCHECK(pref_service);
  // Note: the CSD service will be nullptr here in testing.
  if (auto service = GetClientSideDetectionService()) {
    ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
    ClientSideDetectionFeatureCache::FromWebContents(web_contents())
        ->AddClearCacheSubscription(service);
  }

  // |ui_manager_| and |database_manager_| can
  // be null if safe browsing service is not available in the embedder.
  ui_manager_ = delegate_->GetSafeBrowsingUIManager();
  database_manager_ = delegate_->GetSafeBrowsingDBManager();

  RegisterPermissionRequestManager();
  RegisterAsyncCheckTracker();
  RegisterAutofillManager();
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
  MaybeRunUserReportCallback();
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  CancelPendingRequests();
}

void ClientSideDetectionHost::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
  ClientSideDetectionHostBase::CancelPendingRequests();
}

void ClientSideDetectionHost::GetInnerText(HostInnerTextCallback callback) {
  delegate_->GetInnerText(std::move(callback));
}

bool ClientSideDetectionHost::IsAccountSignedIn() {
  return !account_signed_in_callback_.is_null() &&
         account_signed_in_callback_.Run();
}

bool ClientSideDetectionHost::IsErrorDocument() {
  return web_contents()->GetPrimaryMainFrame()->IsErrorDocument();
}

ChromeUserPopulation ClientSideDetectionHost::GetUserPopulation() {
  return delegate_->GetUserPopulation();
}

safe_browsing::credit_card_form::ReferringApp
ClientSideDetectionHost::GetReferringApp() const {
#if BUILDFLAG(IS_ANDROID)
  return safe_browsing::credit_card_form::FromReferringAppInfo(
      delegate_->GetReferringAppInfo(web_contents()));
#else
  return safe_browsing::credit_card_form::kNoReferringApp;
#endif
}

void ClientSideDetectionHost::RegisterPermissionRequestManager() {
  if (IsEnhancedProtectionEnabled()) {
    permission_request_observation_.Observe(
        permissions::PermissionRequestManager::FromWebContents(web_contents()));
  }
}

void ClientSideDetectionHost::RegisterAsyncCheckTracker() {
  if (IsEnhancedProtectionEnabled()) {
    AsyncCheckTracker* tracker =
        AsyncCheckTracker::FromWebContents(web_contents());
    CHECK(tracker);
    async_check_observation_.Observe(tracker);
  }
}

void ClientSideDetectionHost::RegisterAutofillManager() {
  if (!IsEnhancedProtectionEnabled()) {
    return;
  }
  autofill_managers_observation_.Observe(
      autofill::ContentAutofillClient::FromWebContents(web_contents()),
      autofill::ScopedAutofillManagersObservation::InitializationPolicy::
          kObservePreexistingManagers);
}

void ClientSideDetectionHost::MaybeRunUserReportCallback() {
  user_report_timeout_timer_.Stop();
  if (user_report_callback_) {
    std::move(user_report_callback_).Run();
  }
}

void ClientSideDetectionHost::ReportUnsafeSite(SkBitmap screenshot,
                                               base::OnceClosure callback) {
  if (!screenshot.drawsNothing() &&
      screenshot.width() <= kMaxHighResScreenshotWidth &&
      screenshot.height() <= kMaxHighResScreenshotHeight) {
    screenshot_ = screenshot;
  }
  MaybeRunUserReportCallback();
  user_report_callback_ = std::move(callback);

  // Start a 30-second timer that will run the callback if it hasn't been run
  // yet.
  user_report_timeout_timer_.Start(
      FROM_HERE, base::Seconds(kUserReportCallbackTimer),
      base::BindOnce(&ClientSideDetectionHost::MaybeRunUserReportCallback,
                     weak_factory_.GetWeakPtr()));

  MaybeStartPreClassification(ClientSideDetectionType::USER_REPORT);
}

void ClientSideDetectionHost::OnUnfamiliarLoginPageDetected() {
  if (base::FeatureList::IsEnabled(kProactivePasswordProtection) &&
      IsEnhancedProtectionEnabled()) {
    MaybeStartPreClassification(ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE);
  }
}

void ClientSideDetectionHost::MaybeStartPreClassification(
    ClientSideDetectionType request_type) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    MaybeRunUserReportCallback();
    return;
  }

  // Cancel any pending classification request.
  // TODO(b/447359124): Support multiple classifications on the same page.
  if (classification_request_.get()) {
    // First check if there's an ongoing preclassification check.
    if (classification_request_->ShouldClassifyForPhishing() &&
        base::FeatureList::IsEnabled(kClientSideDetectionTierSystem)) {
      if (NewRequestTypeTierHigher(request_type)) {
        classification_request_->Cancel(request_type);
      } else {
        base::UmaHistogramExactLinear(
            base::StrCat({"SBClientPhishing.BlockingRequestType.",
                          GetRequestTypeName(request_type)}),
            last_request_type(), ClientSideDetectionType_MAX + 1);
        return;
      }
    } else {
      classification_request_->Cancel(request_type);
    }
  }

  // If there is a renderer classification going on and the incoming request
  // type is not higher, do not let that cancel pending classification.
  if (is_classifying() &&
      base::FeatureList::IsEnabled(kClientSideDetectionTierSystem) &&
      !NewRequestTypeTierHigher(request_type)) {
    base::UmaHistogramExactLinear(
        base::StrCat({"SBClientPhishing.BlockingRequestType.",
                      GetRequestTypeName(request_type)}),
        last_request_type(), ClientSideDetectionType_MAX + 1);
    return;
  }

  if (!GetClientSideDetectionService()) {
    if (request_type == ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
    return;
  }

  if (!preclassification_started_cb_for_testing_.is_null()) {
    preclassification_started_cb_for_testing_.Run(request_type);
  }

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  set_current_url(rfh->GetLastCommittedURL());
  set_last_committed_url(request_type, current_url());
  current_outermost_main_frame_id_ = rfh->GetGlobalId();

  set_last_request_type(request_type);

  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kTriggerStartsPreClassification, request_type);

  // Check whether we can cassify the current URL for phishing.
  classification_request_ = std::make_unique<ShouldClassifyUrlRequest>(
      rfh->GetLastCommittedURL(), rfh->GetLastResponseHead(),
      base::BindOnce(&ClientSideDetectionHost::OnPhishingPreClassificationDone,
                     weak_factory_.GetWeakPtr(), request_type),
      web_contents(), GetClientSideDetectionService(), database_manager_.get(),
      request_type, probability_for_accepting_hc_allowlist_trigger_,
      weak_factory_.GetWeakPtr());
  classification_request_->Start();
}

void ClientSideDetectionHost::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // `current_navigation_id_` will be set to UnsafeResource, and later in
  // `BaseUIManager::DisplayBlockingPage` we will check
  // `AsyncCheckTracker::IsMainPageResourceLoadPending`, and `AsyncCheckTracker`
  // tracks only committed not-same-document navigations, see
  // `AsyncCheckTracker::DidFinishNavigation`. So we should never set
  // `current_navigation_id_` to non-comitted or same-document navigations.
  if ((navigation_handle->HasCommitted() &&
       !navigation_handle->IsSameDocument())) {
    current_navigation_id_ = navigation_handle->GetNavigationId();
  }
}

void ClientSideDetectionHost::PrimaryPageChanged(content::Page& page) {
  // TODO(noelutz): move this DCHECK to WebContents and fix all the unit tests
  // that don't call this method on the UI thread.
  // DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If we navigate away and there currently is a pending phishing report
  // request we have to cancel it to make sure we don't display an
  // interstitial for the wrong page.  Note that this won't cancel the server
  // ping back but only cancel the showing of the interstitial.
  CancelPendingRequests();

  if (is_csd_running()) {
    base::UmaHistogramExactLinear(
        "SBClientPhishing.ClientSideDetection.InterruptedByNavigation",
        last_request_type(), ClientSideDetectionType_MAX + 1);
  }
  set_is_csd_running(false);
  set_is_classifying(false);
  set_last_request_type(
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED);
  set_should_send_as_force_request(false);
  clear_clipboard_extracted_data();

  MaybeRunUserReportCallback();

  if (base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification) &&
      !IsEnhancedProtectionEnabled()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    if (did_first_visually_non_empty_paint_ ^ on_first_contentful_paint_) {
      auto value = did_first_visually_non_empty_paint_
                       ? CSDObserverCalled::kDidFirstVisuallyNonEmptyPaint
                       : CSDObserverCalled::kOnFirstContentfulPaint;
      base::UmaHistogramEnumeration(
          "SBClientPhishing.SingleObserverCalledOnNewPage", value);
    }
    did_first_visually_non_empty_paint_ = false;
    on_first_contentful_paint_ = false;
    set_trigger_model_request_sent_as_force_request(false);
    // It is possible for the async check force request to complete before this,
    // and we should have the URL set in case it can match in the verdict cache
    // manager.
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    content::NavigationEntry* nav_entry =
        web_contents()->GetController().GetLastCommittedEntry();
    bool is_reload = nav_entry && ui::PageTransitionCoreTypeIs(
                                      nav_entry->GetTransitionType(),
                                      ui::PAGE_TRANSITION_RELOAD);

    if (current_url() == rfh->GetLastCommittedURL() && !is_reload) {
      base::UmaHistogramBoolean("SBClientPhishing.SameURLAtPrimaryPageChanged",
                                true);
    }
    set_current_url(rfh->GetLastCommittedURL());
    return;
  }

  set_trigger_model_request_sent_as_force_request(false);
  MaybeStartPreClassification(ClientSideDetectionType::TRIGGER_MODELS);
}

void ClientSideDetectionHost::DidFirstVisuallyNonEmptyPaint() {
  if (base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification) &&
      !IsEnhancedProtectionEnabled()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    did_first_visually_non_empty_paint_ = true;
    if (on_first_contentful_paint_) {
      if (should_send_as_force_request() || HasForceRequestFromRtUrlLookup()) {
        base::UmaHistogramBoolean(
            "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad",
            true);
        MaybeStartPreClassification(ClientSideDetectionType::FORCE_REQUEST);
      } else {
        MaybeStartPreClassification(ClientSideDetectionType::TRIGGER_MODELS);
      }
    }
  }
}

void ClientSideDetectionHost::OnFirstContentfulPaintInPrimaryMainFrame(
    base::TimeTicks presentation_time) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification) &&
      !IsEnhancedProtectionEnabled()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    on_first_contentful_paint_ = true;
    if (did_first_visually_non_empty_paint_) {
      if (should_send_as_force_request() || HasForceRequestFromRtUrlLookup()) {
        base::UmaHistogramBoolean(
            "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad",
            true);
        MaybeStartPreClassification(ClientSideDetectionType::FORCE_REQUEST);
      } else {
        MaybeStartPreClassification(ClientSideDetectionType::TRIGGER_MODELS);
      }
    }
  }
}

void ClientSideDetectionHost::OnPromptAdded() {
  if (!IsEnhancedProtectionEnabled()) {
    return;
  }

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  CHECK(permission_request_manager);

  if (std::ranges::contains(permission_request_manager->Requests(),
                            permissions::RequestType::kNotifications,
                            &permissions::PermissionRequest::request_type)) {
    MaybeStartPreClassification(
        ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT);
  }
}

void ClientSideDetectionHost::OnPermissionRequestManagerDestructed() {
  permission_request_observation_.Reset();
}

void ClientSideDetectionHost::OnAsyncSafeBrowsingCheckCompleted() {
  if (!HasForceRequestFromRtUrlLookup()) {
    RecordAsyncCheckTriggerForceRequestResult(
        AsyncCheckTriggerForceRequestResult::kSkippedNotForced);
    return;
  }

  // If a TRIGGER_MODELS requested ping is sent as a FORCE_REQUEST, do not allow
  // async check to trigger another request. This is to avoid duplicate pings.
  if (trigger_model_request_sent_as_force_request()) {
    RecordAsyncCheckTriggerForceRequestResult(
        AsyncCheckTriggerForceRequestResult::
            kSkippedTriggerModelsPingSentAsForceRequest);
    return;
  }

  RecordAsyncCheckTriggerForceRequestResult(
      AsyncCheckTriggerForceRequestResult::kTriggered);
  // Any TRIGGER_MODELS from this URL on should be converted to force request.
  set_should_send_as_force_request(true);
  MaybeStartPreClassification(ClientSideDetectionType::FORCE_REQUEST);
}

void ClientSideDetectionHost::OnAsyncSafeBrowsingCheckTrackerDestructed() {
  async_check_observation_.Reset();
}





void ClientSideDetectionHost::MaybeFillScreenshotData(
    ClientPhishingRequest* request) {
  if (request->client_side_detection_type() !=
      ClientSideDetectionType::USER_REPORT) {
    return;
  }

  if (screenshot_) {
    visual_utils::EncodeScreenshot(
        *screenshot_,
        request->mutable_visual_features()->mutable_high_res_screenshot());
  }
  screenshot_ = std::nullopt;
}

void ClientSideDetectionHost::KeyboardLockRequested() {
  if (!IsEnhancedProtectionEnabled()) {
    return;
  }

  if (!HasDonePreclassificationCheckOnSameURL(
          ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED)) {
    MaybeStartPreClassification(
        ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
  }
}

void ClientSideDetectionHost::VibrationRequested() {
  if (!IsEnhancedProtectionEnabled()) {
    return;
  }

  // Vibration API can be triggered on a page in intervals between 0 and 1
  // seconds. Because of this, we want to only classify once per given URL since
  // a page can send a request multiple vibration at a time.
  if (!HasDonePreclassificationCheckOnSameURL(
          ClientSideDetectionType::VIBRATION_API)) {
    MaybeStartPreClassification(ClientSideDetectionType::VIBRATION_API);
  }
}

void ClientSideDetectionHost::OnTextCopiedToClipboard(
    content::RenderFrameHost* render_frame_host,
    const std::u16string& copied_text) {
  ClientSideDetectionHostBase::OnTextCopiedToClipboard(copied_text);
}



void ClientSideDetectionHost::OnPhishingPreClassificationDone(
    ClientSideDetectionType request_type,
    bool should_classify,
    bool is_sample_ping,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kPreClassificationCheckComplete, request_type);

  if (!preclassification_done_cb_for_testing_.is_null()) {
    preclassification_done_cb_for_testing_.Run(request_type);
  }

  if (!should_classify) {
    if (request_type == ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
    return;
  }

  set_is_csd_running(true);

  bool intelligent_scan_ongoing = GetIntelligentScanId().has_value();
  base::UmaHistogramBoolean(
      "SBClientPhishing.IntelligentScanOngoingOnNewPreclassification",
      intelligent_scan_ongoing);
  base::UmaHistogramBoolean(
      base::StrCat(
          {"SBClientPhishing.IntelligentScanOngoingOnNewPreclassification.",
           GetRequestTypeName(request_type)}),
      intelligent_scan_ongoing);
  if (intelligent_scan_ongoing) {
    DCHECK(GetIntelligentScanDelegate());
    GetIntelligentScanDelegate()->CancelIntelligentScan(
        *GetIntelligentScanId());
  }

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  phishing_detector_.reset();
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&phishing_detector_);

  if (!phishing_detector_.is_bound()) {
    if (request_type == ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
    return;
  }

  // For TRIGGER_MODELS and IMAGE_EMBEDDING_MATCH only, perform phishing
  // classification as the next step.
  // For all other triggers, if enabled, skip phishing detection.
  if (IsEnhancedProtectionEnabled() &&
      request_type == ClientSideDetectionType::TRIGGER_MODELS &&
      // Only ESB users should be in the study.
      base::FeatureList::IsEnabled(kClientSideDetectionImageEmbeddingMatch)) {
    LogClientSideDetectionEvent(
        ClientSideDetectionEvent::kImageClassificationBegin, request_type);
    set_is_classifying(true);
    phishing_detector_->StartPhishingDetection(
        current_url(),
        GetClientSideDetectionMojomType(
            ClientSideDetectionType::IMAGE_EMBEDDING_MATCH),
        base::BindOnce(&ClientSideDetectionHost::PhishingDetectionDone,
                       weak_factory_.GetWeakPtr(),
                       ClientSideDetectionType::IMAGE_EMBEDDING_MATCH,
                       is_sample_ping, did_match_high_confidence_allowlist,
                       is_invalid_ip, tick_clock_->NowTicks()));
    return;
  }

  if (request_type != ClientSideDetectionType::TRIGGER_MODELS &&
      base::FeatureList::IsEnabled(
          kSkipImageClassificationScoringForNonPageLoadTriggers)) {
    ClientPhishingRequest verdict;
    verdict.set_url(current_url().spec());
    verdict.set_client_score(0.0);
    PhishingDetectionDone(request_type, is_sample_ping,
                          did_match_high_confidence_allowlist, is_invalid_ip,
                          tick_clock_->NowTicks(),
                          mojom::PhishingDetectorResult::CLASSIFICATION_SKIPPED,
                          mojo_base::ProtoWrapper(verdict));
    return;
  }

  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationBegin, request_type);
  set_is_classifying(true);
  phishing_detector_->StartPhishingDetection(
      current_url(), GetClientSideDetectionMojomType(request_type),
      base::BindOnce(&ClientSideDetectionHost::PhishingDetectionDone,
                     weak_factory_.GetWeakPtr(), request_type, is_sample_ping,
                     did_match_high_confidence_allowlist, is_invalid_ip,
                     tick_clock_->NowTicks()));
}

void ClientSideDetectionHost::PhishingDetectionDone(
    ClientSideDetectionType request_type,
    bool is_sample_ping,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    base::TimeTicks start_time,
    mojom::PhishingDetectorResult result,
    std::optional<mojo_base::ProtoWrapper> wrapped_verdict) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(GetClientSideDetectionService());

  phishing_detector_.reset();

  std::optional<ClientPhishingRequest> verdict;
  // There's no need to parse the wrapped verdict if the result is not SUCCESS
  // or CLASSIFICATION_SKIPPED.
  if (wrapped_verdict.has_value() &&
      (result == mojom::PhishingDetectorResult::SUCCESS ||
       result == mojom::PhishingDetectorResult::CLASSIFICATION_SKIPPED)) {
    verdict = wrapped_verdict->As<ClientPhishingRequest>();
  }

  ClientSideDetectionHostBase::PhishingDetectionDone(
      request_type, is_sample_ping, did_match_high_confidence_allowlist,
      is_invalid_ip, start_time, GetPhishingDetectorResult(result),
      std::move(verdict));
}

void ClientSideDetectionHost::ClassifyPhishingThroughThresholds(
    ClientPhishingRequest* verdict) {
  GetClientSideDetectionService()->ClassifyPhishingThroughThresholds(verdict);
  VLOG(2) << "Phishing classification score: " << verdict->client_score();
  VLOG(2) << "Visual model scores:";
  for (const ClientPhishingRequest::CategoryScore& label_and_value :
       verdict->tflite_model_scores()) {
    VLOG(2) << label_and_value.label() << ": " << label_and_value.value();
  }
}

visual_utils::CanExtractVisualFeaturesResult
ClientSideDetectionHost::DetermineVisualFeaturesExtraction() {
  int viewport_width = -1;
  int viewport_height = -1;
  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result;
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
#if BUILDFLAG(IS_ANDROID)
  gfx::Size size;
  // native view can be null in tests.
  if (view && view->GetNativeView()) {
    gfx::SizeF viewport = view->GetNativeView()->viewport_size();
    viewport_width = static_cast<int>(viewport.width());
    viewport_height = static_cast<int>(viewport.height());
    size = gfx::Size(viewport_width, viewport_height);
  }
  can_extract_visual_features_result = visual_utils::CanExtractVisualFeatures(
      IsEnhancedProtectionEnabled(),
      web_contents()->GetBrowserContext()->IsOffTheRecord(), size);
#else
  gfx::Size size;
  if (view) {
    size = view->GetVisibleViewportSize();
    viewport_width = size.width();
    viewport_height = size.height();
  }
  can_extract_visual_features_result = visual_utils::CanExtractVisualFeatures(
      IsEnhancedProtectionEnabled(),
      web_contents()->GetBrowserContext()->IsOffTheRecord(), size,
      zoom::ZoomController::GetZoomLevelForWebContents(web_contents()));
#endif
  base::UmaHistogramSparse("SBClientPhishing.Viewport.Width", viewport_width);
  base::UmaHistogramSparse("SBClientPhishing.Viewport.Height", viewport_height);

  float ppi = 0;
  if (view && view->GetNativeView() && display::Screen::Get()) {
    ppi = display::Screen::Get()
              ->GetDisplayNearestView(view->GetNativeView())
              .GetPixelsPerInchX();
  }
  base::UmaHistogramSparse("SBClientPhishing.Viewport.PixelsPerInch",
                           static_cast<int>(ppi));

  if (viewport_width <= 0xFFFF && viewport_width >= 0 &&
      viewport_height <= 0xFFFF && viewport_height >= 0) {
    int32_t encoded_resolution = (viewport_width << 16) | viewport_height;
    base::UmaHistogramSparse("SBClientPhishing.Viewport.EncodedResolution",
                             encoded_resolution);
  }

  base::UmaHistogramEnumeration("SBClientPhishing.VisualFeaturesClearReason2",
                                can_extract_visual_features_result);
  return can_extract_visual_features_result;
}


void ClientSideDetectionHost::PhishingImageEmbeddingDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    mojom::PhishingImageEmbeddingResult result,
    std::optional<mojo_base::ProtoWrapper> image_feature_embedding_wrapper,
    std::optional<mojo_base::ProtoWrapper> visual_features_wrapper) {
  LogClientSideDetectionEvent(ClientSideDetectionEvent::kImageEmbeddingComplete,
                              verdict->client_side_detection_type());

  std::string_view request_type_name =
      GetRequestTypeName(verdict->client_side_detection_type());

  base::TimeDelta image_embedding_duration =
      base::TimeTicks::Now() - image_embedding_start_time_;
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.PhishingImageEmbeddingDuration",
      image_embedding_duration);
  base::UmaHistogramMediumTimes(
      base::StrCat({"SBClientPhishing.PhishingImageEmbeddingDuration.",
                    request_type_name}),
      image_embedding_duration);
  base::UmaHistogramEnumeration("SBClientPhishing.PhishingImageEmbeddingResult",
                                result);
  base::UmaHistogramEnumeration(
      base::StrCat({"SBClientPhishing.PhishingImageEmbeddingResult.",
                    request_type_name}),
      result);

  // If the embedding was not possible due to an invalid document, then exit
  // early without sending a ping since feature extraction is not possible.
  if (result == mojom::PhishingImageEmbeddingResult::kInvalidURLFormatRequest ||
      result == mojom::PhishingImageEmbeddingResult::kInvalidDocumentLoader) {
    set_is_csd_running(false);
    if (verdict->client_side_detection_type() ==
        ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
    return;
  }

  if (result == mojom::PhishingImageEmbeddingResult::kSuccess) {
    std::optional<ImageFeatureEmbedding> embedding;
    if (image_feature_embedding_wrapper.has_value()) {
      embedding = image_feature_embedding_wrapper->As<ImageFeatureEmbedding>();
    }
    if (embedding.has_value()) {
      embedding->set_embedding_model_version(
          GetClientSideDetectionService()->GetImageEmbeddingModelVersion());
      *verdict->mutable_image_feature_embedding() =
          std::move(embedding.value());
      // Tier 2 and higher will add embedding metadata information because lower
      // tiers process and require the embedding metadata phishy condition to
      // go further, whereas tier 2 and above do not.
      if (base::FeatureList::IsEnabled(kClientSideDetectionTierSystem) &&
          GetClientSideDetectionTypeTier(
              verdict->client_side_detection_type()) <= 2) {
        GetClientSideDetectionService()->ClassifyThroughEmbeddings(
            verdict.get());
      }
    } else {
      VLOG(0) << "Failed to parse image feature embedding.";
    }

    std::optional<VisualFeatures> visual_features;
    if (visual_features_wrapper.has_value()) {
      visual_features = visual_features_wrapper->As<VisualFeatures>();
    }
    if (visual_features.has_value()) {
      *verdict->mutable_visual_features() = std::move(visual_features.value());
    }
    if (!verdict->has_visual_features()) {
      VLOG(0) << "Failed to parse visual features.";
    }
    base::UmaHistogramBoolean(
        "SBClientPhishing.VisualFeaturesExistAfterImageEmbedding",
        verdict->has_visual_features());
  }

  MaybeStartIntelligentScanForScamDetection(
      std::move(verdict), did_match_high_confidence_allowlist, is_invalid_ip);
}


void ClientSideDetectionHost::MaybeShowPhishingWarning(
    bool is_from_cache,
    ClientSideDetectionType request_type,
    std::optional<bool> did_match_high_confidence_allowlist,
    GURL phishing_url,
    bool is_phishing,
    std::optional<net::HttpStatusCode> response_code,
    std::optional<IntelligentScanVerdict> intelligent_scan_verdict) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string_view request_type_name = GetRequestTypeName(request_type);
  if (!is_from_cache) {
    LogClientSideDetectionEvent(
        ClientSideDetectionEvent::kNetworkResponseReceived, request_type);
    base::UmaHistogramBoolean("SBClientPhishing.ServerModelDetectsPhishing",
                              is_phishing);
    base::UmaHistogramBoolean(
        base::StrCat({"SBClientPhishing.ServerModelDetectsPhishing.",
                      request_type_name}),
        is_phishing);
  }

  if (IsEnhancedProtectionEnabled() && response_code.has_value()) {
    ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
    ClientSideDetectionFeatureCache* feature_cache_map =
        ClientSideDetectionFeatureCache::FromWebContents(web_contents());
    feature_cache_map->GetOrCreateDebuggingMetadataForURL(phishing_url)
        ->set_network_result(response_code.value());
  }

  if (IsEnhancedProtectionEnabled() && intelligent_scan_verdict.has_value()) {
    base::UmaHistogramExactLinear("SBClientPhishing.IntelligentScanVerdict",
                                  intelligent_scan_verdict.value(),
                                  IntelligentScanVerdict_MAX + 1);
  }

  DCHECK(GetIntelligentScanDelegate());
  bool should_show_scam_warning =
      GetIntelligentScanDelegate()->ShouldShowScamWarning(
          intelligent_scan_verdict);

  // We will only show the warning if |is_phishing| is true, or while the
  // feature is enabled, the intelligent scan verdict matches the corresponding
  // feature. When a feature is cleaned up, remove the feature enabled check
  // alongside the corresponding IntelligentScanVerdict.
  if (is_phishing || should_show_scam_warning) {
    if (!is_from_cache && did_match_high_confidence_allowlist.has_value()) {
      base::UmaHistogramBoolean(
          "SBClientPhishing.HighConfidenceAllowlistMatchOnServerVerdictPhishy",
          did_match_high_confidence_allowlist.value());
      base::UmaHistogramBoolean(
          base::StrCat({"SBClientPhishing."
                        "HighConfidenceAllowlistMatchOnServerVerdictPhishy.",
                        request_type_name}),
          did_match_high_confidence_allowlist.value());
    }
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      auto* primary_main_frame = web_contents()->GetPrimaryMainFrame();
      const content::GlobalRenderFrameHostId primary_main_frame_id =
          primary_main_frame->GetGlobalId();

      security_interstitials::UnsafeResource resource;
      resource.url = phishing_url;
      resource.original_url = phishing_url;
      resource.threat_type =
          SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
      resource.threat_source =
          safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
      resource.navigation_id = current_navigation_id_;
      // When we present a scam warning, we want to add separate interstitial
      // metrics to track specifics.
      if (should_show_scam_warning) {
        resource.threat_subtype = GetThreatSubtype(*intelligent_scan_verdict);
        DCHECK(GetIntelligentScanDelegate());
        GetIntelligentScanDelegate()->OnScamWarningShown();
      }
      resource.rfh_locator = security_interstitials::UnsafeResourceLocator::
          CreateForRenderFrameToken(
              primary_main_frame_id.child_id.value(),
              primary_main_frame->GetFrameToken().value());
      if (!ui_manager_->IsAllowlisted(
              resource.url, resource.rfh_locator, resource.navigation_id,
              resource.threat_type, resource.threat_source)) {
        // We need to stop any pending navigations, otherwise the interstitial
        // might not get created properly.
        web_contents()->GetController().DiscardNonCommittedEntries();
      }
      LogClientSideDetectionEvent(ClientSideDetectionEvent::kWarningShown,
                                  request_type);
      ui_manager_->DisplayBlockingPage(resource);
    }
    // If there is true phishing verdict, invalidate weakptr so that no longer
    // consider the malware vedict.
    CancelPendingRequests();
  }
}


void ClientSideDetectionHost::set_ui_manager(BaseUIManager* ui_manager) {
  ui_manager_ = ui_manager;
}

void ClientSideDetectionHost::set_database_manager(
    SafeBrowsingDatabaseManager* database_manager) {
  database_manager_ = database_manager;
}

void ClientSideDetectionHost::AddMiscellaneousMetadataToClientPhishingRequest(
    ClientPhishingRequest* verdict,
    bool is_invalid_ip) {
  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  // Check the frame id as a precaution against unexpected race conditions.
  if (rfh && rfh->GetGlobalId() == current_outermost_main_frame_id_) {
    const network::mojom::URLResponseHead* response_head =
        rfh->GetLastResponseHead();
    if (response_head && response_head->headers) {
      verdict->set_http_response_code(response_head->headers->response_code());
    }
  }

  ClientSideDetectionHostBase::AddMiscellaneousMetadataToClientPhishingRequest(
      verdict, is_invalid_ip);
}

void ClientSideDetectionHost::AddReferrerChain(ClientPhishingRequest* verdict) {
  if (IsEnhancedProtectionEnabled()) {
    delegate_->AddReferrerChain(verdict, current_url(),
                                current_outermost_main_frame_id_);
  }
}

void ClientSideDetectionHost::MaybeStartGeminiAntiscamProtection(
    GURL url,
    ClientSideDetectionType request_type,
    std::optional<bool> did_match_high_confidence_allowlist) {
  delegate_->MaybeStartGeminiAntiscamProtection(
      url, request_type, did_match_high_confidence_allowlist);
}

void ClientSideDetectionHost::
    set_high_confidence_allowlist_acceptance_rate_for_testing(
        float acceptance_rate) {
  probability_for_accepting_hc_allowlist_trigger_ = acceptance_rate;
}

ClientSideDetectionFeatureCacheBase*
ClientSideDetectionHost::GetFeatureCache() {
  ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
  return ClientSideDetectionFeatureCache::FromWebContents(web_contents());
}

void ClientSideDetectionHost::MaybeStartImageEmbedding(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    PhishingDetectorResult result) {
  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result = DetermineVisualFeaturesExtraction();

  // Clear the blurred image from the visual features if we should not extract
  // visual features.
  if (can_extract_visual_features_result !=
      visual_utils::CanExtractVisualFeaturesResult::kCanExtractVisualFeatures) {
    verdict->mutable_visual_features()->clear_image();
  } else {
    base::UmaHistogramBoolean("SBClientPhishing.HasVisualFeaturesImage2",
                              verdict->has_visual_features() &&
                                  verdict->visual_features().has_image());
  }

  if (auto service = GetClientSideDetectionService();
      IsEnhancedProtectionEnabled() && service &&
      service->HasImageEmbeddingModel() &&
      service->IsModelMetadataImageEmbeddingVersionMatching() &&
      !verdict->has_image_feature_embedding()) {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

    phishing_image_embedder_.reset();
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(
        &phishing_image_embedder_);

    if (phishing_image_embedder_.is_bound()) {
      LogClientSideDetectionEvent(
          ClientSideDetectionEvent::kImageEmbeddingBegin,
          verdict->client_side_detection_type());
      bool can_extract_visual_features =
          result == PhishingDetectorResult::CLASSIFICATION_SKIPPED ||
          can_extract_visual_features_result ==
              visual_utils::CanExtractVisualFeaturesResult::
                  kCanExtractVisualFeatures;
      image_embedding_start_time_ = tick_clock_->NowTicks();
      phishing_image_embedder_->StartImageEmbedding(
          current_url(), can_extract_visual_features,
          base::BindOnce(&ClientSideDetectionHost::PhishingImageEmbeddingDone,
                         weak_factory_.GetWeakPtr(), std::move(verdict),
                         did_match_high_confidence_allowlist, is_invalid_ip));
    }

    return;
  }

  MaybeStartIntelligentScanForScamDetection(
      std::move(verdict), did_match_high_confidence_allowlist, is_invalid_ip);
}

std::vector<GURL> ClientSideDetectionHost::GetRedirectChain() {
  if (!web_contents() ||
      !web_contents()->GetController().GetLastCommittedEntry()) {
    return std::vector<GURL>();
  }
  return web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->GetRedirectChain();
}

GURL ClientSideDetectionHost::GetCurrentUrl() const {
  if (!web_contents() || !web_contents()->GetPrimaryMainFrame()) {
    return GURL();
  }
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL();
}

}  // namespace safe_browsing
