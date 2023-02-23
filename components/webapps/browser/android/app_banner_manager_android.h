// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_APP_BANNER_MANAGER_ANDROID_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_APP_BANNER_MANAGER_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_client.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_message_controller.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "url/gurl.h"

class SkBitmap;

namespace webapps {

struct AddToHomescreenParams;

// Extends the AppBannerManager to support native Android apps. This class owns
// a Java-side AppBannerManager which interfaces with the Java runtime to fetch
// native app data and install them when requested.
//
// A site requests a native app banner by setting "prefer_related_applications"
// to true in its manifest, and providing at least one related application for
// the "play" platform with a Play Store ID.
//
// This class uses that information to request the app's metadata, including an
// icon. If successful, the icon is downloaded and the native app banner shown.
// Otherwise, if no related applications were detected, or their manifest
// entries were invalid, this class falls back to trying to verify if a web app
// banner is suitable.
//
// The code path forks in PerformInstallableCheck(); for a native app, it will
// eventually call to OnAppIconFetched(), while a web app calls through to
// OnDidPerformInstallableCheck(). Each of these methods then calls
// SendBannerPromptRequest(), which combines the forked code paths back
// together.
//
// TODO(crbug.com/1147268): remove remaining Chrome-specific functionality and
// move to //components/webapps.
class AppBannerManagerAndroid : public AppBannerManager,
                                public InstallableAmbientBadgeClient {
 public:
  explicit AppBannerManagerAndroid(content::WebContents* web_contents);
  AppBannerManagerAndroid(const AppBannerManagerAndroid&) = delete;
  AppBannerManagerAndroid& operator=(const AppBannerManagerAndroid&) = delete;
  ~AppBannerManagerAndroid() override;

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.banners
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AmbientBadgeState
  enum class AmbientBadgeState {
    // The ambient badge pipeline has not yet been triggered for this page load.
    INACTIVE = 0,

    // The ambient badge pipeline is running.
    ACTIVE = 1,

    // Ambient badge blocked because of recently dismissed
    BLOCKED = 2,

    // Waiting for service worker install to trigger the banner.
    PENDING_WORKER = 3,

    // Waiting for sufficient engagement to trigger the ambient badge.
    PENDING_ENGAGEMENT = 4,

    // Showing Ambient Badge.
    SHOWING = 5,

    // Ambient badge dismissed.
    DISMISSED = 6,
  };

  // Returns a reference to the Java-side AppBannerManager owned by this object.
  const base::android::ScopedJavaLocalRef<jobject> GetJavaBannerManager() const;

  // Returns the name of the installable web app, if the name has been
  // determined (and blank if not).
  base::android::ScopedJavaLocalRef<jstring> GetInstallableWebAppName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_web_contents);
  base::android::ScopedJavaLocalRef<jstring> GetInstallableWebAppManifestId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // Returns true if the banner pipeline is currently running.
  bool IsRunningForTesting(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj);

  // Returns the state of the processing pipeline for testing purposes.
  int GetPipelineStatusForTesting(JNIEnv* env);

  int GetBadgeStatusForTesting(JNIEnv* env);

  // Called when the Java-side has retrieved information for the app.
  // Returns |false| if an icon fetch couldn't be kicked off.
  bool OnAppDetailsRetrieved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& japp_data,
      const base::android::JavaParamRef<jstring>& japp_title,
      const base::android::JavaParamRef<jstring>& japp_package,
      const base::android::JavaParamRef<jstring>& jicon_url);

  // AppBannerManager overrides.
  void RequestAppBanner(const GURL& validated_url) override;

  // InstallableAmbientBadgeClient overrides.
  void AddToHomescreenFromBadge() override;
  void BadgeDismissed() override;

  // Installs the app referenced by the data in |a2hs_params|.
  // |a2hs_event_callback| will be run to inform the caller of the progress of
  // the installation.
  void Install(const AddToHomescreenParams& a2hs_params,
               base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                            const AddToHomescreenParams&)>
                   a2hs_event_callback);

  // Returns the appropriate app name based on whether we have a native/web app.
  std::u16string GetAppName() const override;

  // Returns false if the bottom sheet can't be shown. In that case an
  // alternative UI should be shown.
  bool MaybeShowPwaBottomSheetController(bool expand_sheet,
                                         WebappInstallSource install_source);

 protected:
  // AppBannerManager overrides.
  std::string GetAppIdentifier() override;
  std::string GetBannerType() override;
  void PerformInstallableChecks() override;
  InstallableParams ParamsToPerformInstallableWebAppCheck() override;
  void PerformInstallableWebAppCheck() override;
  void PerformWorkerCheckForAmbientBadge() override;
  void OnDidPerformWorkerCheckForAmbientBadge(
      const InstallableData& data) override;
  void ResetCurrentPageData() override;
  void ShowBannerUi(WebappInstallSource install_source) override;
  void MaybeShowAmbientBadge() override;
  base::WeakPtr<AppBannerManager> GetWeakPtr() override;
  void InvalidateWeakPtrs() override;
  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override;
  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;
  bool IsWebAppConsideredInstalled() const override;

  // Called to show UI that promotes installation of a PWA. This is normally the
  // mini-infobar ("banner") but clients can override it by providing a
  // specialization of this class.
  virtual void ShowAmbientBadge();

  // Called when an install event occurs, allowing specializations to record
  // additional metrics.
  virtual void RecordExtraMetricsForInstallEvent(
      AddToHomescreenInstaller::Event event,
      const AddToHomescreenParams& a2hs_params);

  // Creates the AddToHomescreenParams for a given install source.
  std::unique_ptr<AddToHomescreenParams> CreateAddToHomescreenParams(
      webapps::WebappInstallSource install_source);

  // Use as a callback to notify |this| after an install event such as a dialog
  // being cancelled or an app being installed has occurred.
  void OnInstallEvent(AddToHomescreenInstaller::Event event,
                      const AddToHomescreenParams& a2hs_params);

  base::WeakPtr<AppBannerManagerAndroid> GetAndroidWeakPtr();

  // Java-side object containing data about a native app.
  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;

 private:
  // Creates the Java-side AppBannerManager.
  void CreateJavaBannerManager(content::WebContents* web_contents);

  // Returns the query value for |name| in |url|, e.g. example.com?name=value.
  std::string ExtractQueryValueForName(const GURL& url,
                                       const std::string& name);

  bool ShouldPerformInstallableNativeAppCheck();
  void PerformInstallableNativeAppCheck();

  // Returns NO_ERROR_DETECTED if |platform|, |url|, and |id| are
  // consistent and can be used to query the Play Store for a native app.
  // Otherwise returns the error which prevents querying from taking place. The
  // query may not necessarily succeed (e.g. |id| doesn't map to anything), but
  // if this method returns NO_ERROR_DETECTED, only a native app banner
  // may be shown, and the web app banner flow will not be run.
  InstallableStatusCode QueryNativeApp(const std::u16string& platform,
                                       const GURL& url,
                                       const std::string& id);

  // Called when the download of a native app's icon is complete, as native
  // banners use an icon provided from the Play Store rather than the web
  // manifest.
  void OnNativeAppIconFetched(const SkBitmap& bitmap);

  // Checks whether the web page has sufficient engagement for showing the
  // ambient badge.
  bool HasSufficientEngagementForAmbientBadge();

  bool ShouldSuppressAmbientBadge();

  // Shows the in-product help if possible and returns true when a request to
  // show it was made, but false if conditions (e.g. engagement score) for
  // showing where not deemed adequate.
  bool MaybeShowInProductHelp() const;

  // Hides the ambient badge if it is showing.
  void HideAmbientBadge();

  // The Java-side AppBannerManager.
  base::android::ScopedJavaGlobalRef<jobject> java_banner_manager_;

  // Message controller for the ambient badge.
  InstallableAmbientBadgeMessageController message_controller_{this};

  // App package name for a native app banner.
  std::string native_app_package_;

  // Title to display in the banner for native app.
  std::u16string native_app_title_;

  // The current ambient badge status.
  AmbientBadgeState badge_state_ = AmbientBadgeState::INACTIVE;

  base::WeakPtrFactory<AppBannerManagerAndroid> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_APP_BANNER_MANAGER_ANDROID_H_
