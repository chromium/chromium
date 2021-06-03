// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_host.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/db/allowlist_checker_client.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::WebContents;

namespace safe_browsing {

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
      ClientSideDetectionService* csd_service,
      SafeBrowsingDatabaseManager* database_manager,
      ClientSideDetectionHost* host)
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

  void Start() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // We start by doing some simple checks that can run on the UI thread.
    base::UmaHistogramBoolean("SBClientPhishing.ClassificationStart", true);

    // Only classify [X]HTML documents.
    if (mime_type_ != "text/html" && mime_type_ != "application/xhtml+xml") {
      DontClassifyForPhishing(NO_CLASSIFY_UNSUPPORTED_MIME_TYPE);
    }

    if (csd_service_->IsPrivateIPAddress(
            remote_endpoint_.ToStringWithoutPort())) {
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
    if (host_->delegate_->GetPrefs() &&
        IsURLAllowlistedByPolicy(url_, *host_->delegate_->GetPrefs())) {
      DontClassifyForPhishing(NO_CLASSIFY_ALLOWLISTED_BY_POLICY);
    }

    // If the tab has a delayed warning, ignore this second verdict. We don't
    // want to immediately undelay a page that's already blocked as phishy.
    if (host_->delegate_->HasSafeBrowsingUserInteractionObserver()) {
      DontClassifyForPhishing(NO_CLASSIFY_HAS_DELAYED_WARNING);
    }

    // We lookup the csd-allowlist before we lookup the cache because
    // a URL may have recently been allowlisted.  If the URL matches
    // the csd-allowlist we won't start phishing classification.  The
    // csd-allowlist check has to be done on the IO thread because it
    // uses the SafeBrowsing service class.
    if (ShouldClassifyForPhishing()) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ShouldClassifyUrlRequest::CheckSafeBrowsingDatabase,
                         this, url_));
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
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    PreClassificationCheckResult phishing_reason = NO_CLASSIFY_MAX;
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
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // We don't want to call the classification callbacks from the IO
    // thread so we simply pass the results of this method to CheckCache()
    // which is called on the UI thread;
    if (match_allowlist) {
      phishing_reason = NO_CLASSIFY_MATCH_CSD_ALLOWLIST;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ShouldClassifyUrlRequest::CheckCache, this,
                                  phishing_reason));
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
    bool is_phishing;
    if (csd_service_->GetValidCachedResult(url_, &is_phishing)) {
      base::UmaHistogramBoolean("SBClientPhishing.RequestSatisfiedFromCache",
                                true);
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
    if (!csd_service_->IsInCache(url_) &&
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
  WebContents* web_contents_;
  ClientSideDetectionService* csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  ClientSideDetectionHost* host_;

  ShouldClassifyUrlCallback start_phishing_classification_cb_;

  DISALLOW_COPY_AND_ASSIGN(ShouldClassifyUrlRequest);
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
  if (csd_service_)
    csd_service_->RemoveClientSideDetectionHost(this);
}

void ClientSideDetectionHost::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
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

  // Check whether we can cassify the current URL for phishing.
  classification_request_ = new ShouldClassifyUrlRequest(
      navigation_handle,
      base::BindOnce(&ClientSideDetectionHost::OnPhishingPreClassificationDone,
                     weak_factory_.GetWeakPtr()),
      web_contents(), csd_service_, database_manager_.get(), this);
  classification_request_->Start();
}

void ClientSideDetectionHost::SendModelToRenderFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents() || web_contents() != tab_ || !csd_service_)
    return;

  for (content::RenderFrameHost* frame : web_contents()->GetAllFrames()) {
    if (!frame->IsRenderFrameCreated())
      continue;  // We'd send to this frame on RenderFrameCreated().
    if (phishing_detector_)
      phishing_detector_.reset();
    frame->GetRemoteInterfaces()->GetInterface(
        phishing_detector_.BindNewPipeAndPassReceiver());
    phishing_detector_->SetPhishingModel(csd_service_->GetModelStr());
  }
}

void ClientSideDetectionHost::WebContentsDestroyed() {
  // Tell any pending classification request that it is being canceled.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  if (csd_service_)
    csd_service_->RemoveClientSideDetectionHost(this);
}

void ClientSideDetectionHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (phishing_detector_)
    phishing_detector_.reset();
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      phishing_detector_.BindNewPipeAndPassReceiver());
  phishing_detector_->SetPhishingModel(csd_service_->GetModelStr());
}

void ClientSideDetectionHost::OnPhishingPreClassificationDone(
    bool should_classify) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (should_classify) {
    content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
    phishing_detector_.reset();
    rfh->GetRemoteInterfaces()->GetInterface(
        phishing_detector_.BindNewPipeAndPassReceiver());
    phishing_detection_start_time_ = tick_clock_->NowTicks();
    phishing_detector_->StartPhishingDetection(
        current_url_,
        base::BindOnce(&ClientSideDetectionHost::PhishingDetectionDone,
                       weak_factory_.GetWeakPtr()));
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

  UmaHistogramMediumTimes(
      "SBClientPhishing.PhishingDetectionDuration",
      base::TimeTicks::Now() - phishing_detection_start_time_);
  base::UmaHistogramEnumeration("SBClientPhishing.PhishingDetectorResult",
                                result);
  if (result == mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY) {
    base::UmaHistogramEnumeration("SBClientPhishing.ClassifierNotReadyReason",
                                  csd_service_->GetLastModelStatus());
  }
  if (result != mojom::PhishingDetectorResult::SUCCESS)
    return;

  // We parse the protocol buffer here.  If we're unable to parse it we won't
  // send the verdict further.
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  if (csd_service_ && verdict->ParseFromString(verdict_str) &&
      verdict->IsInitialized()) {
    VLOG(2) << "Phishing classification score: " << verdict->client_score();
    for (auto& match : verdict->vision_match()) {
      VLOG(2) << "Target Digest: " << match.matched_target_digest();
      VLOG(2) << "Phash Score: " << match.vision_matched_phash_score();
      VLOG(2) << "EMD Score: " << match.vision_matched_emd_score();
    }
    if (!IsExtendedReportingEnabled(*delegate_->GetPrefs()) &&
        !IsEnhancedProtectionEnabled(*delegate_->GetPrefs())) {
      // These fields should only be set for SBER users.
      verdict->clear_screenshot_digest();
      verdict->clear_screenshot_phash();
      verdict->clear_phash_dimension_size();
    }

    if (IsEnhancedProtectionEnabled(*delegate_->GetPrefs()) &&
        base::FeatureList::IsEnabled(kClientSideDetectionReferrerChain)) {
      delegate_->AddReferrerChain(verdict.get(), current_url_);
    }

    base::UmaHistogramBoolean("SBClientPhishing.LocalModelDetectsPhishing",
                              verdict->is_phishing());

    // We only send phishing verdict to the server if the verdict is phishing.
    if (!verdict->is_phishing())
      return;

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
  if (is_from_cache) {
    base::UmaHistogramBoolean("SBClientPhishing.CacheDetectsPhishing",
                              is_phishing);
  } else {
    base::UmaHistogramBoolean("SBClientPhishing.ServerModelDetectsPhishing",
                              is_phishing);
  }

  if (is_phishing) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      security_interstitials::UnsafeResource resource;
      resource.url = phishing_url;
      resource.original_url = phishing_url;
      resource.is_subresource = false;
      resource.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
      resource.threat_source =
          safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
      resource.web_contents_getter =
          security_interstitials::GetWebContentsGetter(
              web_contents()->GetMainFrame()->GetProcess()->GetID(),
              web_contents()->GetMainFrame()->GetRoutingID());
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
    ClientSideDetectionService* service) {
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

  // Return true if the finch feature is enabled for an ESB user, and if the
  // primary user account is signed in.
  return base::FeatureList::IsEnabled(kClientSideDetectionWithToken) &&
         IsEnhancedProtectionEnabled(*pref_service_) &&
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
