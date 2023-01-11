// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/app_banner_manager_android.h"

#include <limits>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/messages/android/messages_feature.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "components/webapps/browser/android/add_to_homescreen_coordinator.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/bottomsheet/pwa_bottom_sheet_controller.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_infobar_delegate.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/android/webapps_jni_headers/AppBannerManager_jni.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace webapps {

namespace {

constexpr char kPlatformPlay[] = "play";

// Whether to ignore the Chrome channel in QueryNativeApp() for testing.
bool gIgnoreChromeChannelForTesting = false;

}  // anonymous namespace

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) {
  CreateJavaBannerManager(web_contents);
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerManager_destroy(env, java_banner_manager_);
  java_banner_manager_.Reset();
}

const base::android::ScopedJavaLocalRef<jobject>
AppBannerManagerAndroid::GetJavaBannerManager() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_banner_manager_);
}

bool AppBannerManagerAndroid::IsRunningForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsRunning();
}

int AppBannerManagerAndroid::GetPipelineStatusForTesting(JNIEnv* env) {
  return (int)state();
}

bool AppBannerManagerAndroid::OnAppDetailsRetrieved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& japp_data,
    const JavaParamRef<jstring>& japp_title,
    const JavaParamRef<jstring>& japp_package,
    const JavaParamRef<jstring>& jicon_url) {
  UpdateState(State::ACTIVE);
  native_app_data_.Reset(japp_data);
  native_app_title_ = ConvertJavaStringToUTF16(env, japp_title);
  native_app_package_ = ConvertJavaStringToUTF8(env, japp_package);
  primary_icon_url_ = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  if (!CheckIfShouldShowBanner())
    return false;

  return content::ManifestIconDownloader::Download(
      web_contents(), primary_icon_url_,
      WebappsIconUtils::GetIdealHomescreenIconSizeInPx(),
      WebappsIconUtils::GetMinimumHomescreenIconSizeInPx(),
      /* maximum_icon_size_in_px= */ std::numeric_limits<int>::max(),
      base::BindOnce(&AppBannerManagerAndroid::OnNativeAppIconFetched,
                     weak_factory_.GetWeakPtr()));
}

void AppBannerManagerAndroid::RequestAppBanner(const GURL& validated_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AppBannerManager_isSupported(env) ||
      !WebappsClient::Get()->CanShowAppBanners(web_contents())) {
    return;
  }

  AppBannerManager::RequestAppBanner(validated_url);
}

void AppBannerManagerAndroid::AddToHomescreenFromBadge() {
  ShowBannerUi(InstallableMetrics::GetInstallSource(
      web_contents(), InstallTrigger::AMBIENT_BADGE));

  // Close our bindings to ensure that any existing beforeinstallprompt events
  // cannot trigger add to home screen (which would cause a crash). If the
  // banner is dismissed, the event will be resent.
  ResetBindings();
}

void AppBannerManagerAndroid::BadgeDismissed() {
  TrackDismissEvent(DISMISS_EVENT_AMBIENT_INFOBAR_DISMISSED);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, GetCurrentTime());
}

std::string AppBannerManagerAndroid::GetAppIdentifier() {
  return native_app_data_.is_null() ? AppBannerManager::GetAppIdentifier()
                                    : native_app_package_;
}

std::string AppBannerManagerAndroid::GetBannerType() {
  return native_app_data_.is_null() ? AppBannerManager::GetBannerType()
                                    : "play";
}

InstallableParams
AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params =
      AppBannerManager::ParamsToPerformInstallableWebAppCheck();
  params.prefer_maskable_icon =
      WebappsIconUtils::DoesAndroidSupportMaskableIcons();
  return params;
}

void AppBannerManagerAndroid::PerformInstallableChecks() {
  if (ShouldPerformInstallableNativeAppCheck())
    PerformInstallableNativeAppCheck();
  else
    PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::PerformInstallableWebAppCheck() {
  if (!manifest_url_.SchemeIsHTTPOrHTTPS()) {
    Stop(MANIFEST_URL_SCHEME_NOT_SUPPORTED_FOR_WEBAPK);
    return;
  }
  if (!webapps::WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest())) {
    Stop(URL_NOT_SUPPORTED_FOR_WEBAPK);
    return;
  }
  AppBannerManager::PerformInstallableWebAppCheck();
}

void AppBannerManagerAndroid::PerformWorkerCheckForAmbientBadge() {
  manager()->GetData(
      ParamsToPerformWorkerCheck(),
      base::BindOnce(
          &AppBannerManagerAndroid::OnDidPerformWorkerCheckForAmbientBadge,
          weak_factory_.GetWeakPtr()));
}

void AppBannerManagerAndroid::OnDidPerformWorkerCheckForAmbientBadge(
    const InstallableData& data) {
  if (!data.NoBlockingErrors()) {
    return;
  }

  passed_worker_check_ = true;

  if (state_ == State::PENDING_PROMPT_NOT_CANCELED) {
    MaybeShowAmbientBadge();
  }
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  AppBannerManager::ResetCurrentPageData();
  native_app_data_.Reset();
  native_app_package_ = "";
}

std::unique_ptr<AddToHomescreenParams>
AppBannerManagerAndroid::CreateAddToHomescreenParams(
    WebappInstallSource install_source) {
  auto a2hs_params = std::make_unique<AddToHomescreenParams>();
  a2hs_params->primary_icon = primary_icon_;
  if (native_app_data_.is_null()) {
    a2hs_params->app_type = AddToHomescreenParams::AppType::WEBAPK;
    a2hs_params->shortcut_info = ShortcutInfo::CreateShortcutInfo(
        manifest_url_, manifest(), primary_icon_url_);
    a2hs_params->install_source = install_source;
    a2hs_params->has_maskable_primary_icon = has_maskable_primary_icon_;
  } else {
    a2hs_params->app_type = AddToHomescreenParams::AppType::NATIVE;
    a2hs_params->native_app_data = native_app_data_;
    a2hs_params->native_app_package_name = native_app_package_;
  }
  return a2hs_params;
}

void AppBannerManagerAndroid::ShowBannerUi(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  auto a2hs_params = CreateAddToHomescreenParams(install_source);

  bool was_shown = AddToHomescreenCoordinator::ShowForAppBanner(
      weak_factory_.GetWeakPtr(), std::move(a2hs_params),
      base::BindRepeating(&AppBannerManagerAndroid::OnInstallEvent,
                          weak_factory_.GetWeakPtr()));

  // If we are installing from the ambient badge, it will remove itself.
  if (install_source != WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB &&
      install_source != WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB &&
      install_source != WebappInstallSource::RICH_INSTALL_UI_WEBLAYER) {
    HideAmbientBadge();
  }

  if (was_shown) {
    if (native_app_data_.is_null()) {
      ReportStatus(SHOWING_WEB_APP_BANNER);
    } else {
      ReportStatus(SHOWING_NATIVE_APP_BANNER);
    }
  } else {
    ReportStatus(FAILED_TO_CREATE_BANNER);
  }
}

void AppBannerManagerAndroid::OnInstallEvent(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  RecordExtraMetricsForInstallEvent(event, a2hs_params);

  // If the app is not native and the install source is a menu install source,
  // user interacted with the bottom sheet installer UI.
  if (a2hs_params.app_type != AddToHomescreenParams::AppType::NATIVE &&
      (a2hs_params.install_source == WebappInstallSource::MENU_BROWSER_TAB ||
       a2hs_params.install_source == WebappInstallSource::MENU_CUSTOM_TAB)) {
    switch (event) {
      case AddToHomescreenInstaller::Event::INSTALL_STARTED:
        AppBannerSettingsHelper::RecordBannerEvent(
            web_contents(), web_contents()->GetVisibleURL(),
            a2hs_params.shortcut_info->url.spec(),
            AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
            base::Time::Now());
        break;
      case AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED:
        SendBannerAccepted();
        OnInstall(a2hs_params.shortcut_info->display);
        break;
      case AddToHomescreenInstaller::Event::UI_CANCELLED:
        // Collapsing the bottom sheet installer UI does not count as
        // UI_CANCELLED as it is still visible to the user and they can expand
        // it later.
        SendBannerDismissed();
        AppBannerSettingsHelper::RecordBannerDismissEvent(
            web_contents(), a2hs_params.shortcut_info->url.spec());
        break;
      default:
        break;
    }
    return;
  }

  DCHECK(a2hs_params.app_type == AddToHomescreenParams::AppType::NATIVE ||
         a2hs_params.install_source ==
             WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB ||
         a2hs_params.install_source ==
             WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB ||
         a2hs_params.install_source ==
             WebappInstallSource::RICH_INSTALL_UI_WEBLAYER ||
         a2hs_params.install_source == WebappInstallSource::API_BROWSER_TAB ||
         a2hs_params.install_source == WebappInstallSource::API_CUSTOM_TAB ||
         a2hs_params.install_source == WebappInstallSource::DEVTOOLS);

  switch (event) {
    case AddToHomescreenInstaller::Event::INSTALL_STARTED:
      TrackDismissEvent(DISMISS_EVENT_DISMISSED);
      switch (a2hs_params.app_type) {
        case AddToHomescreenParams::AppType::NATIVE:
          TrackUserResponse(USER_RESPONSE_NATIVE_APP_ACCEPTED);
          break;
        case AddToHomescreenParams::AppType::WEBAPK:
          [[fallthrough]];
        case AddToHomescreenParams::AppType::SHORTCUT:
          TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
          AppBannerSettingsHelper::RecordBannerInstallEvent(
              web_contents(), a2hs_params.shortcut_info->url.spec());
          break;
        default:
          NOTREACHED();
      }
      break;

    case AddToHomescreenInstaller::Event::INSTALL_FAILED:
      TrackDismissEvent(DISMISS_EVENT_ERROR);
      break;

    case AddToHomescreenInstaller::Event::NATIVE_INSTALL_OR_OPEN_FAILED:
      DCHECK_EQ(a2hs_params.app_type, AddToHomescreenParams::AppType::NATIVE);
      TrackInstallEvent(INSTALL_EVENT_NATIVE_APP_INSTALL_TRIGGERED);
      break;

    case AddToHomescreenInstaller::Event::NATIVE_INSTALL_OR_OPEN_SUCCEEDED:
      DCHECK_EQ(a2hs_params.app_type, AddToHomescreenParams::AppType::NATIVE);
      TrackDismissEvent(DISMISS_EVENT_APP_OPEN);
      break;

    case AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED:
      SendBannerAccepted();
      if (a2hs_params.app_type == AddToHomescreenParams::AppType::WEBAPK ||
          a2hs_params.app_type == AddToHomescreenParams::AppType::SHORTCUT) {
        OnInstall(a2hs_params.shortcut_info->display);
      }
      break;

    case AddToHomescreenInstaller::Event::NATIVE_DETAILS_SHOWN:
      TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
      break;

    case AddToHomescreenInstaller::Event::UI_SHOWN:
      RecordDidShowBanner();
      TrackDisplayEvent(a2hs_params.app_type ==
                                AddToHomescreenParams::AppType::NATIVE
                            ? DISPLAY_EVENT_NATIVE_APP_BANNER_CREATED
                            : DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
      break;

    case AddToHomescreenInstaller::Event::UI_CANCELLED:
      TrackDismissEvent(DISMISS_EVENT_DISMISSED);

      SendBannerDismissed();
      if (a2hs_params.app_type == AddToHomescreenParams::AppType::NATIVE) {
        DCHECK(!a2hs_params.native_app_package_name.empty());
        TrackUserResponse(USER_RESPONSE_NATIVE_APP_DISMISSED);
        AppBannerSettingsHelper::RecordBannerDismissEvent(
            web_contents(), a2hs_params.native_app_package_name);
      } else {
        TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
        AppBannerSettingsHelper::RecordBannerDismissEvent(
            web_contents(), a2hs_params.shortcut_info->url.spec());
      }
      break;
  }
}

void AppBannerManagerAndroid::CreateJavaBannerManager(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_banner_manager_.Reset(
      Java_AppBannerManager_create(env, reinterpret_cast<intptr_t>(this)));
}

std::string AppBannerManagerAndroid::ExtractQueryValueForName(
    const GURL& url,
    const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name)
      return std::string(it.GetValue());
  }
  return std::string();
}

bool AppBannerManagerAndroid::ShouldPerformInstallableNativeAppCheck() {
  if (!manifest().prefer_related_applications || java_banner_manager_.is_null())
    return false;

  // Ensure there is at least one related app specified that is supported on
  // the current platform.
  for (const auto& application : manifest().related_applications) {
    if (base::EqualsASCII(application.platform.value_or(std::u16string()),
                          kPlatformPlay))
      return true;
  }
  return false;
}

void AppBannerManagerAndroid::PerformInstallableNativeAppCheck() {
  DCHECK(ShouldPerformInstallableNativeAppCheck());
  InstallableStatusCode code = NO_ERROR_DETECTED;
  for (const auto& application : manifest().related_applications) {
    std::string id =
        base::UTF16ToUTF8(application.id.value_or(std::u16string()));
    code = QueryNativeApp(application.platform.value_or(std::u16string()),
                          application.url, id);
    if (code == NO_ERROR_DETECTED)
      return;
  }

  // We must have some error in |code| if we reached this point, so report it.
  Stop(code);
}

InstallableStatusCode AppBannerManagerAndroid::QueryNativeApp(
    const std::u16string& platform,
    const GURL& url,
    const std::string& id) {
  if (!base::EqualsASCII(platform, kPlatformPlay))
    return PLATFORM_NOT_SUPPORTED_ON_ANDROID;

  if (id.empty())
    return NO_ID_SPECIFIED;

  // AppBannerManager#fetchAppDetails() only works on Beta and Stable because
  // the called Google Play API uses an old way of checking whether the Chrome
  // app is first party. See http://b/147780265
  // Run AppBannerManager#fetchAppDetails() for local builds regardless of
  // Android channel to avoid having to set android_channel GN flag for manual
  // testing. Do not run AppBannerManager#fetchAppDetails() for non-official
  // local builds to ensure that tests use gIgnoreChromeChannelForTesting rather
  // than relying on the local build exemption.
  version_info::Channel channel = version_info::android::GetChannel();
  bool local_build = channel == version_info::Channel::UNKNOWN &&
                     version_info::IsOfficialBuild();
  if (!(local_build || gIgnoreChromeChannelForTesting ||
        channel == version_info::Channel::BETA ||
        channel == version_info::Channel::STABLE)) {
    return PREFER_RELATED_APPLICATIONS_SUPPORTED_ONLY_BETA_STABLE;
  }

  TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);

  std::string id_from_app_url = ExtractQueryValueForName(url, "id");
  if (id_from_app_url.size() && id != id_from_app_url)
    return IDS_DO_NOT_MATCH;

  // Attach the chrome_inline referrer value, prefixed with "&" if the
  // referrer is non empty.
  std::string referrer = ExtractQueryValueForName(url, "referrer");
  if (!referrer.empty())
    referrer += "&";
  referrer += "playinline=chrome_inline";

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, validated_url_.spec()));
  base::android::ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  base::android::ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer));

  // This async call will run OnAppDetailsRetrieved() when completed.
  UpdateState(State::FETCHING_NATIVE_DATA);
  Java_AppBannerManager_fetchAppDetails(
      env, java_banner_manager_, jurl, jpackage, jreferrer,
      WebappsIconUtils::GetIdealHomescreenIconSizeInPx());
  return NO_ERROR_DETECTED;
}

void AppBannerManagerAndroid::OnNativeAppIconFetched(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    Stop(NO_ICON_AVAILABLE);
    return;
  }

  if (!skia::SkBitmapToN32OpaqueOrPremul(bitmap, &primary_icon_))
    return;

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Terminate(). We wait for OnEngagementEvent to tell us that we
  // should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  SendBannerPromptRequest();
}

std::u16string AppBannerManagerAndroid::GetAppName() const {
  if (native_app_data_.is_null()) {
    // Prefer the short name if it's available. It's guaranteed that at least
    // one of these is non-empty.
    std::u16string short_name = manifest().short_name.value_or(u"");
    return short_name.empty() ? manifest().name.value_or(u"") : short_name;
  }

  return native_app_title_;
}

bool AppBannerManagerAndroid::MaybeShowPwaBottomSheetController(
    bool expand_sheet,
    WebappInstallSource install_source) {
  // Do not show the peeked bottom sheet if it was recently dismissed.
  if (!expand_sheet && AppBannerSettingsHelper::WasBannerRecentlyBlocked(
                           web_contents(), validated_url_, GetAppIdentifier(),
                           GetCurrentTime())) {
    return false;
  }

  auto a2hs_params = CreateAddToHomescreenParams(install_source);
  return PwaBottomSheetController::MaybeShow(
      web_contents(), GetAppName(), primary_icon_, has_maskable_primary_icon_,
      manifest().start_url, screenshots_, manifest().description.value_or(u""),
      expand_sheet, std::move(a2hs_params),
      base::BindRepeating(&AppBannerManagerAndroid::OnInstallEvent,
                          AppBannerManagerAndroid::GetAndroidWeakPtr()));
}

void AppBannerManagerAndroid::Install(
    const AddToHomescreenParams& a2hs_params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        a2hs_event_callback) {
  AddToHomescreenInstaller::Install(web_contents(), a2hs_params,
                                    std::move(a2hs_event_callback));
}

void AppBannerManagerAndroid::MaybeShowAmbientBadge() {
  if (!base::FeatureList::IsEnabled(
          features::kInstallableAmbientBadgeInfoBar) &&
      !base::FeatureList::IsEnabled(
          features::kInstallableAmbientBadgeMessage)) {
    return;
  }

  // Do not show the ambient badge if it was recently dismissed.
  if (AppBannerSettingsHelper::WasBannerRecentlyBlocked(
          web_contents(), validated_url_, GetAppIdentifier(),
          GetCurrentTime())) {
    return;
  }

  infobars::ContentInfoBarManager* infobar_manager =
      webapps::WebappsClient::Get()->GetInfoBarManagerForWebContents(
          web_contents());
  bool infobar_visible =
      infobar_manager &&
      InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
          infobar_manager);

  if (infobar_visible || message_controller_.IsMessageEnqueued())
    return;

  // Only show if it's native app, or the worker check already passed.
  if (!features::SkipServiceWorkerForInstallPromotion() ||
      passed_worker_check_ || native_app_data_) {
    ShowAmbientBadge();
  }
}

void AppBannerManagerAndroid::HideAmbientBadge() {
  message_controller_.DismissMessage();
  infobars::ContentInfoBarManager* infobar_manager =
      webapps::WebappsClient::Get()->GetInfoBarManagerForWebContents(
          web_contents());
  if (infobar_manager == nullptr)
    return;

  infobars::InfoBar* ambient_badge_infobar =
      InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
          infobar_manager);

  if (ambient_badge_infobar)
    infobar_manager->RemoveInfoBar(ambient_badge_infobar);
}

bool AppBannerManagerAndroid::IsSupportedNonWebAppPlatform(
    const std::u16string& platform) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

bool AppBannerManagerAndroid::IsRelatedNonWebAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  // TODO(https://crbug.com/949430): Implement for Android apps.
  return false;
}

bool AppBannerManagerAndroid::IsWebAppConsideredInstalled() const {
  // Also check if a WebAPK is currently being installed. Installation may take
  // some time, so ensure we don't accidentally allow a new installation whilst
  // one is in flight for the current site.
  return WebappsUtils::IsWebApkInstalled(web_contents()->GetBrowserContext(),
                                         manifest().start_url) ||
         WebappsClient::Get()->IsInstallationInProgress(
             web_contents(), manifest_url_, manifest_id_);
}

void AppBannerManagerAndroid::ShowAmbientBadge() {
  if (base::FeatureList::IsEnabled(features::kInstallableAmbientBadgeMessage) &&
      base::FeatureList::IsEnabled(
          messages::kMessagesForAndroidInfrastructure)) {
    message_controller_.EnqueueMessage(
        web_contents(), GetAppName(), primary_icon_, has_maskable_primary_icon_,
        manifest().start_url);
  } else {
    InstallableAmbientBadgeInfoBarDelegate::Create(
        web_contents(), weak_factory_.GetWeakPtr(), GetAppName(), primary_icon_,
        has_maskable_primary_icon_, manifest().start_url);
  }
}

void AppBannerManagerAndroid::RecordExtraMetricsForInstallEvent(
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {}

base::WeakPtr<AppBannerManager> AppBannerManagerAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<AppBannerManagerAndroid>
AppBannerManagerAndroid::GetAndroidWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerAndroid::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

// static
base::android::ScopedJavaLocalRef<jobject>
JNI_AppBannerManager_GetJavaBannerManagerForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  auto* manager =
      static_cast<AppBannerManagerAndroid*>(AppBannerManager::FromWebContents(
          content::WebContents::FromJavaWebContents(java_web_contents)));
  return manager ? manager->GetJavaBannerManager()
                 : base::android::ScopedJavaLocalRef<jobject>();
}

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_AppBannerManager_GetInstallableWebAppName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  return base::android::ConvertUTF16ToJavaString(
      env, AppBannerManager::GetInstallableWebAppName(
               content::WebContents::FromJavaWebContents(java_web_contents)));
}

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_AppBannerManager_GetInstallableWebAppManifestId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  return base::android::ConvertUTF8ToJavaString(
      env, AppBannerManager::GetInstallableWebAppManifestId(
               content::WebContents::FromJavaWebContents(java_web_contents)));
}

// static
void JNI_AppBannerManager_IgnoreChromeChannelForTesting(JNIEnv*) {
  gIgnoreChromeChannelForTesting = true;
}

// static
void JNI_AppBannerManager_SetDaysAfterDismissAndIgnoreToTrigger(
    JNIEnv* env,
    jint dismiss_days,
    jint ignore_days) {
  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(dismiss_days,
                                                                 ignore_days);
}

// static
void JNI_AppBannerManager_SetTimeDeltaForTesting(JNIEnv* env, jint days) {
  AppBannerManager::SetTimeDeltaForTesting(days);
}

// static
void JNI_AppBannerManager_SetTotalEngagementToTrigger(JNIEnv* env,
                                                      jdouble engagement) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(engagement);
}

}  // namespace webapps
