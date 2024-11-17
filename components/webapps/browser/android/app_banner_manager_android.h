// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_APP_BANNER_MANAGER_ANDROID_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_APP_BANNER_MANAGER_ANDROID_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/webapps/browser/android/add_to_homescreen_installer.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_client.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_message_controller.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/pwa_install_path_tracker.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class SkBitmap;
class PrefService;

namespace segmentation_platform {
class SegmentationPlatformService;
}

namespace webapps {

class AmbientBadgeManager;
struct AddToHomescreenParams;
struct InstallBannerConfig;

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
// TODO(crbug.com/40730613): remove remaining Chrome-specific functionality and
// move to //components/webapps.
class AppBannerManagerAndroid
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerAndroid> {
 public:
  class ChromeDelegate {
   public:
    virtual ~ChromeDelegate() = default;

    // Called when the current page completed the installable web app check and
    // there were no errors. Note: This does not mean that the site is
    // installable.
    virtual void OnInstallableCheckedNoErrors(
        const ManifestId& manifest_id) const = 0;

    virtual segmentation_platform::SegmentationPlatformService*
    GetSegmentationPlatformService() = 0;

    virtual PrefService* GetPrefService() = 0;

    // Called when an install event occurs, allowing specializations to record
    // additional metrics.
    virtual void RecordExtraMetricsForInstallEvent(
        AddToHomescreenInstaller::Event event,
        const AddToHomescreenParams& a2hs_params) = 0;
  };

  static void CreateForWebContents(content::WebContents* web_contents,
                                   std::unique_ptr<ChromeDelegate> delegate);
  using content::WebContentsUserData<AppBannerManagerAndroid>::FromWebContents;

  AppBannerManagerAndroid(const AppBannerManagerAndroid&) = delete;
  AppBannerManagerAndroid& operator=(const AppBannerManagerAndroid&) = delete;
  ~AppBannerManagerAndroid() override;

  // TODO(b/323192242): Remove this in favor of an optional getter in
  // AppBannerManager later in the refactor.
  InstallBannerConfig GetCurrentInstallBannerConfig();

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
  void OnAppDetailsRetrieved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int request_id,
      const base::android::JavaParamRef<jobject>& japp_data,
      const base::android::JavaParamRef<jstring>& japp_title,
      const base::android::JavaParamRef<jstring>& japp_package,
      const base::android::JavaParamRef<jstring>& jicon_url);

  void ShowBannerFromBadge(const InstallBannerConfig& config);

  // Installs the app referenced by the data in |a2hs_params|.
  // |a2hs_event_callback| will be run to inform the caller of the progress of
  // the installation.
  void Install(const AddToHomescreenParams& a2hs_params,
               base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                            const AddToHomescreenParams&)>
                   a2hs_event_callback);

  // Returns false if the bottom sheet can't be shown. In that case an
  // alternative UI should be shown.
  bool MaybeShowPwaBottomSheetController(bool expand_sheet,
                                         WebappInstallSource install_source,
                                         const InstallBannerConfig& data);

  // AppBannerManager override:
  void OnMlInstallPrediction(base::PassKey<MLInstallabilityPromoter>,
                             std::string result_label) override;

 protected:
  friend class content::WebContentsUserData<AppBannerManagerAndroid>;

  // Creates the AddToHomescreenParams for a given install source and
  // configuration.
  // TODO(b/320681613): Wrap this configuration in a struct.
  static std::unique_ptr<AddToHomescreenParams> CreateAddToHomescreenParams(
      const InstallBannerConfig& config,
      const base::android::ScopedJavaGlobalRef<jobject>& native_java_app_data,
      WebappInstallSource install_source);

  AppBannerManagerAndroid(content::WebContents* web_contents,
                          std::unique_ptr<ChromeDelegate> delegate);

  // AppBannerManager overrides.
  bool CanRequestAppBanner() const override;
  InstallableParams ParamsToPerformInstallableWebAppCheck() override;
  bool ShouldDoNativeAppCheck(
      const blink::mojom::Manifest& manifest) const override;
  void DoNativeAppInstallableCheck(content::WebContents* web_contents,
                                   const GURL& validated_url,
                                   const blink::mojom::Manifest& manifest,
                                   NativeCheckCallback callback) override;
  void OnWebAppInstallableCheckedNoErrors(
      const ManifestId& manifest_id) const override;
  base::expected<void, InstallableStatusCode> CanRunWebAppInstallableChecks(
      const blink::mojom::Manifest& manifest) override;
  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override;
  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;
  void MaybeShowAmbientBadge(const InstallBannerConfig& config) override;
  void ShowBannerUi(WebappInstallSource install_source,
                    const InstallBannerConfig& config) override;
  base::WeakPtr<AppBannerManager> GetWeakPtrForThisNavigation() override;
  void InvalidateWeakPtrsForThisNavigation() override;
  void ResetCurrentPageData() override;

  // Use as a callback to notify |this| after an install event such as a dialog
  // being cancelled or an app being installed has occurred.
  void OnInstallEvent(GURL validated_url,
                      AddToHomescreenInstaller::Event event,
                      const AddToHomescreenParams& a2hs_params);

  base::WeakPtr<AppBannerManagerAndroid> GetAndroidWeakPtr();

  // TODO(b/323192242): Remove.
  const base::android::ScopedJavaGlobalRef<jobject>&
  native_java_app_data_for_testing() const {
    return native_java_app_data_;
  }

 private:
  friend class content::WebContentsUserData<AppBannerManagerAndroid>;

  struct QueryNativeAppConfig {
    QueryNativeAppConfig(
        const base::android::ScopedJavaLocalRef<jstring>& url,
        const base::android::ScopedJavaLocalRef<jstring>& package,
        const base::android::ScopedJavaLocalRef<jstring>& referrer);
    QueryNativeAppConfig(const QueryNativeAppConfig& config);
    ~QueryNativeAppConfig();

    base::android::ScopedJavaLocalRef<jstring> url;
    base::android::ScopedJavaLocalRef<jstring> package;
    base::android::ScopedJavaLocalRef<jstring> referrer;
  };

  // Creates the Java-side AppBannerManager.
  void CreateJavaBannerManager(content::WebContents* web_contents);

  // Returns the `QueryNativeAppConfig` if |platform|, |url|, and |id| are
  // consistent and can be used to query the Play Store for a native app.
  // Otherwise returns the error which prevents querying from taking place. The
  // query may not necessarily succeed (e.g. |id| doesn't map to anything), but
  // if this method returns the expected struct, only a native app banner
  // may be shown, and the web app banner flow will not be run.
  base::expected<QueryNativeAppConfig, InstallableStatusCode>
  GetNativeAppFetchRequestConfig(
      const GURL& validated_url,
      JNIEnv* env,
      const blink::Manifest::RelatedApplication& related_application) const;

  // Called when the download of a native app's icon is complete, as native
  // banners use an icon provided from the Play Store rather than the web
  // manifest.
  void OnNativeAppIconFetched(std::string app_package,
                              std::u16string app_title,
                              GURL primary_icon_url,
                              const SkBitmap& bitmap);

  const std::unique_ptr<ChromeDelegate> delegate_;

  // The Java-side AppBannerManager.
  base::android::ScopedJavaGlobalRef<jobject> java_banner_manager_;

  // Java-side object containing data about a native app.
  base::android::ScopedJavaGlobalRef<jobject> native_java_app_data_;

  int next_native_request_id_ = 0;
  std::optional<int> current_native_request_id_;
  // This can turn into a raw pointer passed to Android and sent back in
  // OnAppDetailsRetrieved, however this adds leak risk.
  NativeCheckCallback native_check_callback_storage_;

  std::unique_ptr<AmbientBadgeManager> ambient_badge_manager_;

  base::WeakPtrFactory<AppBannerManagerAndroid> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_APP_BANNER_MANAGER_ANDROID_H_
