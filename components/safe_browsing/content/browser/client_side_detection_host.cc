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
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model_optimization_guide.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/common/visual_utils.h"
#include "components/safe_browsing/core/browser/db/allowlist_checker_client.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
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
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"

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

void WriteFeaturesToDisk(const ClientPhishingRequest& features,
                         const base::FilePath& base_path) {
  base::FilePath path =
      base_path.AppendASCII(base::Uuid::GenerateRandomV4().AsLowercaseString());
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return;
  std::string serialized_features = features.SerializeAsString();
  file.WriteAtCurrentPos(serialized_features.data(),
                         serialized_features.size());
}

bool HasDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kCsdDebugFeatureDirectoryFlag);
}

base::FilePath GetDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      kCsdDebugFeatureDirectoryFlag);
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
      content::NavigationHandle* navigation_handle,
      ShouldClassifyUrlCallback start_phishing_classification,
      WebContents* web_contents,
      base::WeakPtr<ClientSideDetectionService> csd_service,
      SafeBrowsingDatabaseManager* database_manager,
      base::WeakPtr<ClientSideDetectionHost> host)
      : web_contents_(web_contents),
        csd_service_(csd_service),
        database_manager_(database_manager),
        host_(host),
        start_phishing_classification_cb_(
            std::move(start_phishing_classification)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(web_contents_);
    DCHECK(csd_service_);
    DCHECK(database_manager_.get());
    DCHECK(host_);
    url_ = navigation_handle->GetURL();
    if (navigation_handle->GetResponseHeaders())
      navigation_handle->GetResponseHeaders()->GetMimeType(&mime_type_);
    remote_endpoint_ = navigation_handle->GetSocketAddress();
  }

  ShouldClassifyUrlRequest(const ShouldClassifyUrlRequest&) = delete;
  ShouldClassifyUrlRequest& operator=(const ShouldClassifyUrlRequest&) = delete;

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // We start by doing some simple checks that can run on the UI thread.
    base::UmaHistogramBoolean("SBClientPhishing.ClassificationStart", true);

    if (url_.SchemeIs(content::kChromeUIScheme)) {
      DontClassifyForPhishing(NO_CLASSIFY_CHROME_UI_PAGE);
    }

    if (csd_service_ &&
        csd_service_->IsLocalResource(remote_endpoint_.address())) {
      DontClassifyForPhishing(NO_CLASSIFY_LOCAL_RESOURCE);
    }

    // Only classify [X]HTML documents.
    if (mime_type_ != "text/html" && mime_type_ != "application/xhtml+xml") {
      DontClassifyForPhishing(NO_CLASSIFY_UNSUPPORTED_MIME_TYPE);
    }

    if (csd_service_ &&
        csd_service_->IsPrivateIPAddress(remote_endpoint_.address())) {
      DontClassifyForPhishing(NO_CLASSIFY_PRIVATE_IP);
    }

    // For phishing we only classify HTTP or HTTPS pages.
    if (!url_.SchemeIsHTTPOrHTTPS()) {
      DontClassifyForPhishing(NO_CLASSIFY_SCHEME_NOT_SUPPORTED);
    }

    // Don't run any classifier if the tab is incognito.
    if (web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      DontClassifyForPhishing(NO_CLASSIFY_OFF_THE_RECORD);
    }

    // Don't start classification if |url_| is allowlisted by enterprise policy.
    if (host_ && host_->delegate_->GetPrefs() &&
        IsURLAllowlistedByPolicy(url_, *host_->delegate_->GetPrefs())) {
      DontClassifyForPhishing(NO_CLASSIFY_ALLOWLISTED_BY_POLICY);
    }

    // Don't start classification if CSD-Phishing is not allowed by enterprise
    // policy.
    if (host_ && host_->delegate_->GetPrefs() &&
        !IsCsdPhishingProtectionAllowed(*host_->delegate_->GetPrefs())) {
      DontClassifyForPhishing(NO_CLASSIFY_NOT_ALLOWED_BY_POLICY);
    }

    // If the tab has a delayed warning, ignore this second verdict. We don't
    // want to immediately undelay a page that's already blocked as phishy.
    if (host_ && host_->delegate_->HasSafeBrowsingUserInteractionObserver()) {
      DontClassifyForPhishing(NO_CLASSIFY_HAS_DELAYED_WARNING);
    }

    // We lookup the csd-allowlist before we lookup the cache because
    // a URL may have recently been allowlisted.  If the URL matches
    // the csd-allowlist we won't start phishing classification.  The
    // csd-allowlist check has to be done on the IO thread because it
    // uses the SafeBrowsing service class.
    if (ShouldClassifyForPhishing()) {
      if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
        CheckSafeBrowsingDatabase(url_);
      } else {
        content::GetIOThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&ShouldClassifyUrlRequest::CheckSafeBrowsingDatabase,
                           this, url_));
      }
    }
  }

  void Cancel() {
    DontClassifyForPhishing(NO_CLASSIFY_CANCEL);
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

  // Enum used to keep stats about why the pre-classification check failed.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum PreClassificationCheckResult {
    OBSOLETE_NO_CLASSIFY_PROXY_FETCH = 0,
    NO_CLASSIFY_PRIVATE_IP = 1,
    NO_CLASSIFY_OFF_THE_RECORD = 2,
    NO_CLASSIFY_MATCH_CSD_ALLOWLIST = 3,
    NO_CLASSIFY_TOO_MANY_REPORTS = 4,
    NO_CLASSIFY_UNSUPPORTED_MIME_TYPE = 5,
    NO_CLASSIFY_NO_DATABASE_MANAGER = 6,
    NO_CLASSIFY_KILLSWITCH = 7,
    NO_CLASSIFY_CANCEL = 8,
    NO_CLASSIFY_RESULT_FROM_CACHE = 9,
    DEPRECATED_NO_CLASSIFY_NOT_HTTP_URL = 10,
    NO_CLASSIFY_SCHEME_NOT_SUPPORTED = 11,
    NO_CLASSIFY_ALLOWLISTED_BY_POLICY = 12,
    CLASSIFY = 13,
    NO_CLASSIFY_HAS_DELAYED_WARNING = 14,
    NO_CLASSIFY_LOCAL_RESOURCE = 15,
    NO_CLASSIFY_CHROME_UI_PAGE = 16,
    NO_CLASSIFY_NOT_ALLOWED_BY_POLICY = 17,

    NO_CLASSIFY_MAX  // Always add new values before this one.
  };

  // The destructor can be called either from the UI or the IO thread.
  virtual ~ShouldClassifyUrlRequest() = default;

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
          NO_CLASSIFY_MAX);
      std::move(start_phishing_classification_cb_).Run(false);
    }
    start_phishing_classification_cb_.Reset();
  }

  void CheckSafeBrowsingDatabase(const GURL& url) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    PreClassificationCheckResult phishing_reason = NO_CLASSIFY_MAX;

    // When doing debug feature dumps, ignore the allowlist.
    if (HasDebugFeatureDirectory()) {
      OnAllowlistCheckDoneOnIO(url, NO_CLASSIFY_MAX,
                               /*match_allowlist=*/false);
      return;
    }

    if (!database_manager_.get()) {
      // We cannot check the Safe Browsing allowlists so we stop here
      // for safety.
      OnAllowlistCheckDoneOnIO(url, NO_CLASSIFY_NO_DATABASE_MANAGER,
                               /*match_allowlist=*/false);
      return;
    }

    // Query the CSD Allowlist asynchronously. We're already on the IO thread so
    // can call AllowlistCheckerClient directly.
    base::OnceCallback<void(bool)> result_callback =
        base::BindOnce(&ClientSideDetectionHost::ShouldClassifyUrlRequest::
                           OnAllowlistCheckDoneOnIO,
                       this, url, phishing_reason);
    AllowlistCheckerClient::StartCheckCsdAllowlist(database_manager_, url,
                                                   std::move(result_callback));
  }

  void OnAllowlistCheckDoneOnIO(const GURL& url,
                                PreClassificationCheckResult phishing_reason,
                                bool match_allowlist) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    // We don't want to call the classification callbacks from the IO
    // thread so we simply pass the results of this method to CheckCache()
    // which is called on the UI thread;
    if (match_allowlist) {
      phishing_reason = NO_CLASSIFY_MATCH_CSD_ALLOWLIST;
    }
    if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
      CheckCache(phishing_reason);
    } else {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&ShouldClassifyUrlRequest::CheckCache, this,
                                    phishing_reason));
    }
  }

  void CheckCache(PreClassificationCheckResult phishing_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (phishing_reason != NO_CLASSIFY_MAX)
      DontClassifyForPhishing(phishing_reason);
    if (!ShouldClassifyForPhishing()) {
      return;  // No point in doing anything else.
    }
    // If result is cached, we don't want to run classification again.
    // In that case we're just trying to show the warning.
    // If we're dumping features for debugging, ignore the cache.
    bool is_phishing;
    if (!HasDebugFeatureDirectory() && host_ && csd_service_ &&
        csd_service_->GetValidCachedResult(url_, &is_phishing)) {
      // Since we are already on the UI thread, this is safe.
      host_->MaybeShowPhishingWarning(/*is_from_cache=*/true, url_,
                                      is_phishing);
      DontClassifyForPhishing(NO_CLASSIFY_RESULT_FROM_CACHE);
    }

    // We want to limit the number of requests, though we will ignore the
    // limit for urls in the cache.  We don't want to start classifying
    // too many pages as phishing, but for those that we already think are
    // phishing we want to send a request to the server to give ourselves
    // a chance to fix misclassifications.
    // If we're dumping features for debugging, allow us to exceed the report
    // limit.
    if (!HasDebugFeatureDirectory() && csd_service_ &&
        !csd_service_->IsInCache(url_) &&
        csd_service_->OverPhishingReportLimit()) {
      DontClassifyForPhishing(NO_CLASSIFY_TOO_MANY_REPORTS);
    }

    // Everything checks out, so start classification.
    // |web_contents_| is safe to call as we will be destructed
    // before it is.
    if (ShouldClassifyForPhishing()) {
      base::UmaHistogramEnumeration(
          "SBClientPhishing.PreClassificationCheckResult", CLASSIFY,
          NO_CLASSIFY_MAX);
      std::move(start_phishing_classification_cb_).Run(true);
      // Reset the callback to make sure ShouldClassifyForPhishing()
      // returns false.
      start_phishing_classification_cb_.Reset();
    }
  }

  GURL url_;
  std::string mime_type_;
  net::IPEndPoint remote_endpoint_;
  raw_ptr<WebContents> web_contents_;
  base::WeakPtr<ClientSideDetectionService> csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
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

  // |ui_manager_| and |database_manager_| can
  // be null if safe browsing service is not available in the embedder.
  ui_manager_ = delegate_->GetSafeBrowsingUIManager();
  database_manager_ = delegate_->GetSafeBrowsingDBManager();
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
}

void ClientSideDetectionHost::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch))
    return;

  // TODO(noelutz): move this DCHECK to WebContents and fix all the unit tests
  // that don't call this method on the UI thread.
  // DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (navigation_handle->IsSameDocument()) {
    // If the navigation is within the same document, the user isn't really
    // navigating away.  We don't need to cancel a pending callback or
    // begin a new classification.
    return;
  }
  // Cancel any pending classification request.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  // If we navigate away and there currently is a pending phishing
  // report request we have to cancel it to make sure we don't display
  // an interstitial for the wrong page.  Note that this won't cancel
  // the server ping back but only cancel the showing of the
  // interstitial.
  weak_factory_.InvalidateWeakPtrs();

  if (!csd_service_) {
    return;
  }

  current_url_ = navigation_handle->GetURL();
  current_outermost_main_frame_id_ = navigation_handle->GetRenderFrameHost()
                                         ->GetOutermostMainFrame()
                                         ->GetGlobalId();

  // Check whether we can cassify the current URL for phishing.
  classification_request_ = new ShouldClassifyUrlRequest(
      navigation_handle,
      base::BindOnce(&ClientSideDetectionHost::OnPhishingPreClassificationDone,
                     weak_factory_.GetWeakPtr()),
      web_contents(), csd_service_, database_manager_.get(),
      weak_factory_.GetWeakPtr());
  classification_request_->Start();
}

void ClientSideDetectionHost::OnPhishingPreClassificationDone(
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
                         weak_factory_.GetWeakPtr()));
    }
  }
}

void ClientSideDetectionHost::PhishingDetectionDone(
    mojom::PhishingDetectorResult result,
    const std::string& verdict_str) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(csd_service_);

  phishing_detector_.reset();

  UmaHistogramMediumTimes(
      "SBClientPhishing.PhishingDetectionDuration",
      base::TimeTicks::Now() - phishing_detection_start_time_);
  base::UmaHistogramEnumeration("SBClientPhishing.PhishingDetectorResult",
                                result);
  if (result == mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY) {
    bool isModelAvailable =
        base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
            ? csd_service_->IsModelAvailable()
            : ClientSidePhishingModel::GetInstance()->IsEnabled();
    base::UmaHistogramBoolean(
        "SBClientPhishing.BrowserReadyOnClassifierNotReady", isModelAvailable);
  }
  if (result != mojom::PhishingDetectorResult::SUCCESS)
    return;

  // We parse the protocol buffer here.  If we're unable to parse it we won't
  // send the verdict further.
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  if (csd_service_ && verdict->ParseFromString(verdict_str) &&
      verdict->IsInitialized()) {
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
    bool can_extract_visual_features = visual_utils::CanExtractVisualFeatures(
        IsExtendedReportingEnabled(*delegate_->GetPrefs()),
        web_contents()->GetBrowserContext()->IsOffTheRecord(), size);
#else
    gfx::Size size;
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderWidgetHostView();
    if (view) {
      size = view->GetVisibleViewportSize();
    }
    bool can_extract_visual_features = visual_utils::CanExtractVisualFeatures(
        IsExtendedReportingEnabled(*delegate_->GetPrefs()),
        web_contents()->GetBrowserContext()->IsOffTheRecord(), size,
        zoom::ZoomController::GetZoomLevelForWebContents(web_contents()));
#endif
    if (!can_extract_visual_features) {
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

    if (base::FeatureList::IsEnabled(kClientSideDetectionTypeForceRequest)) {
      if (cache_manager) {
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
    }

    // We only send a phishing verdict if the verdict is phishing OR we get a
    // FORCE_REQUEST from a RTLookupResponse for a SBER/ESB user.
    if (!verdict->is_phishing() && !force_request_from_rt_url_lookup) {
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

void ClientSideDetectionHost::MaybeShowPhishingWarning(bool is_from_cache,
                                                       GURL phishing_url,
                                                       bool is_phishing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!is_from_cache) {
    base::UmaHistogramBoolean("SBClientPhishing.ServerModelDetectsPhishing",
                              is_phishing);
  }

  if (is_phishing) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      const content::GlobalRenderFrameHostId primary_main_frame_id =
          web_contents()->GetPrimaryMainFrame()->GetGlobalId();

      security_interstitials::UnsafeResource resource;
      resource.url = phishing_url;
      resource.original_url = phishing_url;
      resource.is_subresource = false;
      resource.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
      resource.threat_source =
          safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
      resource.render_process_id = primary_main_frame_id.child_id;
      resource.render_frame_id = primary_main_frame_id.frame_routing_id;
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
  if (is_off_the_record_)
    return false;

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
                     /*is_from_cache=*/false);
  csd_service_->SendClientReportPhishingRequest(
      std::move(verdict), std::move(callback), access_token);
}

}  // namespace safe_browsing
