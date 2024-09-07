// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/app_banner_manager_android.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "components/webapps/browser/android/add_to_homescreen_coordinator.h"
#include "components/webapps/browser/android/add_to_homescreen_params.h"
#include "components/webapps/browser/android/ambient_badge_manager.h"
#include "components/webapps/browser/android/bottomsheet/pwa_bottom_sheet_controller.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/banners/install_banner_config.h"
#include "components/webapps/browser/banners/native_app_banner_data.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/url_util.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/AppBannerManager_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

namespace webapps {

namespace {

constexpr char kPlatformPlay[] = "play";

// Whether to ignore the Chrome channel in QueryNativeApp() for testing.
bool gIgnoreChromeChannelForTesting = false;

std::string ExtractQueryValueForName(const GURL& url, const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name) {
      return std::string(it.GetValue());
    }
  }
  return std::string();
}

}  // anonymous namespace

// static
void AppBannerManagerAndroid::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<ChromeDelegate> delegate) {
  if (FromWebContents(web_contents)) {
    return;
  }
  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new AppBannerManagerAndroid(
                                web_contents, std::move(delegate))));
}

AppBannerManagerAndroid::AppBannerManagerAndroid(
    content::WebContents* web_contents,
    std::unique_ptr<ChromeDelegate> delegate)
    : AppBannerManager(web_contents),
      content::WebContentsUserData<AppBannerManagerAndroid>(*web_contents),
      delegate_(std::move(delegate)) {
  CreateJavaBannerManager(web_contents);
}

AppBannerManagerAndroid::~AppBannerManagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppBannerManager_destroy(env, java_banner_manager_);
  java_banner_manager_.Reset();
}

AppBannerManagerAndroid::QueryNativeAppConfig::QueryNativeAppConfig(
    const base::android::ScopedJavaLocalRef<jstring>& url,
    const base::android::ScopedJavaLocalRef<jstring>& package,
    const base::android::ScopedJavaLocalRef<jstring>& referrer)
    : url(url), package(package), referrer(referrer) {}

AppBannerManagerAndroid::QueryNativeAppConfig::QueryNativeAppConfig(
    const QueryNativeAppConfig& config) = default;
AppBannerManagerAndroid::QueryNativeAppConfig::~QueryNativeAppConfig() =
    default;

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

int AppBannerManagerAndroid::GetBadgeStatusForTesting(JNIEnv* env) {
  if (!ambient_badge_manager_) {
    return 0;
  }
  return (int)ambient_badge_manager_->state();
}

void AppBannerManagerAndroid::OnAppDetailsRetrieved(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int request_id,
    const JavaParamRef<jobject>& japp_data,
    const JavaParamRef<jstring>& japp_title,
    const JavaParamRef<jstring>& japp_package,
    const JavaParamRef<jstring>& jicon_url) {
  // If the state isn't fetching native data, that means the page must have
  // navigated or reset in some way.
  if (state() != State::FETCHING_NATIVE_DATA) {
    return;
  }
  if (request_id != current_native_request_id_) {
    return;
  }
  if (!native_check_callback_storage_) {
    return;
  }
  current_native_request_id_ = std::nullopt;
  native_java_app_data_.Reset(japp_data);
  std::string app_package = ConvertJavaStringToUTF8(env, japp_package);
  std::u16string app_title = ConvertJavaStringToUTF16(env, japp_title);
  GURL primary_icon_url = GURL(ConvertJavaStringToUTF8(env, jicon_url));

  if (app_package.empty()) {
    std::move(native_check_callback_storage_)
        .Run(base::unexpected(
            InstallableStatusCode::PACKAGE_NAME_OR_START_URL_EMPTY));
    return;
  }

  bool icon_download_initiated = content::ManifestIconDownloader::Download(
      &GetWebContents(), primary_icon_url,
      WebappsIconUtils::GetIdealHomescreenIconSizeInPx(),
      WebappsIconUtils::GetMinimumHomescreenIconSizeInPx(),
      /* maximum_icon_size_in_px= */ std::numeric_limits<int>::max(),
      base::BindOnce(&AppBannerManagerAndroid::OnNativeAppIconFetched,
                     GetAndroidWeakPtr(), std::move(app_package),
                     std::move(app_title), primary_icon_url));
  if (!icon_download_initiated) {
    std::move(native_check_callback_storage_)
        .Run(base::unexpected(InstallableStatusCode::CANNOT_DOWNLOAD_ICON));
  }
}

void AppBannerManagerAndroid::ShowBannerFromBadge(
    const InstallBannerConfig& config) {
  ShowBannerUi(InstallableMetrics::GetInstallSource(
                   web_contents(), InstallTrigger::AMBIENT_BADGE),
               config);

  // Close our bindings to ensure that any existing beforeinstallprompt events
  // cannot trigger add to home screen (which would cause a crash). If the
  // banner is dismissed, the event will be resent.
  ResetBindings();
}

// static

std::unique_ptr<AddToHomescreenParams>
AppBannerManagerAndroid::CreateAddToHomescreenParams(
    const InstallBannerConfig& config,
    const base::android::ScopedJavaGlobalRef<jobject>& native_java_app_data,
    WebappInstallSource install_source) {
  if (native_java_app_data.is_null()) {
    CHECK(config.mode == AppBannerMode::kWebApp);
    const WebAppBannerData& web_app_data = config.web_app_data;
    return std::make_unique<AddToHomescreenParams>(
        AddToHomescreenParams::AppType::WEBAPK,
        ShortcutInfo::CreateShortcutInfo(
            config.validated_url, web_app_data.manifest_url,
            web_app_data.manifest(), web_app_data.web_page_metadata(),
            web_app_data.primary_icon_url,
            web_app_data.has_maskable_primary_icon),
        web_app_data.primary_icon, InstallableStatusCode::NO_ERROR_DETECTED,
        install_source);
  } else {
    CHECK(config.mode == AppBannerMode::kNativeApp);
    CHECK(config.native_app_data);
    const NativeAppBannerData& native_app_data = *config.native_app_data;
    return std::make_unique<AddToHomescreenParams>(
        native_app_data.app_package, native_java_app_data,
        native_app_data.primary_icon, install_source);
  }
}

bool AppBannerManagerAndroid::CanRequestAppBanner() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Note: This check is actually for "A2HS" aka add shortcuts. It doesn't
  // really belongs here.
  if (!Java_AppBannerManager_isSupported(env) ||
      !WebappsClient::Get()->CanShowAppBanners(&GetWebContents())) {
    return false;
  }
  return true;
}

InstallableParams
AppBannerManagerAndroid::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params;
  params.valid_primary_icon = true;
  params.installable_criteria =
      InstallableCriteria::kImplicitManifestFieldsHTML;
  params.fetch_screenshots = true;
  params.prefer_maskable_icon =
      WebappsIconUtils::DoesAndroidSupportMaskableIcons();
  params.fetch_favicon = true;
  return params;
}

bool AppBannerManagerAndroid::ShouldDoNativeAppCheck(
    const blink::mojom::Manifest& manifest) const {
  if (!manifest.prefer_related_applications || java_banner_manager_.is_null()) {
    return false;
  }
  // Ensure there is at least one related app specified that is supported on
  // the current platform.
  for (const auto& application : manifest.related_applications) {
    if (base::EqualsASCII(application.platform.value_or(std::u16string()),
                          kPlatformPlay)) {
      return true;
    }
  }
  return false;
}

void AppBannerManagerAndroid::DoNativeAppInstallableCheck(
    content::WebContents* web_contents,
    const GURL& validated_url,
    const blink::mojom::Manifest& manifest,
    NativeCheckCallback callback) {
  CHECK(manifest.prefer_related_applications &&
        !java_banner_manager_.is_null());

  InstallableStatusCode code = InstallableStatusCode::NO_ERROR_DETECTED;
  for (const auto& application : manifest.related_applications) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::expected<QueryNativeAppConfig, InstallableStatusCode> result =
        GetNativeAppFetchRequestConfig(validated_url, env, application);

    if (!result.has_value()) {
      code = result.error();
      continue;
    }

    TrackDisplayEvent(DISPLAY_EVENT_NATIVE_APP_BANNER_REQUESTED);
    // Send the info to the Java side to get info about the app.
    // This async call will run OnAppDetailsRetrieved() when completed.
    current_native_request_id_ = next_native_request_id_;
    ++next_native_request_id_;
    native_check_callback_storage_ = std::move(callback);
    Java_AppBannerManager_fetchAppDetails(
        env, java_banner_manager_, current_native_request_id_.value(),
        result.value().url, result.value().package, result.value().referrer,
        WebappsIconUtils::GetIdealHomescreenIconSizeInPx());
    return;
  }
  CHECK(callback);
  std::move(callback).Run(base::unexpected(code));
}

void AppBannerManagerAndroid::OnWebAppInstallableCheckedNoErrors(
    const ManifestId& manifest_id) const {
  delegate_->OnInstallableCheckedNoErrors(manifest_id);
}

base::expected<void, InstallableStatusCode>
AppBannerManagerAndroid::CanRunWebAppInstallableChecks(
    const blink::mojom::Manifest& manifest) {
  if (!webapps::WebappsUtils::AreWebManifestUrlsWebApkCompatible(manifest)) {
    return base::unexpected(
        InstallableStatusCode::URL_NOT_SUPPORTED_FOR_WEBAPK);
  }
  return base::ok();
}

bool AppBannerManagerAndroid::IsSupportedNonWebAppPlatform(
    const std::u16string& platform) const {
  return base::EqualsASCII(platform, kPlatformPlay);
}

bool AppBannerManagerAndroid::IsRelatedNonWebAppInstalled(
    const blink::Manifest::RelatedApplication& related_app) const {
  if (!related_app.id || related_app.id->empty()) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_id(
      ConvertUTF16ToJavaString(env, related_app.id.value()));
  return Java_AppBannerManager_isRelatedNonWebAppInstalled(env, java_id);
}

void AppBannerManagerAndroid::MaybeShowAmbientBadge(
    const InstallBannerConfig& install_config) {
  // Since this can be triggered in some weird async ways, check against the
  // current config, and if their manifest_id's don't match then do not proceed.
  std::optional<InstallBannerConfig> current_config = GetCurrentBannerConfig();
  if (!current_config || install_config.web_app_data.manifest_id !=
                             current_config->web_app_data.manifest_id) {
    // TODO(https://crbug.com/324322110): remove once crash understood.
    DUMP_WILL_BE_CHECK(false) << "Pipeline state:" << (int)state();
    return;
  }

  ambient_badge_manager_ = std::make_unique<AmbientBadgeManager>(
      GetWebContents(), delegate_->GetSegmentationPlatformService(),
      *delegate_->GetPrefService());

  std::unique_ptr<AddToHomescreenParams> a2hs_params =
      AppBannerManagerAndroid::CreateAddToHomescreenParams(
          install_config, native_java_app_data_,
          InstallableMetrics::GetInstallSource(&GetWebContents(),
                                               InstallTrigger::AMBIENT_BADGE));

  ambient_badge_manager_->MaybeShow(
      install_config.validated_url, install_config.GetWebOrNativeAppName(),
      install_config.GetWebOrNativeAppIdentifier(), std::move(a2hs_params),
      // TODO(b/323192242): See if these callbacks can be merged.
      base::BindOnce(&AppBannerManagerAndroid::ShowBannerFromBadge,

                     GetAndroidWeakPtr(), install_config),
      // Create the params, then pass them to MaybeShow.
      base::BindOnce(&AppBannerManagerAndroid::CreateAddToHomescreenParams,
                     install_config, native_java_app_data_)
          .Then(base::BindOnce(
              &PwaBottomSheetController::MaybeShow, web_contents(),
              install_config.web_app_data, /*expand_sheet=*/false,
              base::BindRepeating(&AppBannerManagerAndroid::OnInstallEvent,
                                  GetAndroidWeakPtr(),
                                  install_config.validated_url))));
}

void AppBannerManagerAndroid::ShowBannerUi(WebappInstallSource install_source,
                                           const InstallBannerConfig& config) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  bool was_shown = native_java_app_data_.is_null() &&
                   MaybeShowPwaBottomSheetController(/* expand_sheet= */ true,
                                                     install_source, config);

  if (!was_shown) {
    auto a2hs_params = AppBannerManagerAndroid::CreateAddToHomescreenParams(
        config, native_java_app_data_, install_source);
    was_shown = AddToHomescreenCoordinator::ShowForAppBanner(
        weak_factory_.GetWeakPtr(), std::move(a2hs_params),
        base::BindRepeating(&AppBannerManagerAndroid::OnInstallEvent,
                            weak_factory_.GetWeakPtr(), config.validated_url));
  }

  // If we are installing from the ambient badge, it will remove itself.
  if (install_source != WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB &&
      install_source != WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB &&
      install_source != WebappInstallSource::RICH_INSTALL_UI_WEBLAYER) {
    if (ambient_badge_manager_) {
      ambient_badge_manager_->HideAmbientBadge();
    }
  }

  if (was_shown) {
    if (native_java_app_data_.is_null()) {
      ReportStatus(InstallableStatusCode::SHOWING_WEB_APP_BANNER);
    } else {
      ReportStatus(InstallableStatusCode::SHOWING_NATIVE_APP_BANNER);
    }
  } else {
    ReportStatus(InstallableStatusCode::FAILED_TO_CREATE_BANNER);
  }
}

void AppBannerManagerAndroid::ResetCurrentPageData() {
  current_native_request_id_ = std::nullopt;
  ambient_badge_manager_.reset();
  native_check_callback_storage_.Reset();
  native_java_app_data_.Reset();
}

void AppBannerManagerAndroid::OnInstallEvent(
    GURL validated_url,
    AddToHomescreenInstaller::Event event,
    const AddToHomescreenParams& a2hs_params) {
  delegate_->RecordExtraMetricsForInstallEvent(event, a2hs_params);

  // If the app is not native and the install source is a menu install source,
  // user interacted with the bottom sheet installer UI.
  if (a2hs_params.app_type != AddToHomescreenParams::AppType::NATIVE &&
      (a2hs_params.install_source == WebappInstallSource::MENU_BROWSER_TAB ||
       a2hs_params.install_source == WebappInstallSource::MENU_CUSTOM_TAB)) {
    switch (event) {
      case AddToHomescreenInstaller::Event::INSTALL_REQUEST_FINISHED:
        SendBannerAccepted();
        OnInstall(a2hs_params.shortcut_info->display,
                  /*set_current_web_app_not_installable=*/false);
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

  std::string identifier;
  if (a2hs_params.app_type == AddToHomescreenParams::AppType::NATIVE) {
    DCHECK(!a2hs_params.native_app_package_name.empty());
    identifier = a2hs_params.native_app_package_name;
  } else {
    DCHECK(a2hs_params.shortcut_info->manifest_id.is_valid());
    identifier = a2hs_params.shortcut_info->manifest_id.spec();
  }

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
          NOTREACHED_IN_MIGRATION();
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
        OnInstall(a2hs_params.shortcut_info->display,
                  /*set_current_web_app_not_installable=*/false);
      }
      break;

    case AddToHomescreenInstaller::Event::NATIVE_DETAILS_SHOWN:
      TrackDismissEvent(DISMISS_EVENT_BANNER_CLICK);
      break;

    case AddToHomescreenInstaller::Event::UI_SHOWN:
      AppBannerSettingsHelper::RecordBannerEvent(
          web_contents(), validated_url, identifier,
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, GetCurrentTime());
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

base::expected<AppBannerManagerAndroid::QueryNativeAppConfig,
               InstallableStatusCode>
AppBannerManagerAndroid::GetNativeAppFetchRequestConfig(
    const GURL& validated_url,
    JNIEnv* env,
    const blink::Manifest::RelatedApplication& related_application) const {
  if (!related_application.platform.has_value() ||
      !base::EqualsASCII(related_application.platform.value(), kPlatformPlay)) {
    return base::unexpected(
        InstallableStatusCode::PLATFORM_NOT_SUPPORTED_ON_ANDROID);
  }
  std::string id =
      base::UTF16ToUTF8(related_application.id.value_or(std::u16string()));
  if (id.empty()) {
    return base::unexpected(InstallableStatusCode::NO_ID_SPECIFIED);
  }

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
    return base::unexpected(
        InstallableStatusCode::
            PREFER_RELATED_APPLICATIONS_SUPPORTED_ONLY_BETA_STABLE);
  }

  std::string id_from_app_url =
      ExtractQueryValueForName(related_application.url, "id");
  if (id_from_app_url.size() && id != id_from_app_url) {
    return base::unexpected(InstallableStatusCode::IDS_DO_NOT_MATCH);
  }

  // Attach the chrome_inline referrer value, prefixed with "&" if the
  // referrer is non empty.
  std::string referrer =
      ExtractQueryValueForName(related_application.url, "referrer");
  if (!referrer.empty()) {
    referrer += "&";
  }
  referrer += "playinline=chrome_inline";

  base::android::ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, validated_url.spec()));
  base::android::ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, id));
  base::android::ScopedJavaLocalRef<jstring> jreferrer(
      ConvertUTF8ToJavaString(env, referrer));

  return QueryNativeAppConfig(jurl, jpackage, jreferrer);
}

void AppBannerManagerAndroid::OnNativeAppIconFetched(std::string app_package,
                                                     std::u16string app_title,
                                                     GURL primary_icon_url,
                                                     const SkBitmap& bitmap) {
  if (!native_check_callback_storage_) {
    return;
  }
  if (bitmap.drawsNothing()) {
    std::move(native_check_callback_storage_)
        .Run(base::unexpected(InstallableStatusCode::NO_ICON_AVAILABLE));
    return;
  }

  SkBitmap primary_icon;
  if (!skia::SkBitmapToN32OpaqueOrPremul(bitmap, &primary_icon)) {
    std::move(native_check_callback_storage_)
        .Run(base::unexpected(InstallableStatusCode::NO_ICON_AVAILABLE));
    return;
  }

  std::move(native_check_callback_storage_)
      .Run(base::ok(NativeAppBannerData(
          std::move(app_package), std::move(app_title),
          std::move(primary_icon_url), std::move(primary_icon))));
}

bool AppBannerManagerAndroid::MaybeShowPwaBottomSheetController(
    bool expand_sheet,
    WebappInstallSource install_source,
    const InstallBannerConfig& config) {
  if (config.mode == AppBannerMode::kNativeApp) {
    return false;
  }

  const WebAppBannerData& web_app_data = config.web_app_data;

  // Do not show the peeked bottom sheet if it was recently dismissed.
  if (!expand_sheet && AppBannerSettingsHelper::WasBannerRecentlyBlocked(
                           &GetWebContents(), config.validated_url,
                           web_app_data.manifest_id.spec(),
                           AppBannerManager::GetCurrentTime())) {
    return false;
  }

  auto a2hs_params = AppBannerManagerAndroid::CreateAddToHomescreenParams(
      config, native_java_app_data_, install_source);

  return PwaBottomSheetController::MaybeShow(
      web_contents(), web_app_data, expand_sheet,
      base::BindRepeating(&AppBannerManagerAndroid::OnInstallEvent,
                          AppBannerManagerAndroid::GetAndroidWeakPtr(),
                          config.validated_url),
      std::move(a2hs_params));
}

void AppBannerManagerAndroid::OnMlInstallPrediction(
    base::PassKey<MLInstallabilityPromoter>,
    std::string result_label) {
  // TODO(crbug.com/40269982): Implement.
}

void AppBannerManagerAndroid::Install(
    const AddToHomescreenParams& a2hs_params,
    base::RepeatingCallback<void(AddToHomescreenInstaller::Event,
                                 const AddToHomescreenParams&)>
        a2hs_event_callback) {
  AddToHomescreenInstaller::Install(web_contents(), a2hs_params,
                                    std::move(a2hs_event_callback));
}

base::WeakPtr<AppBannerManager>
AppBannerManagerAndroid::GetWeakPtrForThisNavigation() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<AppBannerManagerAndroid>
AppBannerManagerAndroid::GetAndroidWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppBannerManagerAndroid::InvalidateWeakPtrsForThisNavigation() {
  weak_factory_.InvalidateWeakPtrs();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerAndroid);

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

// static
void JNI_AppBannerManager_SetOverrideSegmentationResultForTesting(  // IN-TEST
    JNIEnv* env,
    jboolean show) {
  AmbientBadgeManager::SetOverrideSegmentationResultForTesting(show);
}

}  // namespace webapps
