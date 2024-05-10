// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_proto_builder.h"

#include <string>

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/version_info/version_info.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/android/webapp_icon.h"
#include "components/webapps/browser/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/android/color_utils_android.h"
#include "ui/gfx/codec/png_codec.h"

namespace webapps {
namespace {

// Limit the icon size to 512KB.
constexpr size_t kMaxIconSizeInBytes = 512 * 1024;

webapk::WebApk_UpdateReason ConvertUpdateReasonToProtoEnum(
    WebApkUpdateReason update_reason) {
  switch (update_reason) {
    case WebApkUpdateReason::NONE:
      return webapk::WebApk::NONE;
    case WebApkUpdateReason::OLD_SHELL_APK:
      return webapk::WebApk::OLD_SHELL_APK;
    case WebApkUpdateReason::PRIMARY_ICON_HASH_DIFFERS:
      return webapk::WebApk::PRIMARY_ICON_HASH_DIFFERS;
    case WebApkUpdateReason::PRIMARY_ICON_MASKABLE_DIFFERS:
      return webapk::WebApk::PRIMARY_ICON_MASKABLE_DIFFERS;
    case WebApkUpdateReason::SPLASH_ICON_HASH_DIFFERS:
      return webapk::WebApk::SPLASH_ICON_HASH_DIFFERS;
    case WebApkUpdateReason::SCOPE_DIFFERS:
      return webapk::WebApk::SCOPE_DIFFERS;
    case WebApkUpdateReason::START_URL_DIFFERS:
      return webapk::WebApk::START_URL_DIFFERS;
    case WebApkUpdateReason::SHORT_NAME_DIFFERS:
      return webapk::WebApk::SHORT_NAME_DIFFERS;
    case WebApkUpdateReason::NAME_DIFFERS:
      return webapk::WebApk::NAME_DIFFERS;
    case WebApkUpdateReason::BACKGROUND_COLOR_DIFFERS:
      return webapk::WebApk::BACKGROUND_COLOR_DIFFERS;
    case WebApkUpdateReason::THEME_COLOR_DIFFERS:
      return webapk::WebApk::THEME_COLOR_DIFFERS;
    case WebApkUpdateReason::ORIENTATION_DIFFERS:
      return webapk::WebApk::ORIENTATION_DIFFERS;
    case WebApkUpdateReason::DISPLAY_MODE_DIFFERS:
      return webapk::WebApk::DISPLAY_MODE_DIFFERS;
    case WebApkUpdateReason::WEB_SHARE_TARGET_DIFFERS:
      return webapk::WebApk::WEB_SHARE_TARGET_DIFFERS;
    case WebApkUpdateReason::MANUALLY_TRIGGERED:
      return webapk::WebApk::MANUALLY_TRIGGERED;
    case WebApkUpdateReason::SHORTCUTS_DIFFER:
      return webapk::WebApk::SHORTCUTS_DIFFER;
    case WebApkUpdateReason::DARK_BACKGROUND_COLOR_DIFFERS:
      return webapk::WebApk::DARK_BACKGROUND_COLOR_DIFFERS;
    case WebApkUpdateReason::DARK_THEME_COLOR_DIFFERS:
      return webapk::WebApk::DARK_THEME_COLOR_DIFFERS;
    case WebApkUpdateReason::PRIMARY_ICON_CHANGE_BELOW_THRESHOLD:
      return webapk::WebApk::PRIMARY_ICON_CHANGE_BELOW_THRESHOLD;
    case WebApkUpdateReason::PRIMARY_ICON_CHANGE_SHELL_UPDATE:
      return webapk::WebApk::PRIMARY_ICON_CHANGE_SHELL_UPDATE;
  }
}

// Get Chrome's current ABI. It depends on whether Chrome is running as a 32 bit
// app or 64 bit, and the device's cpu architecture as well. Note: please keep
// this function stay in sync with |chromium_android_linker::GetCpuAbi()|.
std::string getCurrentAbi() {
#if defined(__arm__) && defined(__ARM_ARCH_7A__)
  return "armeabi-v7a";
#elif defined(__arm__)
  return "armeabi";
#elif defined(__i386__)
  return "x86";
#elif defined(__mips__)
  return "mips";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "arm64-v8a";
#elif defined(__riscv) && (__riscv_xlen == 64)
  return "riscv64";
#else
#error "Unsupported target abi"
#endif
}

void AddWebApkIcon(webapk::WebAppManifest* web_app_manifest,
                   const WebappIcon* webapk_icon) {
  CHECK(webapk_icon);
  webapk::Image* icon_image = web_app_manifest->add_icons();
  if (webapk_icon->url().is_valid()) {
    icon_image->set_src(webapk_icon->url().spec());
  }
  if (!webapk_icon->hash().empty()) {
    icon_image->set_hash(webapk_icon->hash());
  }
  if (webapk_icon->has_unsafe_data()) {
    icon_image->set_image_data(webapk_icon->unsafe_data());
  }
  for (auto usage : webapk_icon->usages()) {
    icon_image->add_usages(usage);
  }
  icon_image->add_purposes(webapk_icon->purpose());
}

void AddShortcutIcon(webapk::Image* icon_image,
                     const GURL& icon_url,
                     std::map<GURL, std::unique_ptr<WebappIcon>>& icons) {
  icon_image->set_src(icon_url.spec());

  auto it = icons.find(icon_url);
  if (it != icons.end()) {
    icon_image->set_hash(it->second->hash());
    if (it->second->unsafe_data().size() <= kMaxIconSizeInBytes) {
      icon_image->set_image_data(it->second->ExtractData());
    }
    for (auto usage : it->second->usages()) {
      icon_image->add_usages(usage);
    }
    icon_image->add_purposes(it->second->purpose());
  }
}

// Add other WebAPK images that are not used in primary, splash or shortcuts.
// We include only the url and hash for them.
// TODO(eirage): we had these icons in the request proto to compare with harpoon
// result, they are not needed now and should be removed.
void AddOtherWebApkImage(webapk::WebAppManifest* web_app_manifest,
                         const GURL& icon_url,
                         std::map<GURL, std::unique_ptr<WebappIcon>>& icons) {
  webapk::Image* icon_image = web_app_manifest->add_icons();
  icon_image->set_src(icon_url.spec());
  auto it = icons.find(icon_url);
  if (it != icons.end()) {
    icon_image->set_hash(it->second->hash());
  }
}

}  // namespace

std::unique_ptr<std::string> BuildProtoInBackground(
    const ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<WebApkUpdateReason> update_reasons) {
  std::unique_ptr<webapk::WebApk> webapk(new webapk::WebApk);
  webapk->set_manifest_url(shortcut_info.manifest_url.spec());
  webapk->set_requester_application_package(
      base::android::BuildInfo::GetInstance()->package_name());
  webapk->set_requester_application_version(
      std::string(version_info::GetVersionNumber()));
  webapk->set_android_abi(getCurrentAbi());
  webapk->set_package_name(package_name);
  webapk->set_version(version);
  webapk->set_stale_manifest(is_manifest_stale);
  webapk->set_app_identity_update_supported(is_app_identity_update_supported);
  webapk->set_android_version(base::SysInfo::OperatingSystemVersion());

  for (auto update_reason : update_reasons) {
    webapk->add_update_reasons(ConvertUpdateReasonToProtoEnum(update_reason));
  }

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  web_app_manifest->set_has_custom_name(shortcut_info.has_custom_title);
  if (shortcut_info.has_custom_title) {
    web_app_manifest->set_name(base::UTF16ToUTF8(shortcut_info.user_title));
    web_app_manifest->set_short_name(
        base::UTF16ToUTF8(shortcut_info.user_title));
  } else {
    web_app_manifest->set_name(base::UTF16ToUTF8(shortcut_info.name));
    web_app_manifest->set_short_name(
        base::UTF16ToUTF8(shortcut_info.short_name));
  }
  web_app_manifest->set_start_url(shortcut_info.url.spec());
  web_app_manifest->set_orientation(
      blink::WebScreenOrientationLockTypeToString(shortcut_info.orientation));
  web_app_manifest->set_display_mode(
      blink::DisplayModeToString(shortcut_info.display));
  web_app_manifest->set_background_color(
      ui::OptionalSkColorToString(shortcut_info.background_color));
  web_app_manifest->set_theme_color(
      ui::OptionalSkColorToString(shortcut_info.theme_color));
  web_app_manifest->set_dark_background_color(
      ui::OptionalSkColorToString(shortcut_info.dark_background_color));
  web_app_manifest->set_dark_theme_color(
      ui::OptionalSkColorToString(shortcut_info.dark_theme_color));

  web_app_manifest->set_id(shortcut_info.manifest_id.spec());
  webapk->set_app_key(app_key.spec());

  std::string* scope = web_app_manifest->add_scopes();
  scope->assign(shortcut_info.scope.spec());

  if (shortcut_info.share_target) {
    webapk::ShareTarget* share_target = web_app_manifest->add_share_targets();
    share_target->set_action(shortcut_info.share_target->action.spec());
    if (shortcut_info.share_target->method ==
        blink::mojom::ManifestShareTarget_Method::kPost) {
      share_target->set_method("POST");
    } else {
      share_target->set_method("GET");
    }
    if (shortcut_info.share_target->enctype ==
        blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData) {
      share_target->set_enctype("multipart/form-data");
    } else {
      share_target->set_enctype("application/x-www-form-urlencoded");
    }
    webapk::ShareTargetParams* share_target_params =
        share_target->mutable_params();
    share_target_params->set_title(
        base::UTF16ToUTF8(shortcut_info.share_target->params.title));
    share_target_params->set_text(
        base::UTF16ToUTF8(shortcut_info.share_target->params.text));

    for (const webapps::ShareTargetParamsFile& share_target_params_file :
         shortcut_info.share_target->params.files) {
      webapk::ShareTargetParamsFile* share_files =
          share_target_params->add_files();
      share_files->set_name(base::UTF16ToUTF8(share_target_params_file.name));
      for (std::u16string mime_type : share_target_params_file.accept) {
        share_files->add_accept(base::UTF16ToUTF8(mime_type));
      }
    }
  }

  if (primary_icon) {
    AddWebApkIcon(web_app_manifest, primary_icon.get());
  } else if (shortcut_info.best_primary_icon_url.is_valid()) {
    auto icon_it = icons.find(shortcut_info.best_primary_icon_url);
    if (icon_it != icons.end()) {
      // Pass the pointer instead of move the unique_ptr, the same icon may be
      // used for shortcuts.
      AddWebApkIcon(web_app_manifest, icon_it->second.get());
    }
  }

  if (splash_icon) {
    AddWebApkIcon(web_app_manifest, splash_icon.get());
  } else if (shortcut_info.splash_image_url.is_valid() &&
             shortcut_info.splash_image_url !=
                 shortcut_info.best_primary_icon_url) {
    auto icon_it = icons.find(shortcut_info.splash_image_url);
    if (icon_it != icons.end()) {
      AddWebApkIcon(web_app_manifest, icon_it->second.get());
    }
  }

  for (const std::string& icon_url : shortcut_info.icon_urls) {
    if (icon_url.empty() ||
        icon_url == shortcut_info.best_primary_icon_url.spec() ||
        icon_url == shortcut_info.splash_image_url) {
      continue;
    }

    AddOtherWebApkImage(web_app_manifest, GURL(icon_url), icons);
  }

  for (const auto& manifest_shortcut_item : shortcut_info.shortcut_items) {
    auto* shortcut_item = web_app_manifest->add_shortcuts();
    shortcut_item->set_name(base::UTF16ToUTF8(manifest_shortcut_item.name));
    shortcut_item->set_short_name(base::UTF16ToUTF8(
        manifest_shortcut_item.short_name.value_or(std::u16string())));
    shortcut_item->set_url(manifest_shortcut_item.url.spec());

    for (const auto& manifest_icon : manifest_shortcut_item.icons) {
      AddShortcutIcon(shortcut_item->add_icons(), manifest_icon.src, icons);
    }
  }

  std::unique_ptr<std::string> serialized_proto =
      std::make_unique<std::string>();
  webapk->SerializeToString(serialized_proto.get());
  return serialized_proto;
}

// Returns task runner for running background tasks.
scoped_refptr<base::TaskRunner> GetBackgroundTaskRunner() {
  return base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

void BuildProto(
    const webapps::ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    base::OnceCallback<void(std::unique_ptr<std::string>)> callback) {
  GetBackgroundTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&webapps::BuildProtoInBackground, shortcut_info, app_key,
                     std::move(primary_icon), std::move(splash_icon),
                     package_name, version, std::move(icons), is_manifest_stale,
                     is_app_identity_update_supported,
                     std::vector<webapps::WebApkUpdateReason>()),
      std::move(callback));
}

// Builds the WebAPK proto for an update request and stores it to
// |update_request_path|. Returns whether the proto was successfully written to
// disk.
bool StoreUpdateRequestToFileInBackground(
    const base::FilePath& update_request_path,
    const webapps::ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<WebApkUpdateReason> update_reasons) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<std::string> proto = BuildProtoInBackground(
      shortcut_info, app_key, std::move(primary_icon), std::move(splash_icon),
      package_name, version, std::move(icons), is_manifest_stale,
      is_app_identity_update_supported, std::move(update_reasons));

  // Create directory if it does not exist.
  base::CreateDirectory(update_request_path.DirName());

  return base::WriteFile(update_request_path, *proto);
}

}  // namespace webapps
