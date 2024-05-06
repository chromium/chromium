// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/uuid.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/common/visual_utils.h"
#include "components/safe_browsing/core/browser/db/allowlist_checker_client.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/view_android.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace safe_browsing {

namespace {

// Command-line flag that can be used to write extracted CSD features to disk.
// This is also enables a few other behaviors that are useful for debugging.
const char kCsdDebugFeatureDirectoryFlag[] = "csd-debug-feature-directory";
const char kSkipCSDAllowlistOnPreclassification[] =
    "safe-browsing-skip-csd-allowlist";
const float kProbabilityForSendingSampleRequest = 0.01;

void WriteFeaturesToDisk(const ClientPhishingRequest& features,
                         const base::FilePath& base_path) {
  base::FilePath path =
      base_path.AppendASCII(base::Uuid::GenerateRandomV4().AsLowercaseString());
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return;
  }
  std::string serialized_features = features.SerializeAsString();
  file.WriteAtCurrentPos(serialized_features.data(),
                         serialized_features.size());
}

bool HasDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kCsdDebugFeatureDirectoryFlag);
}

bool ShouldSkipCSDAllowlist() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSkipCSDAllowlistOnPreclassification);
}

base::FilePath GetDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      kCsdDebugFeatureDirectoryFlag);
}

std::string GetRequestTypeName(
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
  }
}

}  // namespace

typedef base::OnceCallback<void(bool)> ShouldClassifyUrlCallback;

// This class is instantiated each time a new toplevel URL loads, and
// asynchronously checks whether the phishing classifier should run
// for this URL.  If so, it notifies the host class by calling the provided
// callback form the UI thread.  Objects of this class are ref-counted and will
// be destroyed once nobody uses it anymore.  If |web_contents|, |csd_service|
// or |host| go away you need to call Cancel().  We keep the |database_manager|
// alive in a ref pointer for as long as it takes.
class ClientSideDetectionHost::ShouldClassifyUrlRequest
    : public base::RefCountedThreadSafe<
          ClientSideDetectionHost::ShouldClassifyUrlRequest> {
 public:
  ShouldClassifyUrlRequest(
      const GURL& url,
      const network::mojom::URLResponseHead* response_head,
      ShouldClassifyUrlCallback start_phishing_classification,
      WebContents* web_contents,
      base::WeakPtr<ClientSideDetectionService> csd_service,
      SafeBrowsingDatabaseManager* database_manager,
      ClientSideDetectionType phishing_detection_request_type,
      base::WeakPtr<ClientSideDetectionHost> host)
      : url_(url),
        web_contents_(web_contents),
        csd_service_(csd_service),
        database_manager_(database_manager),
        phishing_detection_request_type_(phishing_detection_request_type),
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

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // We start by doing some simple checks that can run on the UI thread.
    base::UmaHistogramBoolean("SBClientPhishing.ClassificationStart", true);

    if (url_.SchemeIs(content::kChromeUIScheme)) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_CHROME_UI_PAGE);
    }

    if (csd_service_ &&
        csd_service_->IsLocalResource(remote_endpoint_.address())) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE);
    }

    // Only classify [X]HTML documents.
    if (mime_type_ != "text/html" && mime_type_ != "application/xhtml+xml") {
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
    if (host_ && host_->delegate_->GetPrefs() &&
        IsURLAllowlistedByPolicy(url_, *host_->delegate_->GetPrefs())) {
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

  void Cancel() {
    DontClassifyForPhishing(PreClassificationCheckResult::NO_CLASSIFY_CANCEL);
    // Just to make sure we don't do anything stupid we reset all these
    // pointers except for the safebrowsing service class which may be
    // accessed by CheckSafeBrowsingDatabase().
    web_contents_ = nullptr;
    csd_service_ = nullptr;
    host_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<
      ClientSideDetectionHost::ShouldClassifyUrlRequest>;

  // The destructor can be called either from the UI or the IO thread.
  ~ShouldClassifyUrlRequest() = default;

  bool ShouldClassifyForPhishing() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return !start_phishing_classification_cb_.is_null();
  }

  void DontClassifyForPhishing(PreClassificationCheckResult reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (ShouldClassifyForPhishing()) {
      // Track the first reason why we stopped classifying for phishing.
      base::UmaHistogramEnumeration(
          "SBClientPhishing.PreClassificationCheckResult", reason,
          PreClassificationCheckResult::NO_CLASSIFY_MAX);
      if (base::FeatureList::IsEnabled(kClientSideDetectionImagesCache) &&
          base::FeatureList::IsEnabled(
              kClientSideDetectionDebuggingMetadataCache) &&
          host_ && host_->delegate_->GetPrefs() &&
          IsEnhancedProtectionEnabled(*host_->delegate_->GetPrefs())) {
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
      std::move(start_phishing_classification_cb_).Run(false);
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

    if (ShouldSkipCSDAllowlist()) {
      // Command line flag to skip the allowlist check has been set.
      OnAllowlistCheckDone(url, phishing_reason,
                           /*match_allowlist=*/false);
      return;
    }

    // Query the CSD Allowlist asynchronously. We're already on the IO thread so
    // can call AllowlistCheckerClient directly.
    base::OnceCallback<void(bool)> result_callback =
        base::BindOnce(&ClientSideDetectionHost::ShouldClassifyUrlRequest::
                           OnAllowlistCheckDone,
                       this, url, phishing_reason);
    AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, url,
                                                   std::move(result_callback));
  }

  void OnAllowlistCheckDone(const GURL& url,
                            PreClassificationCheckResult phishing_reason,
                            bool match_allowlist) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (match_allowlist && !CanSendSamplePing()) {
      phishing_reason =
          PreClassificationCheckResult::NO_CLASSIFY_MATCH_CSD_ALLOWLIST;
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
    // For trigger model requests, if result is cached, we don't want to run
    // classification again. In that case we're just trying to show the warning.
    // If we're dumping features for debugging, ignore the cache.
    bool is_phishing;
    if (phishing_detection_request_type_ ==
            ClientSideDetectionType::TRIGGER_MODELS &&
        !HasDebugFeatureDirectory() && host_ && csd_service_ &&
        csd_service_->GetValidCachedResult(url_, &is_phishing)) {
      // Since we are already on the UI thread, this is safe.
      host_->MaybeShowPhishingWarning(/*is_from_cache=*/true,
                                      ClientSideDetectionType::TRIGGER_MODELS,
                                      url_, is_phishing,
                                      /*response_code=*/std::nullopt);
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_RESULT_FROM_CACHE);
    }

    // We want to limit the number of requests, but if we're dumping features
    // for debugging, allow us to exceed the report limit.
    if (!HasDebugFeatureDirectory() && csd_service_ &&
        csd_service_->OverPhishingReportLimit()) {
      DontClassifyForPhishing(
          PreClassificationCheckResult::NO_CLASSIFY_TOO_MANY_REPORTS);
    }

    // Everything checks out, so start classification.
    // |web_contents_| is safe to call as we will be destructed
    // before it is.
    if (ShouldClassifyForPhishing()) {
      base::UmaHistogramEnumeration(
          "SBClientPhishing.PreClassificationCheckResult",
          PreClassificationCheckResult::CLASSIFY,
          PreClassificationCheckResult::NO_CLASSIFY_MAX);
      if (base::FeatureList::IsEnabled(kClientSideDetectionImagesCache) &&
          base::FeatureList::IsEnabled(
              kClientSideDetectionDebuggingMetadataCache) &&
          host_ && host_->delegate_->GetPrefs() &&
          IsEnhancedProtectionEnabled(*host_->delegate_->GetPrefs())) {
        ClientSideDetectionFeatureCache::CreateForWebContents(web_contents_);
        ClientSideDetectionFeatureCache* feature_cache_map =
            ClientSideDetectionFeatureCache::FromWebContents(web_contents_);
        feature_cache_map->GetOrCreateDebuggingMetadataForURL(url_)
            ->set_preclassification_check_result(
                PreClassificationCheckResult::CLASSIFY);
      }
      std::move(start_phishing_classification_cb_).Run(true);
      // Reset the callback to make sure ShouldClassifyForPhishing()
      // returns false.
      start_phishing_classification_cb_.Reset();
    }
  }

  bool CanSendSamplePing() {
    return host_ && host_->delegate_->GetPrefs() &&
           IsEnhancedProtectionEnabled(*host_->delegate_->GetPrefs()) &&
           base::RandDouble() <= kProbabilityForSendingSampleRequest &&
           base::FeatureList::IsEnabled(kClientSideDetectionSamplePing);
  }

  const GURL url_;
  std::string mime_type_;
  net::IPEndPoint remote_endpoint_;
  raw_ptr<WebContents> web_contents_;
  base::WeakPtr<ClientSideDetectionService> csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  ClientSideDetectionType phishing_detection_request_type_;
  base::WeakPtr<ClientSideDetectionHost> host_;

  ShouldClassifyUrlCallback start_phishing_classification_cb_;
};

// static
std::unique_ptr<ClientSideDetectionHost> ClientSideDetectionHost::Create(
    content::WebContents* tab,
    std::unique_ptr<Delegate> delegate,
    PrefService* pref_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    bool is_off_the_record,
    const PrimaryAccountSignedIn& account_signed_in_callback) {
  return base::WrapUnique(new ClientSideDetectionHost(
      tab, std::move(delegate), pref_service, std::move(token_fetcher),
      is_off_the_record, account_signed_in_callback));
}

ClientSideDetectionHost::ClientSideDetectionHost(
    WebContents* tab,
    std::unique_ptr<Delegate> delegate,
    PrefService* pref_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    bool is_off_the_record,
    const PrimaryAccountSignedIn& account_signed_in_callback)
    : content::WebContentsObserver(tab),
      csd_service_(nullptr),
      tab_(tab),
      classification_request_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      delegate_(std::move(delegate)),
      pref_service_(pref_service),
      token_fetcher_(std::move(token_fetcher)),
      is_off_the_record_(is_off_the_record),
      account_signed_in_callback_(account_signed_in_callback) {
  DCHECK(tab);
  DCHECK(pref_service);
  // Note: csd_service_ and sb_service will be nullptr here in testing.
  csd_service_ = delegate_->GetClientSideDetectionService();

  if (csd_service_ &&
      base::FeatureList::IsEnabled(kClientSideDetectionImagesCache)) {
    ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
    ClientSideDetectionFeatureCache::FromWebContents(web_contents())
        ->AddClearCacheSubscription(csd_service_);
  }

  // |ui_manager_| and |database_manager_| can
  // be null if safe browsing service is not available in the embedder.
  ui_manager_ = delegate_->GetSafeBrowsingUIManager();
  database_manager_ = delegate_->GetSafeBrowsingDBManager();

  RegisterPermissionRequestManager();
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
}

void ClientSideDetectionHost::RegisterPermissionRequestManager() {
  if (IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) &&
      base::FeatureList::IsEnabled(kClientSideDetectionNotificationPrompt)) {
    observation_.Observe(
        permissions::PermissionRequestManager::FromWebContents(web_contents()));
  }
}

void ClientSideDetectionHost::MaybeStartPreClassification(
    ClientSideDetectionType request_type) {
  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    return;
  }
  // Cancel any pending classification request.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  // If we navigate away and there currently is a pending phishing report
  // request we have to cancel it to make sure we don't display an interstitial
  // for the wrong page.  Note that this won't cancel the server ping back but
  // only cancel the showing of the interstitial.
  weak_factory_.InvalidateWeakPtrs();

  if (!csd_service_) {
    return;
  }

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

  current_url_ = rfh->GetLastCommittedURL();
  current_outermost_main_frame_id_ = rfh->GetGlobalId();
  // Check whether we can cassify the current URL for phishing.
  classification_request_ = new ShouldClassifyUrlRequest(
      rfh->GetLastCommittedURL(), rfh->GetLastResponseHead(),
      base::BindOnce(&ClientSideDetectionHost::OnPhishingPreClassificationDone,
                     weak_factory_.GetWeakPtr(), request_type),
      web_contents(), csd_service_, database_manager_.get(), request_type,
      weak_factory_.GetWeakPtr());
  classification_request_->Start();
}

void ClientSideDetectionHost::PrimaryPageChanged(content::Page& page) {
  // TODO(noelutz): move this DCHECK to WebContents and fix all the unit tests
  // that don't call this method on the UI thread.
  // DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MaybeStartPreClassification(ClientSideDetectionType::TRIGGER_MODELS);
}

void ClientSideDetectionHost::OnPromptAdded() {
  if (!IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
    return;
  }

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  CHECK(permission_request_manager);

  if (base::Contains(permission_request_manager->Requests(),
                     permissions::RequestType::kNotifications,
                     &permissions::PermissionRequest::request_type)) {
    MaybeStartPreClassification(
        ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT);
  }
}

void ClientSideDetectionHost::OnPermissionRequestManagerDestructed() {
  observation_.Reset();
}

void ClientSideDetectionHost::KeyboardLockRequested() {
  if (!IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) ||
      !base::FeatureList::IsEnabled(
          kClientSideDetectionKeyboardPointerLockRequest)) {
    return;
  }

  MaybeStartPreClassification(ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED);
}

void ClientSideDetectionHost::PointerLockRequested() {
  if (!IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) ||
      !base::FeatureList::IsEnabled(
          kClientSideDetectionKeyboardPointerLockRequest)) {
    return;
  }

  MaybeStartPreClassification(ClientSideDetectionType::POINTER_LOCK_REQUESTED);
}

void ClientSideDetectionHost::OnPhishingPreClassificationDone(
    ClientSideDetectionType request_type,
    bool should_classify) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_classify) {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

    phishing_detector_.reset();
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&phishing_detector_);

    if (phishing_detector_.is_bound()) {
      phishing_detection_start_time_ = tick_clock_->NowTicks();
      phishing_detector_->StartPhishingDetection(
          current_url_,
          base::BindOnce(&ClientSideDetectionHost::PhishingDetectionDone,
                         weak_factory_.GetWeakPtr(), request_type));
    }
  }
}

void ClientSideDetectionHost::PhishingDetectionDone(
    ClientSideDetectionType request_type,
    mojom::PhishingDetectorResult result,
    std::optional<mojo_base::ProtoWrapper> wrapped_verdict) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(csd_service_);

  ClientSideDetectionFeatureCache* feature_cache_map = nullptr;

  if (base::FeatureList::IsEnabled(kClientSideDetectionImagesCache)) {
    ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
    feature_cache_map =
        ClientSideDetectionFeatureCache::FromWebContents(web_contents());
  }

  phishing_detector_.reset();

  std::string request_type_name = GetRequestTypeName(request_type);

  UmaHistogramMediumTimes(
      "SBClientPhishing.PhishingDetectionDuration",
      base::TimeTicks::Now() - phishing_detection_start_time_);
  UmaHistogramMediumTimes(
      "SBClientPhishing.PhishingDetectionDuration." + request_type_name,
      base::TimeTicks::Now() - phishing_detection_start_time_);
  base::UmaHistogramEnumeration("SBClientPhishing.PhishingDetectorResult",
                                result);
  base::UmaHistogramEnumeration(
      "SBClientPhishing.PhishingDetectorResult." + request_type_name, result);

  if (feature_cache_map &&
      base::FeatureList::IsEnabled(
          kClientSideDetectionDebuggingMetadataCache) &&
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
    feature_cache_map->GetOrCreateDebuggingMetadataForURL(current_url_)
        ->set_phishing_detector_result(GetPhishingDetectorResult(result));
  }

  if (result == mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY) {
    bool is_model_available = csd_service_->IsModelAvailable();
    base::UmaHistogramBoolean(
        "SBClientPhishing.BrowserReadyOnClassifierNotReady",
        is_model_available);
  } else if (feature_cache_map &&
             base::FeatureList::IsEnabled(
                 kClientSideDetectionDebuggingMetadataCache) &&
             IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
    // We should only add this if the classifier is ready, because then we have
    // the trigger model version in the model class.
    feature_cache_map->GetOrCreateDebuggingMetadataForURL(current_url_)
        ->set_csd_model_version(csd_service_->GetTriggerModelVersion());
  }

  if (result != mojom::PhishingDetectorResult::SUCCESS) {
    return;
  }

  // We parse the protocol buffer here.  If we're unable to parse it or it was
  // not provided we won't send the verdict further.
  std::optional<ClientPhishingRequest> verdict;
  if (wrapped_verdict.has_value()) {
    verdict = wrapped_verdict->As<ClientPhishingRequest>();
  }
  base::UmaHistogramBoolean("SBClientPhishing.VerdictParseSuccessful",
                            verdict.has_value());
  if (csd_service_ && verdict.has_value()) {
    verdict->set_client_side_detection_type(request_type);

    // We should only cache the verdict string if the result is SUCCESS, so that
    // in a situation where it is not, PG can retry the classification
    // because classifier can be ready or a new model is ready to address
    // the failure reasons.
    if (feature_cache_map) {
      // Initial implementation of the feature is that only PG will use the
      // cache to reuse the images that are computed by CSD-Phishing/PG. In
      // scenarios where the user reloads the page, we could use the images
      // again, and we will log to see the efficiency if we were to.
      bool cache_csd_phishing_data_available =
          feature_cache_map->GetVerdictForURL(current_url_) != nullptr;

      base::UmaHistogramBoolean(
          "SBClientPhishing.CSDPhishingCachedDataAvailable",
          cache_csd_phishing_data_available);

      feature_cache_map->InsertVerdict(
          current_url_, std::make_unique<ClientPhishingRequest>(*verdict));
    }

    MaybeSendClientPhishingRequest(
        std::make_unique<ClientPhishingRequest>(verdict.value()));
  }
}

void ClientSideDetectionHost::MaybeSendClientPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> verdict) {
  csd_service_->ClassifyPhishingThroughThresholds(verdict.get());
  VLOG(2) << "Phishing classification score: " << verdict->client_score();
  VLOG(2) << "Visual model scores:";
  for (const ClientPhishingRequest::CategoryScore& label_and_value :
       verdict->tflite_model_scores()) {
    VLOG(2) << label_and_value.label() << ": " << label_and_value.value();
  }

  if (HasDebugFeatureDirectory()) {
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::BindOnce(&WriteFeaturesToDisk, *verdict,
                                              GetDebugFeatureDirectory()));
  }

#if BUILDFLAG(IS_ANDROID)
  gfx::Size size;
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (view) {
    gfx::SizeF viewport = view->GetNativeView()->viewport_size();
    size = gfx::Size(static_cast<int>(viewport.width()),
                     static_cast<int>(viewport.height()));
  }
  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result =
          visual_utils::CanExtractVisualFeatures(
              IsExtendedReportingEnabled(*delegate_->GetPrefs()),
              web_contents()->GetBrowserContext()->IsOffTheRecord(), size);
#else
  gfx::Size size;
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (view) {
    size = view->GetVisibleViewportSize();
  }
  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result =
          visual_utils::CanExtractVisualFeatures(
              IsExtendedReportingEnabled(*delegate_->GetPrefs()),
              web_contents()->GetBrowserContext()->IsOffTheRecord(), size,
              zoom::ZoomController::GetZoomLevelForWebContents(web_contents()));
#endif
  base::UmaHistogramEnumeration("SBClientPhishing.VisualFeaturesClearReason",
                                can_extract_visual_features_result);
  if (can_extract_visual_features_result !=
      visual_utils::CanExtractVisualFeaturesResult::kCanExtractVisualFeatures) {
    verdict->clear_visual_features();
  }

  if (IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
    delegate_->AddReferrerChain(verdict.get(), current_url_,
                                current_outermost_main_frame_id_);
  }

  base::UmaHistogramBoolean("SBClientPhishing.LocalModelDetectsPhishing",
                            verdict->is_phishing());

  raw_ptr<VerdictCacheManager> cache_manager = delegate_->GetCacheManager();

  bool force_request_from_rt_url_lookup = false;

  if (verdict->client_side_detection_type() ==
          ClientSideDetectionType::TRIGGER_MODELS &&
      cache_manager) {
    safe_browsing::ClientSideDetectionType cached_csd_type =
        cache_manager->GetCachedRealTimeUrlClientSideDetectionType(
            current_url_);
    force_request_from_rt_url_lookup =
        cached_csd_type ==
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST &&
        IsEnhancedProtectionEnabled(*delegate_->GetPrefs());
    if (force_request_from_rt_url_lookup) {
      verdict->set_client_side_detection_type(
          safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
    }
  }

  base::UmaHistogramBoolean("SBClientPhishing.RTLookupForceRequest",
                            force_request_from_rt_url_lookup);

  base::UmaHistogramExactLinear(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      verdict->client_side_detection_type(), ClientSideDetectionType_MAX + 1);

  if (base::FeatureList::IsEnabled(kClientSideDetectionImagesCache) &&
      base::FeatureList::IsEnabled(
          kClientSideDetectionDebuggingMetadataCache) &&
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
    ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
    ClientSideDetectionFeatureCache* feature_cache_map =
        ClientSideDetectionFeatureCache::FromWebContents(web_contents());
    LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
        feature_cache_map->GetOrCreateDebuggingMetadataForURL(current_url_);
    debugging_metadata->set_local_model_detects_phishing(
        verdict->is_phishing());
    debugging_metadata->set_forced_request(force_request_from_rt_url_lookup);
  }

  // We only send a phishing verdict if the verdict is phishing AND the client
  // side detection type is |TRIGGER_MODELS|. The detection type can be
  // changed to FORCE_REQUEST from a RTLookupResponse for a SBER/ESB user.
  // This can also be changed when the request is made from a notification
  // permission prompt.
  if (!verdict->is_phishing() && verdict->client_side_detection_type() ==
                                     ClientSideDetectionType::TRIGGER_MODELS) {
    return;
  }

  // Fill in metadata about which model we used.
  *verdict->mutable_population() = delegate_->GetUserPopulation();

  if (cache_manager) {
    ChromeUserPopulation::PageLoadToken token =
        cache_manager->GetPageLoadToken(current_url_);
    // It's possible that the token is not found because real time URL check
    // is not performed for this navigation. Create a new page load token in
    // this case.
    if (!token.has_token_value()) {
      token = cache_manager->CreatePageLoadToken(current_url_);
    }
    verdict->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
        &token);
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionModelImageEmbedder) &&
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) &&
      csd_service_->HasImageEmbeddingModel() &&
      csd_service_->IsModelMetadataImageEmbeddingVersionMatching()) {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();

    phishing_image_embedder_.reset();
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(
        &phishing_image_embedder_);

    if (phishing_image_embedder_.is_bound()) {
      phishing_image_embedder_->StartImageEmbedding(
          current_url_,
          base::BindOnce(&ClientSideDetectionHost::PhishingImageEmbeddingDone,
                         weak_factory_.GetWeakPtr(), std::move(verdict)));
    }
  } else {
    if (CanGetAccessToken()) {
      token_fetcher_->Start(
          base::BindOnce(&ClientSideDetectionHost::OnGotAccessToken,
                         weak_factory_.GetWeakPtr(), std::move(verdict)));
      return;
    }

    std::string empty_access_token;
    SendRequest(std::move(verdict), empty_access_token);
  }
}

void ClientSideDetectionHost::PhishingImageEmbeddingDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    mojom::PhishingImageEmbeddingResult result,
    std::optional<mojo_base::ProtoWrapper> image_feature_embedding) {
  base::UmaHistogramEnumeration("SBClientPhishing.PhishingImageEmbeddingResult",
                                result);
  if (result == mojom::PhishingImageEmbeddingResult::kSuccess) {
    std::optional<ImageFeatureEmbedding> embedding;
    if (image_feature_embedding.has_value()) {
      embedding = image_feature_embedding->As<ImageFeatureEmbedding>();
    }
    if (embedding.has_value()) {
      *verdict->mutable_image_feature_embedding() =
          std::move(embedding.value());
    } else {
      VLOG(0) << "Failed to parse image feature embedding.";
    }
  }

  if (CanGetAccessToken()) {
    token_fetcher_->Start(
        base::BindOnce(&ClientSideDetectionHost::OnGotAccessToken,
                       weak_factory_.GetWeakPtr(), std::move(verdict)));
    return;
  }

  std::string empty_access_token;
  SendRequest(std::move(verdict), empty_access_token);
}

void ClientSideDetectionHost::MaybeShowPhishingWarning(
    bool is_from_cache,
    ClientSideDetectionType request_type,
    GURL phishing_url,
    bool is_phishing,
    std::optional<net::HttpStatusCode> response_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!is_from_cache) {
    base::UmaHistogramBoolean("SBClientPhishing.ServerModelDetectsPhishing",
                              is_phishing);
    std::string request_type_name = GetRequestTypeName(request_type);
    base::UmaHistogramBoolean(
        "SBClientPhishing.ServerModelDetectsPhishing." + request_type_name,
        is_phishing);
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionImagesCache) &&
      base::FeatureList::IsEnabled(
          kClientSideDetectionDebuggingMetadataCache) &&
      IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) &&
      response_code.has_value()) {
    ClientSideDetectionFeatureCache::CreateForWebContents(web_contents());
    ClientSideDetectionFeatureCache* feature_cache_map =
        ClientSideDetectionFeatureCache::FromWebContents(web_contents());
    feature_cache_map->GetOrCreateDebuggingMetadataForURL(phishing_url)
        ->set_network_result(response_code.value());
  }

  if (is_phishing) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      auto* primary_main_frame = web_contents()->GetPrimaryMainFrame();
      const content::GlobalRenderFrameHostId primary_main_frame_id =
          primary_main_frame->GetGlobalId();

      security_interstitials::UnsafeResource resource;
      resource.url = phishing_url;
      resource.original_url = phishing_url;
      resource.is_subresource = false;
      resource.threat_type =
          SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
      resource.threat_source =
          safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
      resource.render_process_id = primary_main_frame_id.child_id;
      resource.render_frame_token = primary_main_frame->GetFrameToken().value();
      if (!ui_manager_->IsAllowlisted(resource)) {
        // We need to stop any pending navigations, otherwise the interstitial
        // might not get created properly.
        web_contents()->GetController().DiscardNonCommittedEntries();
      }
      ui_manager_->DisplayBlockingPage(resource);
    }
    // If there is true phishing verdict, invalidate weakptr so that no longer
    // consider the malware vedict.
    weak_factory_.InvalidateWeakPtrs();
  }
}

void ClientSideDetectionHost::set_client_side_detection_service(
    base::WeakPtr<ClientSideDetectionService> service) {
  csd_service_ = service;
}

void ClientSideDetectionHost::set_ui_manager(BaseUIManager* ui_manager) {
  ui_manager_ = ui_manager;
}

void ClientSideDetectionHost::set_database_manager(
    SafeBrowsingDatabaseManager* database_manager) {
  database_manager_ = database_manager;
}

void ClientSideDetectionHost::OnGotAccessToken(
    std::unique_ptr<ClientPhishingRequest> verdict,
    const std::string& access_token) {
  ClientSideDetectionHost::SendRequest(std::move(verdict), access_token);
}

bool ClientSideDetectionHost::CanGetAccessToken() {
  if (is_off_the_record_) {
    return false;
  }

  // Return true if the primary user account of an ESB user is signed in.
  return IsEnhancedProtectionEnabled(*pref_service_) &&
         !account_signed_in_callback_.is_null() &&
         account_signed_in_callback_.Run();
}

void ClientSideDetectionHost::SendRequest(
    std::unique_ptr<ClientPhishingRequest> verdict,
    const std::string& access_token) {
  ClientSideDetectionService::ClientReportPhishingRequestCallback callback =
      base::BindOnce(&ClientSideDetectionHost::MaybeShowPhishingWarning,
                     weak_factory_.GetWeakPtr(),
                     /*is_from_cache=*/false,
                     verdict->client_side_detection_type());
  csd_service_->SendClientReportPhishingRequest(
      std::move(verdict), std::move(callback), access_token);
}

}  // namespace safe_browsing
