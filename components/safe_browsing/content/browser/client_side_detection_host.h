// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_HOST_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_HOST_H_

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/content/browser/url_checker/async_check_tracker.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/client_side_detection/client_side_detection_host_base.h"
#include "components/safe_browsing/core/browser/client_side_detection/credit_card_form_event.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/common/visual_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/core/browser/referring_app_info/referring_app_info.h"  // nogncheck
#endif

class PrefService;
class SkBitmap;

namespace history {
class HistoryService;
}

namespace safe_browsing {
class BaseUIManager;
class ClientPhishingRequest;
class ClientSideDetectionService;
class SafeBrowsingDatabaseManager;
class SafeBrowsingTokenFetcher;
class VerdictCacheManager;

// This class is used to receive the IPC from the renderer which
// notifies the browser that a URL was classified as phishing.  This
// class relays this information to the client-side detection service
// class which sends a ping to a server to validate the verdict.
class ClientSideDetectionHost
    : public ClientSideDetectionHostBase,
      public content::WebContentsObserver,
      public permissions::PermissionRequestManager::Observer,
      public AsyncCheckTracker::Observer {
 public:
  using AsyncCheckTriggerForceRequestResult =
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult;

  // ClientSideDetectionHostBase overrides:
  GURL GetCurrentUrl() const override;
  ClientSideDetectionFeatureCacheBase* GetFeatureCache() override;
  std::vector<GURL> GetRedirectChain() override;
  safe_browsing::credit_card_form::ReferringApp GetReferringApp()
      const override;
  ChromeUserPopulation GetUserPopulation() override;
  bool IsAccountSignedIn() override;
  bool IsErrorDocument() override;
  std::optional<double> GetSiteEngagementScore(const GURL& url) const override;
  void GetInnerText(HostInnerTextCallback callback) override;
  std::string GetPageTitle() const override;
  void MaybeStartImageEmbedding(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      PhishingDetectorResult result) override;
  void MaybeRunUserReportCallback() override;
  void MaybeStartGeminiAntiscamProtection(
      GURL url,
      ClientSideDetectionType request_type,
      std::optional<bool> did_match_high_confidence_allowlist) override;
  void MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType request_type) override;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CSDObserverCalled {
    kOnFirstContentfulPaint = 0,
    kDidFirstVisuallyNonEmptyPaint = 1,
    kMaxValue = kDidFirstVisuallyNonEmptyPaint,
  };

  // Visibility of the WebContents when a classification is dispatched. Mirrors
  // `content::Visibility`, which cannot be recorded to UMA directly. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  //
  // LINT.IfChange(ClassificationVisibility)
  enum class ClassificationVisibility {
    kHidden = 0,
    kOccluded = 1,
    kVisible = 2,
    kMaxValue = kVisible,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingClassificationVisibility)

  // A callback via which the client of this component indicates whether the
  // primary account is signed in.
  using PrimaryAccountSignedIn = base::RepeatingCallback<bool()>;

  // Callback for when preclassification is started.
  using PreclassificationStarted =
      base::RepeatingCallback<void(ClientSideDetectionType)>;

  // Outcome of a browser-side page capture
  // (`kClientSideDetectionCopyFromSurface`). These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  //
  // LINT.IfChange(BrowserCaptureResult)
  enum class BrowserCaptureResult {
    kSuccess = 0,
    // The WebContents has no RenderWidgetHostView.
    kViewMissing = 1,
    // The view is visible but has no compositing surface to copy.
    kSurfaceUnavailable = 2,
    // `CopyFromSurface()` reported an error (see
    // SBClientPhishing.BrowserCaptureCopyFromSurfaceError).
    kCopyFailed = 3,
    // `CopyFromSurface()` succeeded but produced an empty bitmap.
    kEmptyBitmap = 4,
    // `CopyFromSurface()` did not run its callback within the timeout.
    kTimedOut = 5,
    // A newer capture request replaced this one before it completed.
    kSuperseded = 6,
    kMaxValue = kSuperseded,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingBrowserCaptureResult)

  // Wraps the `RenderWidgetHostView` calls used by the browser-side page
  // capture so that tests can fake them: the test view never has a compositing
  // surface.
  class VisualCapturer {
   public:
    using CopyCallback =
        base::OnceCallback<void(const content::CopyFromSurfaceResult&)>;

    virtual ~VisualCapturer() = default;

    // Whether `web_contents` has a view whose surface can be copied right now.
    virtual bool IsSurfaceAvailableForCopy(content::WebContents* web_contents);

    // Copies the view's surface, scaled to the view's size in
    // Device-Independent Pixels (DIPs), and runs `callback` on the calling
    // sequence.
    virtual void CopyFromSurface(content::WebContents* web_contents,
                                 base::TimeDelta timeout,
                                 CopyCallback callback);
  };

  // Delegate which allows to provide embedder specific implementations.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns whether there is a SafeBrowsingUserInteractionObserver available.
    virtual bool HasSafeBrowsingUserInteractionObserver() = 0;
    virtual scoped_refptr<SafeBrowsingDatabaseManager>
    GetSafeBrowsingDBManager() = 0;
    virtual scoped_refptr<BaseUIManager> GetSafeBrowsingUIManager() = 0;
    virtual void AddReferrerChain(ClientPhishingRequest* verdict,
                                  GURL current_url,
                                  const content::GlobalRenderFrameHostId&
                                      current_outermost_main_frame_id) = 0;
    // Returns the management status for current profile.
    virtual ChromeUserPopulation GetUserPopulation() = 0;
    // Returns the inner text from the tab, which is combined inner-text of all
    // suitable iframes . The callback is used to retrieve a string back from
    // the delegate when the inner text function is completed. This string is
    // then used to provide the intelligent scan delegate the information about
    // the page.
    virtual void GetInnerText(HostInnerTextCallback callback) = 0;
    // Triggers Gemini Antiscam Protection if conditions are met.
    virtual void MaybeStartGeminiAntiscamProtection(
        GURL url,
        ClientSideDetectionType request_type,
        std::optional<bool> did_match_high_confidence_allowlist) = 0;

#if BUILDFLAG(IS_ANDROID)
    virtual internal::ReferringAppInfo GetReferringAppInfo(
        content::WebContents* web_contents) = 0;
#endif
  };

  static const int kMaxHighResScreenshotWidth;
  static const int kMaxHighResScreenshotHeight;

  // The caller keeps ownership of the tab object and is responsible for
  // ensuring that it stays valid until WebContentsDestroyed is called.
  // The caller also keeps ownership of pref_service. The
  // ClientSideDetectionHost takes ownership of token_fetcher. is_off_the_record
  // indicates if the profile is incognito, and account_signed_in_callback is
  // checked to find out if primary account is signed in.
  static std::unique_ptr<ClientSideDetectionHost> Create(
      content::WebContents* tab,
      std::unique_ptr<Delegate> delegate,
      IntelligentScanDelegate* intelligent_scan_delegate,
      PrefService* pref_service,
      VerdictCacheManager* cache_manager,
      history::HistoryService* history_service,
      base::WeakPtr<ClientSideDetectionService> csd_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      const PrimaryAccountSignedIn& account_signed_in_callback);

  ClientSideDetectionHost(const ClientSideDetectionHost&) = delete;
  ClientSideDetectionHost& operator=(const ClientSideDetectionHost&) = delete;

  ~ClientSideDetectionHost() override;

  // From content::WebContentsObserver.  If we navigate away we cancel all
  // pending callbacks that could show an interstitial, and check to see whether
  // we should classify the new URL. If a request to lock the keyboard or
  // pointer or vibrate the page has arrived, we will re-trigger classification.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void KeyboardLockRequested() override;
  void VibrationRequested() override;
  void OnTextCopiedToClipboard(content::RenderFrameHost* render_frame_host,
                               const std::u16string& copied_text) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnFirstContentfulPaintInPrimaryMainFrame(
      base::TimeTicks presentation_time) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // permissions::PermissionRequestManager::Observer methods:
  void OnPromptAdded() override;
  void OnPermissionRequestManagerDestructed() override;

  void RegisterPermissionRequestManager();

  // AsyncCheckTracker::Observer methods:
  void OnAsyncSafeBrowsingCheckCompleted() override;
  void OnAsyncSafeBrowsingCheckTrackerDestructed() override;

  void RegisterAsyncCheckTracker();



  void RegisterAutofillManager();

  // User requests to report a site as unsafe. The screenshot values come from
  // the report dialog view.
  void ReportUnsafeSite(SkBitmap screenshot, base::OnceClosure callback);

  // Called when an unfamiliar login page is detected (e.g. via password field
  // focus).
  void OnUnfamiliarLoginPageDetected();

  // Sets a callback to be notified when preclassification is started.
  void set_preclassification_started_callback_for_testing(
      const PreclassificationStarted& callback) {
    preclassification_started_cb_for_testing_ = callback;
  }

  // Replaces the `RenderWidgetHostView` wrapper used by the browser-side page
  // capture.
  void SetVisualCapturerForTesting(std::unique_ptr<VisualCapturer> capturer);

 protected:
  explicit ClientSideDetectionHost(
      content::WebContents* tab,
      std::unique_ptr<Delegate> delegate,
      IntelligentScanDelegate* intelligent_scan_delegate,
      PrefService* pref_service,
      VerdictCacheManager* cache_manager,
      history::HistoryService* history_service,
      base::WeakPtr<ClientSideDetectionService> csd_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      const PrimaryAccountSignedIn& account_signed_in_callback);

  // Used for testing.
  void set_ui_manager(BaseUIManager* ui_manager);
  void set_database_manager(SafeBrowsingDatabaseManager* database_manager);

 private:
  friend class ClientSideDetectionHostTestBase;
  friend class ClientSideDetectionHostNotificationTest;
  friend class ClientSideDetectionHostScamDetectionTest;
  friend class ClientSideDetectionHostCreditCardFormTest;
  friend class ClientSideDetectionHostClipboardDataTest;
  friend class ClientSideDetectionHostGeminiAntiscamProtectionTest;
  friend class ClientSideDetectionHostPriorityTest;
  friend class ClientSideDetectionHostKeyboardLockUniformImageDelayTest;
  friend class ClientSideDetectionHostPrerenderBrowserTest;
  friend class ClientSideDetectionHostPrerenderBrowserTest_Screenshot;
  class ShouldClassifyUrlRequest;
  friend class ShouldClassifyUrlRequest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostPrerenderBrowserTest,
                           PrerenderShouldNotAffectClientSideDetection);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderBrowserTest,
      SamePageNavigationShouldNotAffectClientSideDetection);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostPrerenderBrowserTest,
                           ClassifyPrerenderedPageAfterActivation);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderBrowserTest,
      ClassifyPrerenderedPageAfterActivationAndCheckDebuggingMetadataCache);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderBrowserTest,
      CheckDebuggingMetadataCacheAfterClearingCacheAfterNavigation);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      KeyboardLockTriggersPreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      PointerLockTriggersPreClassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      PointerLockClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      KeyboardLockClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostTest,
                           SkipsImageEmbeddingIfAlreadyPresent);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TwoKeyboardLockRequestsOnSamePageOnlyLogsOnePreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostVibrateTest,
                           VibrationApiTriggersPreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostVibrateTest,
                           VibrationApiClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TestPreClassificationCheckMatchHighConfidenceAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlistDueToDisabledFeature);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostSkipImageClassificationScoringTest,
      NeverSkipWhenFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostSkipImageClassificationScoringTest,
      TriggerModelsDoesNotSkipWhenFeatureIsEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostSkipImageClassificationScoringTest,
      AllOtherTypesSkipWhenFeatureIsEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionRTLookupResponseForceRequestTest,
      AsyncCheckTrackerTriggersClassificationRequestOnAllowlistMatch);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostClipboardTest,
                           ClipboardApiTriggersPreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostClipboardTest,
                           ClipboardApiClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormTest,
      NonCreditCardFormDoesNotTriggerPreclassificationChecks);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormTest,
      UnclassifiedFormDoesNotTriggerPreclassificationChecks);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormTest,
      FeatureDisabledDoesNotTriggerPreclassificationChecks);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormTest,
      WhenESBDisabledDoesNotTriggerPreclassificationChecks);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormTest,
      EventDoesNotTriggerPreclassificationChecksWhenESBDisabled);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostCreditCardFormTest,
                           DoesNotStartPreclassificationOnRepeatSiteVisit);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostCreditCardFormTest,
                           IgnoresVisitsInPastTenMinutes);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostCreditCardFormTest,
                           DoesNotStartPreclassificationOnServerHeuristic);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormReferringAppTest,
      DoesNotStartPreclassificationBecauseOfReferringAppFilter);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostCreditCardFormTest,
                           PreclassificationIsDedupedByURL);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostCreditCardFormTest,
                           CreditCardFormTriggersPreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostCreditCardFormTest,
                           CreditCardFormClassificationTriggersCSDPing);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostBrowserTest,
                           NavigateTo404PageLogsErrorDocument);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostGeminiAntiscamProtectionTest,
                           GeminiAntiscamProtectionServiceCalledWithInnerText);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormTriggerDisabledTest,
      InteractionTriggerDisabledDoesNotTrigger);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormDetectionOnlyTest,
      CreditCardFormTriggersDetectionCheckWithoutInteraction);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormDetectionTriggerDisabledTest,
      DetectionTriggerDisabledDoesNotTrigger);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostCreditCardFormDetectionAndInteractionTest,
      DetectionAndInteractionTriggersOnlyTriggerOnce);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostNewObserversForceRequestTest,
                           TestTriggerModelsConvertedToForceRequestAtLoad);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostNewObserversForceRequestTest,
                           TestTriggerModelsConvertedToForceRequestAtRequest);

  // ClientSideDetectionHostBase overrides:
  void CancelPendingRequests() override;
  // Captures the primary page via `RenderWidgetHostView::CopyFromSurface()`
  // once the document has loaded and the WebContents is visible; see
  // `RequestCapture()`. Runs `callback` with an empty image on failure.
  void CaptureScreenshot(ScreenshotCallback callback) override;
  // Reports a browser-side classification that stopped before the classifier
  // ran through `PhishingDetectionDone()`, like the renderer does.
  void OnVisualClassificationEarlyReturn(
      VisualClassificationEarlyReturnReason reason) override;
  // Determines visual features extraction capabilities.
  // `can_extract_visual_features_result` will be used to handle visual features
  // in ClientPhishingRequest after.
  visual_utils::CanExtractVisualFeaturesResult
  DetermineVisualFeaturesExtraction() override;
  void ShowBlockingPage(
      GURL phishing_url,
      ClientSideDetectionType request_type,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict,
      bool is_phishing,
      bool should_show_scam_warning) override;
  void UpdateDebuggingMetadataWithNetworkResult(
      GURL phishing_url,
      net::HttpStatusCode response_code) override;
  void AddReferrerChain(ClientPhishingRequest* verdict) override;
  void MaybeFillScreenshotData(ClientPhishingRequest* request) override;
  void AddMiscellaneousMetadataToClientPhishingRequest(
      ClientPhishingRequest* verdict,
      bool is_invalid_ip) override;
  void MaybeCaptureScreenshotAndStartIntelligentScan(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      std::string inner_text) override;

  // Returns true if the observed WebContents is currently visible and its
  // committed URL matches `url`.
  bool IsTabVisibleAndOnUrl(const GURL& url) const;

  // Called when the viewport screenshot capture completes. Validates that the
  // tab is still visible and on the same URL, then posts a background task to
  // encode the bitmap as PNG bytes before starting the intelligent scan.
  void OnScreenshotTaken(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      std::string inner_text,
      base::TimeTicks capture_start_time,
      const content::CopyFromSurfaceResult& result);

  // Called when the captured screenshot has been encoded into PNG bytes on a
  // background thread. Validates that the tab is still visible and on the same
  // URL before passing the PNG bytes to `StartIntelligentScan`.
  void OnScreenshotEncoded(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      std::string inner_text,
      std::string png_bytes);

  // Called when pre-classification checks are done for the phishing
  // classifiers. |request_type| is passed in to specify the process that
  // requests the classification. |is_invalid_ip| is a temporary field to pass
  // along the result of the local resource check.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void OnPhishingPreClassificationDone(
      ClientSideDetectionType request_type,
      bool should_classify,
      bool is_sample_ping,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip);

  // Records whether the primary page's compositing surface could be copied
  // right now, and the WebContents' visibility, for a classification of
  // `request_type`. Used to size the coverage of capturing page pixels in the
  // browser process instead of the renderer (crbug.com/454896977).
  void RecordSurfaceAvailabilityForCopy(
      ClientSideDetectionType request_type) const;

  // --- Browser-side classification (kClientSideDetectionCopyFromSurface) ----

  // A classification request that is captured and scored in the browser
  // process instead of being sent to the renderer.
  struct BrowserClassificationRequest {
    GURL url;
    ClientSideDetectionType request_type =
        ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;
    bool is_sample_ping = false;
    std::optional<bool> did_match_high_confidence_allowlist;
    bool is_invalid_ip = false;
    base::TimeTicks start_time;
  };

  // Browser-side counterpart of the renderer's `StartPhishingDetection()`:
  // applies the renderer's gates (scorer ready, not a session history
  // navigation, http(s) URL, GET document), reporting failures through
  // `PhishingDetectionDone()`, then captures the page and hands the image to
  // `StartVisualClassification()`. A request still in flight is answered with
  // CANCELLED first, as the renderer does.
  void StartBrowserSideClassification(
      ClientSideDetectionType request_type,
      bool is_sample_ping,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip);
  void OnClassificationCaptureDone(const gfx::Image& image);
  // If a browser-side classification (or its capture) is in flight, cancels it
  // and reports `CLASSIFICATION_CANCELLED` while keeping CSD marked as running
  // for the incoming request.
  void CancelPendingBrowserSideClassification();
  // Answers `browser_classification_` with `result` (a failure) through
  // `PhishingDetectionDone()` and clears it.
  void FinishBrowserSideClassification(PhishingDetectorResult result);

  // Why a capture was requested; only used to pair captures with the request
  // they belong to.
  enum class CapturePurpose { kClassification, kImageEmbedding };

  enum class CaptureStage {
    // Waiting for DocumentOnLoadCompletedInPrimaryMainFrame().
    kWaitingForLoad,
    // Waiting for OnVisibilityChanged(VISIBLE).
    kWaitingForVisibility,
    // Waiting for the `CopyFromSurface()` result or the watchdog.
    kCopying,
  };

  // The page capture in progress.
  struct PendingCapture {
    PendingCapture(int capture_id,
                   CapturePurpose purpose,
                   ScreenshotCallback callback);
    PendingCapture(const PendingCapture&) = delete;
    PendingCapture& operator=(const PendingCapture&) = delete;
    ~PendingCapture();

    int capture_id;
    CapturePurpose purpose;
    ScreenshotCallback callback;
    CaptureStage stage = CaptureStage::kWaitingForLoad;
    ClientSideDetectionType request_type =
        ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;
    base::TimeTicks requested_at;
    base::TimeTicks copy_started_at;
    base::OneShotTimer watchdog_timer;
  };

  // Starts the capture pipeline. Runs `callback` with the captured image, or
  // an empty image on failure. Only one capture is outstanding at a time; a
  // new request supersedes the previous one. The pipeline waits for the
  // primary document to finish loading, waits for the WebContents to become
  // visible, and then copies the surface with `kCsdCopyFromSurfaceTimeout`
  // (plus a browser-side watchdog).
  void RequestCapture(CapturePurpose purpose, ScreenshotCallback callback);
  // Runs `callback` with an empty image for a superseded image-embedding
  // capture while keeping `is_csd_running()` set if the newer request is still
  // active.
  void RunSupersededCaptureCallback(ScreenshotCallback callback);
  // Copies the surface if the tab is visible, or parks in
  // `kWaitingForVisibility` until OnVisibilityChanged(VISIBLE).
  void ScheduleSurfaceCopy();
  void MaybeCopySurface();
  void OnCopyFromSurfaceDone(int capture_id,
                             const content::CopyFromSurfaceResult& result);
  void OnCopyFromSurfaceTimeout();
  // Completes the pending capture, recording `result`, and runs its callback
  // with `image` (empty unless `result` is kSuccess).
  void FinishCapture(BrowserCaptureResult result, const gfx::Image& image);
  void RecordCaptureResult(const PendingCapture& capture,
                           BrowserCaptureResult result) const;
  // Drops the pending capture, if any, without running its callback.
  void DropPendingCapture();

  // Note: This method has the same name as the one in the base class but
  // different parameter types (Mojo types). It converts them and calls the
  // base class method.
  void PhishingDetectionDone(
      ClientSideDetectionType request_type,
      bool is_sample_ping,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      base::TimeTicks start_time,
      mojom::PhishingDetectorResult result,
      std::optional<mojo_base::ProtoWrapper> verdict);

  // |verdict| is an encoded ClientPhishingRequest protocol message, |result| is
  // the outcome of the renderer image embedding. The verdict is passed into
  // this function after the renderer classification is finished.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void PhishingImageEmbeddingDone(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      mojom::PhishingImageEmbeddingResult result,
      std::optional<mojo_base::ProtoWrapper> image_feature_embedding,
      std::optional<mojo_base::ProtoWrapper> visual_features);

  // Sets the primary account signed in callback for testing.
  void set_account_signed_in_for_testing(
      const PrimaryAccountSignedIn& account_signed_in_callback) {
    account_signed_in_callback_ = account_signed_in_callback;
  }

  void set_delegate_for_testing(std::unique_ptr<Delegate> delegate) {
    delegate_ = std::move(delegate);
  }

  // Callback for when preclassification is done.
  using PreclassificationDone =
      base::RepeatingCallback<void(ClientSideDetectionType)>;

  // Sets a callback to be notified when preclassification is done.
  void set_preclassification_done_callback_for_testing(
      const PreclassificationDone& callback) {
    preclassification_done_cb_for_testing_ = callback;
  }

  // Returns true if phishing detection should not proceed beyond
  // preclassification. The purpose of triggering only preclassification is to
  // have an initial assessment on how often we'll be hitting the allowlist and
  // triggering the classification. Detection should not go further than
  // recording metrics.
  bool ShouldStopAtPreClassification();

  // The callback for the report a scam dialog.
  base::OnceClosure user_report_callback_;

  // Timer to call the user report callback.
  base::OneShotTimer user_report_timeout_timer_;

  // The WebContents that the class is observing.
  raw_ptr<content::WebContents> tab_;
  // These pointers may be nullptr if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<BaseUIManager> ui_manager_;
  // Keep a handle to the latest classification request so that we can cancel
  // it if necessary.
  std::unique_ptr<ShouldClassifyUrlRequest> classification_request_;
  // The current outermost main frame's id.
  content::GlobalRenderFrameHostId current_outermost_main_frame_id_;
  // The navigation ID that commits the current URL. Used to set UnsafeResource.
  int64_t current_navigation_id_;

  // The last URL that the fullscreen API was called. This is used because the
  // DidToggleFullscreenModeForTab can be called for both entering and exiting
  // fullscreen.
  GURL last_fullscreen_url_;

  // `did_first_visually_non_empty_paint_` becomes true after the first paint
  // that is not the background color. `on_first_contentful_paint_` becomes
  // true after the browser renders the first content from the DOM (e.g.,
  // text or an image).
  //
  // Client-side detection for TRIGGER_MODELS will only start after both events
  // have occurred. This ensures that classification doesn't begin before the
  // page has meaningfully rendered. These flags are reset on each new main
  // frame navigation.
  bool did_first_visually_non_empty_paint_ = false;
  bool on_first_contentful_paint_ = false;

  std::unique_ptr<Delegate> delegate_;

  // Callback for checking if the user is signed in, before fetching
  // acces_token.
  PrimaryAccountSignedIn account_signed_in_callback_;

  // The remote for the currently active phishing classification.
  mojo::AssociatedRemote<mojom::PhishingDetector> phishing_detector_;

  // The remote for the currently active phishing image embedder.
  mojo::AssociatedRemote<mojom::PhishingImageEmbedderDetector>
      phishing_image_embedder_;

  base::ScopedObservation<permissions::PermissionRequestManager,
                          permissions::PermissionRequestManager::Observer>
      permission_request_observation_{this};

  // A boolean indicates whether TRIGGER_MODELS request is sent via
  // FORCE_REQUEST. This is used to decide whether async check is allowed to

  base::ScopedObservation<AsyncCheckTracker, AsyncCheckTracker::Observer>
      async_check_observation_{this};

  // Manages lifetime registration of this instance as an
  // AutofillManager::Observer.
  autofill::ScopedAutofillManagersObservation autofill_managers_observation_{
      this};

  // Callback settable by tests for verifying whether
  // MaybeStartPreClassification resulted in starting preclassification.
  PreclassificationStarted preclassification_started_cb_for_testing_;

  // Callback settable by tests for verifying whether
  // OnPhishingPreClassificationDone was called at the end of preclassification.
  PreclassificationDone preclassification_done_cb_for_testing_;

  // The high resolution screenshot of the current tab. Should only be populated
  // when a user reports a site as unsafe.
  std::optional<SkBitmap> screenshot_;

  // State of the browser-side classification request currently being captured
  // or scored (`kClientSideDetectionCopyFromSurface`). Cleared when the request
  // is answered before the classifier runs or cancelled; otherwise it goes
  // stale once the base class reports the result, which `is_classifying()`
  // reflects.
  std::optional<BrowserClassificationRequest> browser_classification_;

  // The page capture in progress, if any.
  std::optional<PendingCapture> pending_capture_;
  // Identifies `pending_capture_` so that stale posted tasks and copy results
  // can be told apart from the current capture's.
  int next_capture_id_ = 0;
  std::unique_ptr<VisualCapturer> visual_capturer_;

  base::WeakPtrFactory<ClientSideDetectionHost> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_HOST_H_
