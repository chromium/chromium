// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
#define COMPONENTS_WEBAPPS_BROWSER_BANNERS_APP_BANNER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/browser/pwa_install_path_tracker.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace webapps {
class InstallableManager;
enum class WebappInstallSource;
struct InstallableData;
struct Screenshot;

namespace test {
extern bool g_disable_banner_triggering_for_testing;
}  // namespace test

// Coordinates the creation of an app banner, from detecting eligibility to
// fetching data and creating the infobar. Sites declare that they want an app
// banner using the web app manifest. One web/native app may occupy the pipeline
// at a time; navigation resets the manager and discards any work in progress.
//
// The InstallableManager fetches and validates whether a site is eligible for
// banners. The manager is first called to fetch the manifest, so we can verify
// whether the site is already installed (and on Android, divert the flow to a
// native app banner if requested). The second call completes the checking for a
// web app banner (checking manifest validity, service worker, and icon).
//
// TODO(https://crbug.com/930612): Refactor this into several simpler classes.
class AppBannerManager : public content::WebContentsObserver,
                         public blink::mojom::AppBannerService,
                         public site_engagement::SiteEngagementObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInstallableWebAppStatusUpdated() = 0;
  };

  // A StatusReporter handles the reporting of |InstallableStatusCode|s.
  class StatusReporter;

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.banners
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AppBannerManagerState
  enum class State {
    // The pipeline has not yet been triggered for this page load.
    INACTIVE,

    // The pipeline is running for this page load.
    ACTIVE,

    // The pipeline is waiting for the web app manifest to be fetched.
    FETCHING_MANIFEST,

    // The pipeline is waiting for native app data to be fetched.
    FETCHING_NATIVE_DATA,

    // The pipeline is waiting for the installability criteria to be checked.
    // In this state, the pipeline could be paused while waiting for a service
    // worker to be registered..
    PENDING_INSTALLABLE_CHECK,

    // The pipeline has finished running, but is waiting for sufficient
    // engagement to trigger the banner.
    PENDING_ENGAGEMENT,

    // The pipeline is waiting for service worker install to trigger the banner.
    PENDING_WORKER,

    // The beforeinstallprompt event has been sent and the pipeline is waiting
    // for the response.
    SENDING_EVENT,

    // The beforeinstallprompt event was sent, and the web page called prompt()
    // on the event while the event was being handled.
    SENDING_EVENT_GOT_EARLY_PROMPT,

    // The pipeline has finished running, but is waiting for the web page to
    // call prompt() on the event.
    PENDING_PROMPT_NOT_CANCELED,

    // The pipeline has finished running, web page called preventdefault(),
    // pipeline is waiting for the web page to call prompt() on the event.
    PENDING_PROMPT_CANCELED,

    // The pipeline has finished running for this page load and no more
    // processing is to be done.
    COMPLETE,
  };

  // Installable describes to what degree a site satisifes the installablity
  // requirements.
  enum class InstallableWebAppCheckResult {
    kUnknown,
    kNo,
    kNo_AlreadyInstalled,
    kYes_ByUserRequest,
    kYes_Promotable,
  };

  // Retrieves the platform specific instance of AppBannerManager from
  // |web_contents|.
  static AppBannerManager* FromWebContents(content::WebContents* web_contents);

  AppBannerManager(const AppBannerManager&) = delete;
  AppBannerManager& operator=(const AppBannerManager&) = delete;

  // Returns the current time.
  static base::Time GetCurrentTime();

  // Fast-forwards the current time for testing.
  static void SetTimeDeltaForTesting(int days);

  // TODO(https://crbug.com/930612): Move |GetInstallableAppName| and
  // |IsExternallyInstalledWebApp| out into a more general purpose
  // installability check class.

  // Returns the app name if the current page is installable, otherwise returns
  // the empty string.
  static std::u16string GetInstallableWebAppName(
      content::WebContents* web_contents);

  static std::string GetInstallableWebAppManifestId(
      content::WebContents* web_contents);

  // Returns whether installability checks satisfy promotion requirements
  // (e.g. having a service worker fetch event) or have passed previously within
  // the current manifest scope. Already-installed apps are non-promotable by
  // default but can be checked with |ignore_existing_installations|.
  bool IsProbablyPromotableWebApp(
      bool ignore_existing_installations = false) const;

  // Returns whether installability checks satisfy promotion requirements
  // (e.g. having a service worker fetch event).
  bool IsPromotableWebApp() const;

  // Returns the page's web app start URL if available, otherwise return an
  // empty or invalid GURL.
  const GURL& GetManifestStartUrl() const;

  // Returns the page's web app |DisplayMode| if available, otherwise it will be
  // DisplayMode::kUndefined.
  blink::mojom::DisplayMode GetManifestDisplayMode() const;

  // Each successful installability check gets to show one animation prompt,
  // this returns and consumes the animation prompt if it is available.
  bool MaybeConsumeInstallAnimation();

  // Requests an app banner.
  virtual void RequestAppBanner(const GURL& validated_url);

  // Informs the page that it has been installed with appinstalled event and
  // performs logging related to the app installation. Appinstalled event is
  // redundant for the beforeinstallprompt event's promise being resolved, but
  // is required by the install event spec.
  // This is virtual for testing.
  virtual void OnInstall(blink::mojom::DisplayMode display);

  // Sends a message to the renderer that the user accepted the banner.
  void SendBannerAccepted();

  // Sends a message to the renderer that the user dismissed the banner.
  void SendBannerDismissed();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual base::WeakPtr<AppBannerManager> GetWeakPtr() = 0;

  // This is used to determine if the `AppBannerManager` pipeline should be
  // disabled. A test may disable the original `AppBannerManager` (by using
  // `test::g_disable_banner_triggering_for_testing`) but instead use a
  // `TestAppBannerManager` that override this method to `true`, allowing that
  // class to function correctly.
  virtual bool TriggeringDisabledForTesting() const;

  // Returns whether the site can call "event.prompt()" to prompt the user to
  // install the site.
  bool IsPromptAvailableForTesting() const;

  InstallableWebAppCheckResult GetInstallableWebAppCheckResultForTesting();

  // Return the name of the app for this page.
  virtual std::u16string GetAppName() const;

  // Simple accessors:
  const blink::mojom::Manifest& manifest() const;
  const SkBitmap& primary_icon() const { return primary_icon_; }
  bool has_maskable_primary_icon() const { return has_maskable_primary_icon_; }
  const GURL& validated_url() { return validated_url_; }
  const std::vector<Screenshot>& screenshots() { return screenshots_; }

  // Tracks the route taken to an install of a PWA (whether the bottom sheet
  // was shown or the infobar/install) and what triggered it (install source).
  // Only used on Android.
  void TrackInstallPath(bool bottom_sheet, WebappInstallSource install_source);

  // Tracks that the IPH has been shown. Only used on Android.
  void TrackIphWasShown();

 protected:
  explicit AppBannerManager(content::WebContents* web_contents);
  ~AppBannerManager() override;

  enum class UrlType {
    // This url & page should be considered for installability & promotability.
    kValidForBanner,
    // The load from the render frame host was not for the current/primary page
    // so it can be ignored.
    kNotPrimaryFrame,
    // The primary url that was loaded can never be elibible for installability.
    kInvalidPrimaryFrameUrl,
  };
  // Returns the URL type, allowing the banner logic to ignore urls that aren't
  // the primary frame or aren't a valid URL.
  UrlType GetUrlType(content::RenderFrameHost* render_frame_host,
                     const GURL& url);

  // Returns true if the banner should be shown. Returns false if the banner has
  // been shown too recently, or if the app has already been installed.
  // GetAppIdentifier() must return a valid value for this method to work.
  bool CheckIfShouldShowBanner();

  // Returns whether the site would prefer a related non-web app be installed
  // instead of the PWA or a related non-web app is already installed.
  bool ShouldDeferToRelatedNonWebApp() const;

  // Return a string identifying this app for metrics.
  virtual std::string GetAppIdentifier();

  // Return a string describing what type of banner is being created. Used when
  // alerting websites that a banner is about to be created.
  virtual std::string GetBannerType();

  virtual void InvalidateWeakPtrs() = 0;

  // Returns true if |has_sufficient_engagement_| is true or
  // ShouldBypassEngagementChecks() returns true.
  bool HasSufficientEngagement() const;

  // Returns true if the kBypassAppBannerEngagementChecks flag is set.
  bool ShouldBypassEngagementChecks() const;

  // Returns whether installation of apps from |platform| is supported on the
  // current device and the platform delivers apps considered replacements for
  // web apps.
  virtual bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const = 0;

  // Returns whether |related_app| is already installed and considered a
  // replacement for the manifest's web app.
  virtual bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const = 0;

  // Returns whether the current page is already installed as a web app, or
  // should be considered as installed. Returns true if there is an installed
  // web app within the BrowserContext of |web_contents()| that contains |url|
  // within its scope, and false otherwise. For example, the URL
  // https://example.com/a/b/c/d.html is contained within a web app with scope
  // https://example.com/a/b/.
  virtual bool IsWebAppConsideredInstalled() const = 0;

  // Returns whether the installed web app at the current page can be
  // overwritten with a new app install for the current page.
  virtual bool ShouldAllowWebAppReplacementInstall();

  // Possibly retries the installable manager request given the current state
  // and the result. Returns |true| if the request was restarted.
  // Currently only called during requests to InstallationManager
  bool DidRetryInstallableManagerRequest(const InstallableData& result);

  // Callback invoked by the InstallableManager once it has fetched the page's
  // manifest.
  virtual void OnDidGetManifest(const InstallableData& data);

  // Returns an InstallableParams object that requests all checks
  // necessary for a web app banner.
  virtual InstallableParams ParamsToPerformInstallableWebAppCheck();

  // Returns an InstallableParams object that requests service worker check
  // only.
  virtual InstallableParams ParamsToPerformWorkerCheck();

  // Run at the conclusion of OnDidGetManifest. For web app banners, this calls
  // back to the InstallableManager to continue checking criteria. For native
  // app banners, this checks whether native apps are preferred in the manifest,
  // and calls to Java to verify native app details. If a native banner isn't or
  // can't be requested, it continues with the web app banner checks.
  virtual void PerformInstallableChecks();

  virtual void PerformInstallableWebAppCheck();

  // Callback invoked by the InstallableManager once it has finished checking
  // all other installable properties.
  virtual void OnDidPerformInstallableWebAppCheck(const InstallableData& data);

  // Run at the conclusion of OnDidPerformInstallableWebAppCheck. This calls
  // back to the InstallableManager to continue checking service worker criteria
  // for web app banners.
  virtual void PerformServiceWorkerCheck();

  // Callback invoked by the InstallableManager once it has finished checking
  // service worker.
  virtual void OnDidPerformWorkerCheck(const InstallableData& data);

  // Records that a banner was shown.
  void RecordDidShowBanner();

  // Reports |code| via a UMA histogram or logs it to the console.
  void ReportStatus(InstallableStatusCode code);

  // Voids all outstanding service pointers.
  void ResetBindings();

  // Resets all fetched data for the current page.
  virtual void ResetCurrentPageData();

  // Stops the banner pipeline early.
  void Terminate();

  // Stops the banner pipeline, preventing any outstanding callbacks from
  // running and resetting the manager state. This method is virtual to allow
  // tests to intercept it and verify correct behaviour.
  virtual void Stop(InstallableStatusCode code);

  // Sends a message to the renderer that the page has met the requirements to
  // show a banner. The page can respond to cancel the banner (and possibly
  // display it later), or otherwise allow it to be shown.
  void SendBannerPromptRequest();

  // Shows the ambient badge if the current page advertises a native app or is
  // a web app. By default this shows nothing, but platform-specific code might
  // override this to show UI (e.g. on Android).
  virtual void MaybeShowAmbientBadge();

  // Updates the current state to |state|. Virtual to allow overriding in tests.
  virtual void UpdateState(State state);

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidActivatePortal(content::WebContents* predecessor_contents,
                         base::TimeTicks activation_time) override;
  void DidUpdateWebManifestURL(content::RenderFrameHost* target_frame,
                               const GURL& manifest_url) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const content::MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;
  void WebContentsDestroyed() override;

  // SiteEngagementObserver overrides.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         site_engagement::EngagementType type) override;

  // Subclass accessors for private fields which should not be changed outside
  // this class.
  InstallableManager* manager() const { return manager_; }
  State state() const { return state_; }
  bool IsRunning() const;

  void SetInstallableWebAppCheckResult(InstallableWebAppCheckResult result);
  void RecheckInstallabilityForLoadedPage(const GURL& url, bool uninstalled);

  // The URL for which the banner check is being conducted.
  GURL validated_url_;

  // The URL of the manifest.
  GURL manifest_url_;

  // The manifest id.
  GURL manifest_id_;

  // The URL of the primary icon.
  GURL primary_icon_url_;

  // The primary icon object.
  SkBitmap primary_icon_;

  // Whether or not the primary icon is maskable.
  bool has_maskable_primary_icon_ = false;

  // The current banner pipeline state for this page load.
  State state_ = State::INACTIVE;

  // The screenshots to show in the install UI.
  std::vector<Screenshot> screenshots_;

  // True if the service worker check has passed or no required.
  bool passed_worker_check_ = false;

 private:
  friend class AppBannerManagerTest;

  // Checks whether the web page has sufficient engagement and continue with
  // the pipeline.
  void CheckSufficientEngagement();

  // Record that the banner could be shown at this point, if the triggering
  // heuristic allowed.
  void RecordCouldShowBanner();

  // Creates the app banner UI. Overridden by subclasses as the infobar is
  // platform-specific.
  virtual void ShowBannerUi(WebappInstallSource install_source) = 0;

  // Called after the manager sends a message to the renderer regarding its
  // intention to show a prompt. The renderer will send a message back with the
  // opportunity to cancel.
  virtual void OnBannerPromptReply(
      mojo::Remote<blink::mojom::AppBannerController> controller,
      blink::mojom::AppBannerPromptReply reply);

  // Does the non-platform specific parts of showing the app banner.
  void ShowBanner();

  // blink::mojom::AppBannerService overrides.
  // Called when Blink has prevented a banner from being shown, and is now
  // requesting that it be shown later.
  void DisplayAppBanner() override;

  // Returns a status code indicating whether a banner should be shown.
  InstallableStatusCode ShouldShowBannerCode();

  // Returns a status code based on the current state, to log when terminating.
  InstallableStatusCode TerminationCode() const;

  // Fetches the data required to display a banner for the current page.
  raw_ptr<InstallableManager, DanglingUntriaged> manager_;

  // Measures site UKMs once the AppBannerManager triggers a pipeline and
  // triggers a ML model to promote installability of an app.
  raw_ptr<MLInstallabilityPromoter, DanglingUntriaged> ml_promoter_;

  // The manifest object. This is never null, it will instead be an empty
  // manifest so callers don't have to worry about null checks.
  blink::mojom::ManifestPtr manifest_;

  // We do not want to trigger a banner when the manager is attached to
  // a WebContents that is playing video. Banners triggering on a site in the
  // background will appear when the tab is reactivated.
  std::vector<content::MediaPlayerId> active_media_players_;

  mojo::Receiver<blink::mojom::AppBannerService> receiver_{this};
  mojo::Remote<blink::mojom::AppBannerEvent> event_;

  // If a banner is requested before the page has finished loading, defer
  // triggering the pipeline until the load is complete.
  bool has_sufficient_engagement_ = false;
  bool load_finished_ = false;

  std::unique_ptr<StatusReporter> status_reporter_;
  bool install_animation_pending_ = false;
  InstallableWebAppCheckResult installable_web_app_check_result_ =
      InstallableWebAppCheckResult::kUnknown;

  // The scope of the most recent installability check that passes promotability
  // requirements, otherwise invalid.
  GURL last_promotable_web_app_scope_;
  // The scope of the most recent installability check that was non-promotable
  // due to being already installed, otherwise invalid.
  GURL last_already_installed_web_app_scope_;

  // Keeps track of the path the user took through the UI, before deciding to
  // install.
  PwaInstallPathTracker install_path_tracker_;

  base::ObserverList<Observer, true> observer_list_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
