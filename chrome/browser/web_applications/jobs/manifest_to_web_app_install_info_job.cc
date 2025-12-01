// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-data-view.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// We restrict the number of icons to limit disk usage per installed PWA. This
// value can change overtime as new features are added.
constexpr int kMaxIcons = 20;
constexpr SquareSizePx kMaxIconSize =
    webapps::InstallableEvaluator::kMaximumIconSizeInPx;

// Construct a list of icons from the parsed icons field of the manifest
// *outside* of |web_app_info|, and update the current web_app_info if found.
// If any icons are correctly specified in the manifest, they take precedence
// over any we picked up from web page metadata.
void UpdateWebAppInstallInfoIconsFromManifestIfNeeded(
    const std::vector<blink::Manifest::ImageResource>& icons,
    WebAppInstallInfo* web_app_info) {
  std::vector<apps::IconInfo> web_app_icons;
  for (const auto& icon : icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    CHECK(!icon.purpose.empty());

    for (IconPurpose purpose : icon.purpose) {
      apps::IconInfo info;

      if (!icon.sizes.empty()) {
        if (base::Contains(icon.sizes, gfx::Size()) &&
            icon.src.spec().find(".svg") != std::string::npos) {
          web_app_info->icons_with_size_any.manifest_icons[purpose] = icon.src;
        }

        // Filter out non-square or too large icons.
        auto valid_size =
            std::ranges::find_if(icon.sizes, [](const gfx::Size& size) {
              return size.width() == size.height() &&
                     size.width() <= kMaxIconSize;
            });
        if (valid_size == icon.sizes.end()) {
          continue;
        }

        // TODO(crbug.com/40126722): Take the declared icon density and
        // sizes into account.
        info.square_size_px = valid_size->width();
      }

      info.url = icon.src;
      info.purpose = ManifestPurposeToIconInfoPurpose(purpose);
      web_app_icons.push_back(std::move(info));

      // Limit the number of icons we store on the user's machine.
      if (web_app_icons.size() == kMaxIcons) {
        break;
      }
    }

    // Keep track of the sizes passed in via the manifest which will be
    // later used to compute how many SVG icons of size:any we need to
    // download.
    // This is handled outside the loop above to reduce the number of iterations
    // so that purpose and size metadata is parsed sequentially one after the
    // other.
    if (!web_app_info->icons_with_size_any.manifest_icons.empty()) {
      for (const auto& icon_size : icon.sizes) {
        if (icon_size == gfx::Size()) {
          continue;
        }
        web_app_info->icons_with_size_any.manifest_icon_provided_sizes.emplace(
            icon_size);
      }
    }

    if (web_app_icons.size() == kMaxIcons) {
      break;
    }
  }

  // If any icons have been found from the manifest, set them inside the
  // |web_app_info|.
  if (!web_app_icons.empty()) {
    web_app_info->manifest_icons = std::move(web_app_icons);
  }
}

// Populate |web_app_info|'s shortcuts_menu_item_infos vector using the
// blink::Manifest's shortcuts vector.
void PopulateWebAppShortcutsMenuItemInfos(
    const std::vector<blink::Manifest::ShortcutItem>& shortcuts,
    WebAppInstallInfo* web_app_info) {
  std::vector<WebAppShortcutsMenuItemInfo> web_app_shortcut_infos;
  web_app_shortcut_infos.reserve(shortcuts.size());
  int num_shortcut_icons = 0;
  for (const auto& shortcut : shortcuts) {
    if (web_app_shortcut_infos.size() >= kMaxApplicationDockMenuItems) {
      break;
    }

    WebAppShortcutsMenuItemInfo shortcut_info;
    shortcut_info.name = shortcut.name;
    shortcut_info.url = shortcut.url;

    for (IconPurpose purpose : kIconPurposes) {
      std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_icons;
      for (const auto& icon : shortcut.icons) {
        CHECK(!icon.purpose.empty());
        if (!base::Contains(icon.purpose, purpose)) {
          continue;
        }

        WebAppShortcutsMenuItemInfo::Icon info;

        if (base::Contains(icon.sizes, gfx::Size()) &&
            icon.src.spec().find(".svg") != std::string::npos) {
          web_app_info->icons_with_size_any.shortcut_menu_icons[purpose] =
              icon.src;
        }

        // Filter out non-square or too large icons.
        auto valid_size_it =
            std::ranges::find_if(icon.sizes, [](const gfx::Size& size) {
              return size.width() == size.height() &&
                     size.width() <= kMaxIconSize;
            });
        if (valid_size_it == icon.sizes.end()) {
          continue;
        }
        // TODO(crbug.com/40126722): Take the declared icon density and
        // sizes into account.
        info.square_size_px = valid_size_it->width();

        // Keep track of the sizes passed in via the manifest which will be
        // later used to compute how many SVG icons of size:any we need to
        // download.
        if (!web_app_info->icons_with_size_any.shortcut_menu_icons.empty()) {
          for (const auto& icon_size : icon.sizes) {
            if (icon_size == gfx::Size()) {
              continue;
            }
            web_app_info->icons_with_size_any.shortcut_menu_icons_provided_sizes
                .emplace(icon_size);
          }
        }

        CHECK_LE(num_shortcut_icons, kMaxIcons);
        if (num_shortcut_icons < kMaxIcons) {
          info.url = icon.src;
          shortcut_icons.push_back(std::move(info));
          ++num_shortcut_icons;
        }
        if (num_shortcut_icons == kMaxIcons) {
          break;
        }
      }

      // If any icons are specified in the manifest, they take precedence over
      // any we picked up from web_app_info.
      if (!shortcut_icons.empty()) {
        shortcut_info.SetShortcutIconInfosForPurpose(purpose,
                                                     std::move(shortcut_icons));
      }
    }
    web_app_shortcut_infos.push_back(std::move(shortcut_info));
  }

  web_app_info->shortcuts_menu_item_infos = std::move(web_app_shortcut_infos);
}

apps::ShareTarget::Method ToAppsShareTargetMethod(
    blink::mojom::ManifestShareTarget_Method method) {
  switch (method) {
    case blink::mojom::ManifestShareTarget_Method::kGet:
      return apps::ShareTarget::Method::kGet;
    case blink::mojom::ManifestShareTarget_Method::kPost:
      return apps::ShareTarget::Method::kPost;
  }
  NOTREACHED();
}

apps::ShareTarget::Enctype ToAppsShareTargetEnctype(
    blink::mojom::ManifestShareTarget_Enctype enctype) {
  switch (enctype) {
    case blink::mojom::ManifestShareTarget_Enctype::kFormUrlEncoded:
      return apps::ShareTarget::Enctype::kFormUrlEncoded;
    case blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData:
      return apps::ShareTarget::Enctype::kMultipartFormData;
  }
  NOTREACHED();
}

std::optional<apps::ShareTarget> ToWebAppShareTarget(
    const std::optional<blink::Manifest::ShareTarget>& share_target) {
  if (!share_target) {
    return std::nullopt;
  }
  apps::ShareTarget apps_share_target;
  apps_share_target.action = share_target->action;
  apps_share_target.method = ToAppsShareTargetMethod(share_target->method);
  apps_share_target.enctype = ToAppsShareTargetEnctype(share_target->enctype);

  if (share_target->params.title.has_value()) {
    apps_share_target.params.title =
        base::UTF16ToUTF8(*share_target->params.title);
  }
  if (share_target->params.text.has_value()) {
    apps_share_target.params.text =
        base::UTF16ToUTF8(*share_target->params.text);
  }
  if (share_target->params.url.has_value()) {
    apps_share_target.params.url = base::UTF16ToUTF8(*share_target->params.url);
  }

  for (const auto& file_filter : share_target->params.files) {
    apps::ShareTarget::Files apps_share_target_files;
    apps_share_target_files.name = base::UTF16ToUTF8(file_filter.name);

    for (const auto& file_type : file_filter.accept) {
      apps_share_target_files.accept.push_back(base::UTF16ToUTF8(file_type));
    }

    apps_share_target.params.files.push_back(
        std::move(apps_share_target_files));
  }

  return std::move(apps_share_target);
}

std::vector<apps::ProtocolHandlerInfo> ToWebAppProtocolHandlers(
    const std::vector<blink::mojom::ManifestProtocolHandlerPtr>&
        manifest_protocol_handlers) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;
  for (const auto& manifest_protocol_handler : manifest_protocol_handlers) {
    apps::ProtocolHandlerInfo protocol_handler;
    protocol_handler.protocol =
        base::UTF16ToUTF8(manifest_protocol_handler->protocol);
    protocol_handler.url = manifest_protocol_handler->url;
    protocol_handlers.push_back(std::move(protocol_handler));
  }
  return protocol_handlers;
}

ScopeExtensions ToWebAppScopeExtensions(
    const std::vector<blink::mojom::ManifestScopeExtensionPtr>&
        scope_extensions) {
  ScopeExtensions apps_scope_extensions;
  for (const auto& scope_extension : scope_extensions) {
    CHECK(scope_extension);
    auto new_scope_extension = ScopeExtensionInfo::CreateForOrigin(
        scope_extension->origin, scope_extension->has_origin_wildcard);
    apps_scope_extensions.insert(std::move(new_scope_extension));
  }
  return apps_scope_extensions;
}

base::flat_map<std::string, blink::Manifest::TranslationItem>
ToWebAppTranslations(
    const base::flat_map<std::u16string, blink::Manifest::TranslationItem>&
        manifest_translations) {
  std::vector<std::pair<std::string, blink::Manifest::TranslationItem>>
      translations_vector;
  translations_vector.reserve(manifest_translations.size());
  for (const auto& it : manifest_translations) {
    translations_vector.emplace_back(base::UTF16ToUTF8(it.first), it.second);
  }
  return base::flat_map<std::string, blink::Manifest::TranslationItem>(
      std::move(translations_vector));
}

// Create the WebAppInstallInfo icons list *outside* of |web_app_info|, so
// that we can decide later whether or not to replace the existing
// home tab icons.
// Icons are replaced if we filter out icons that are too large or non-square
// which limits the number of icons.
void PopulateHomeTabIconsFromHomeTabManifestParams(
    WebAppInstallInfo* web_app_info) {
  auto& home_tab = std::get<blink::Manifest::HomeTabParams>(
      web_app_info->tab_strip->home_tab);
  std::vector<blink::Manifest::ImageResource> home_tab_icons;
  for (const auto& icon : home_tab.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    CHECK(!icon.purpose.empty());

    if (!icon.sizes.empty()) {
      if (base::Contains(icon.sizes, gfx::Size()) &&
          icon.src.spec().find(".svg") != std::string::npos) {
        for (const auto& purpose : icon.purpose) {
          web_app_info->icons_with_size_any.home_tab_icons[purpose] = icon.src;
        }
      }
      // Filter out non-square or too large icons.
      auto valid_size =
          std::ranges::find_if(icon.sizes, [](const gfx::Size& size) {
            return size.width() == size.height() &&
                   size.width() <= kMaxIconSize;
          });
      if (valid_size == icon.sizes.end()) {
        continue;
      }

      // Keep track of the sizes passed in via the manifest which will be
      // later used to compute how many SVG icons of size:any we need to
      // download.
      if (!web_app_info->icons_with_size_any.home_tab_icons.empty()) {
        for (const auto& icon_size : icon.sizes) {
          if (icon_size == gfx::Size()) {
            continue;
          }
          web_app_info->icons_with_size_any.home_tab_icon_provided_sizes
              .emplace(icon_size);
        }
      }
    }

    home_tab_icons.push_back(std::move(icon));

    // Limit the number of icons we store on the user's machine.
    if (home_tab_icons.size() == kMaxIcons) {
      break;
    }
  }

  home_tab.icons = std::move(home_tab_icons);
  web_app_info->tab_strip->home_tab = home_tab;
}

// Merges a `WebAppInstallInfo` instance obtained from parsing the web page
// metadata into one that's obtained from the manifest.
// It is the duty of the callsites to perform the necessary checks to ensure
// that `from_info` and `to_info` is valid.
void MergeFallbackInstallInfoIntoNewInfo(const WebAppInstallInfo& from_info,
                                         WebAppInstallInfo& to_info,
                                         bool force_override_name) {
  // Merge fields from `from_info` onto `to_info` if required.
  // `from` is generated from the `WebAppDataRetriever` and populates
  // the following fields:
  // - title
  // - description
  // - start_url
  // - manifest_id
  // - manifest_icons
  // - mobile_capable
  // Out of these, only `title`, `description`, `manifest_icons` and
  // `mobile_capable` needs to be moved over to `to_info`. `start_url` and
  // `manifest_id` has to be valid for the job to run.
  if ((force_override_name && !from_info.title.empty()) ||
      to_info.title.empty()) {
    to_info.title = from_info.title;
  }
  if (to_info.description.empty()) {
    to_info.description = from_info.description;
  }
  to_info.mobile_capable = from_info.mobile_capable;
  if (to_info.manifest_icons.empty() && !from_info.manifest_icons.empty()) {
    to_info.manifest_icons = from_info.manifest_icons;
  }
}

void RecordIconUpdateMetrics(
    IconsDownloadedResult result,
    const IconsMap& icons_map,
    const DownloadedIconsHttpResults& icons_http_results) {
  // TODO(crbug.com/40193545): Report `result` and `icons_http_results` in
  // internals.
  base::UmaHistogramEnumeration("WebApp.Icon.DownloadedResultOnUpdate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnUpdate", icons_http_results);
  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnUpdate", result, icons_http_results);

  size_t total_icon_sizes_downloaded = 0;
  for (const auto& [_, icons] : icons_map) {
    for (const SkBitmap& bitmap : icons) {
      total_icon_sizes_downloaded += bitmap.computeByteSize();
    }
  }

  // The total size is stored in bytes, so convert to MB.
  base::UmaHistogramMemoryMB("WebApp.TotalIconsMemory.DownloadedForUpdate",
                             (total_icon_sizes_downloaded / 1024 / 1024));
}

}  // namespace

ManifestToWebAppInstallInfoJob::~ManifestToWebAppInstallInfoJob() = default;

// static
std::unique_ptr<ManifestToWebAppInstallInfoJob>
ManifestToWebAppInstallInfoJob::CreateAndStart(
    const blink::mojom::Manifest& manifest,
    WebAppDataRetriever& data_retriever,
    bool background_installation,
    webapps::WebappInstallSource install_source,
    base::WeakPtr<content::WebContents> web_contents,
    base::FunctionRef<void(IconUrlSizeSet&)> icon_url_modifications,
    base::Value::Dict& debug_data,
    WebAppInstallInfoCreationCallback creation_callback,
    WebAppInstallInfoConstructOptions options,
    std::optional<WebAppInstallInfo> fallback_info) {
  auto job = base::WrapUnique(new ManifestToWebAppInstallInfoJob(
      manifest, data_retriever, background_installation, install_source,
      debug_data, std::move(creation_callback), options,
      std::move(fallback_info)));
  job->Start(web_contents, icon_url_modifications);
  return job;
}

base::Value
ManifestToWebAppInstallInfoJob::GetManifestToWebAppInfoGenerationErrors() {
  if (!install_error_log_entry_.HasErrorDict()) {
    return base::Value();
  }
  return install_error_log_entry_.TakeErrorDict();
}

void ManifestToWebAppInstallInfoJob::FetchIcons(
    WebAppInstallInfo& install_info,
    content::WebContents& web_contents,
    base::OnceClosure callback,
    std::optional<base::FunctionRef<void(IconUrlSizeSet&)>>
        icon_url_modifications,
    IconUrlExtractionOptions icon_url_options) {
  CHECK(creation_callback_.is_null());
  CHECK(options_.defer_icon_fetching);
  install_info_for_icon_fetch_ = &install_info;
  // The final callback will be called with a blank unique ptr, that is fine and
  // we ignore it.
  creation_callback_ =
      base::IgnoreArgs<std::unique_ptr<WebAppInstallInfo>>(std::move(callback));
  FetchIconsInternal(web_contents, icon_url_modifications, icon_url_options);
}

IconsDownloadedResult ManifestToWebAppInstallInfoJob::icon_download_result()
    const {
  return icon_fetch_result_;
}

ManifestToWebAppInstallInfoJob::ManifestToWebAppInstallInfoJob(
    const blink::mojom::Manifest& manifest,
    WebAppDataRetriever& data_retriever,
    bool background_installation,
    webapps::WebappInstallSource install_source,
    base::Value::Dict& debug_data,
    WebAppInstallInfoCreationCallback creation_callback,
    WebAppInstallInfoConstructOptions options,
    std::optional<WebAppInstallInfo> fallback_info)
    : manifest_(manifest.Clone()),
      data_retriever_(data_retriever),
      creation_callback_(std::move(creation_callback)),
      options_(options),
      fallback_info_(std::move(fallback_info)),
      install_error_log_entry_(background_installation, install_source),
      debug_data_(debug_data) {
  // These are the pre-requisites for constructing a WebAppInstallInfo from a
  // valid manifest id and start url.
  CHECK(manifest_->id.is_valid());
  CHECK(!manifest_->id.has_ref());
  CHECK(manifest_->start_url.is_valid());

  debug_data_->Set("manifest_id", manifest_->id.spec());
  debug_data_->Set("start_url", manifest_->start_url.spec());
  if (manifest_->name && !manifest_->name->empty()) {
    debug_data_->Set("manifest_name", *manifest_->name);
  }
  if (manifest_->short_name && !manifest_->short_name->empty()) {
    debug_data_->Set("manifest_short_name", *manifest_->short_name);
  }
}

WebAppInstallInfo& ManifestToWebAppInstallInfoJob::install_info() {
  return install_info_ ? *install_info_ : *install_info_for_icon_fetch_;
}

void ManifestToWebAppInstallInfoJob::Start(
    base::WeakPtr<content::WebContents> web_contents,
    base::FunctionRef<void(IconUrlSizeSet&)> icon_url_modifications) {
  // Exit early if the web contents is being destroyed.
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ManifestToWebAppInstallInfoJob::CompleteJobAndRunCallback,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  install_info_ =
      std::make_unique<WebAppInstallInfo>(manifest_->id, manifest_->start_url);

  // First, populate the `install_info()` by parsing the fields provided in the
  // manifest.
  ParseManifestAndPopulateInfo();
  if (fallback_info_) {
    MergeFallbackInstallInfoIntoNewInfo(fallback_info_.value(), install_info(),
                                        options_.force_override_name);
  }

  // Second, fetch icons from the URLs in the install_info().
  if (options_.defer_icon_fetching) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ManifestToWebAppInstallInfoJob::CompleteJobAndRunCallback,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  FetchIconsInternal(*web_contents, icon_url_modifications,
                     IconUrlExtractionOptions());
}

void ManifestToWebAppInstallInfoJob::FetchIconsInternal(
    content::WebContents& web_contents,
    std::optional<base::FunctionRef<void(IconUrlSizeSet&)>>
        icon_url_modifications,
    IconUrlExtractionOptions icon_url_options) {
  IconUrlSizeSet icon_urls_to_download =
      GetValidIconUrlsToDownload(install_info(), icon_url_options);
  if (icon_url_modifications.has_value()) {
    (*icon_url_modifications)(icon_urls_to_download);
  }
  for (const IconUrlWithSize& icon_with_size : icon_urls_to_download) {
    debug_data_->EnsureList("icon_urls_from_manifest")
        ->Append(icon_with_size.ToString());
  }

  // This needs to be async to prevent re-entry issues on the caller and to
  // ensure that the outcome of this task is always async, as GetIcons() is
  // async.
  if (icon_urls_to_download.empty() &&
      options_.bypass_icon_generation_if_no_url) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ManifestToWebAppInstallInfoJob::CompleteJobAndRunCallback,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  data_retriever_->GetIcons(
      &web_contents, icon_urls_to_download, options_.download_page_favicons,
      options_.fail_all_if_any_fail,
      base::BindOnce(
          &ManifestToWebAppInstallInfoJob::OnIconsFetchedGetInstallInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

void ManifestToWebAppInstallInfoJob::ParseManifestAndPopulateInfo() {
  // Give the full length name priority if it's not empty.
  std::u16string name = manifest_->name.value_or(std::u16string());
  if (!name.empty()) {
    install_info().title = name;
  } else if (manifest_->short_name) {
    install_info().title = *manifest_->short_name;
  }

  // Clean up.
  if (manifest_->scope.is_valid()) {
    install_info().scope = manifest_->scope;
  }
  // Ensure scope is derived if empty after processing manifest.
  if (install_info().scope.is_empty()) {
    install_info().scope = install_info().start_url().GetWithoutFilename();
  }
  CHECK(!install_info().scope.is_empty());

  if (manifest_->has_theme_color) {
    install_info().theme_color = SkColorSetA(
        static_cast<SkColor>(manifest_->theme_color), SK_AlphaOPAQUE);
  }

  if (manifest_->has_background_color) {
    install_info().background_color = SkColorSetA(
        static_cast<SkColor>(manifest_->background_color), SK_AlphaOPAQUE);
  }

  if (manifest_->display != DisplayMode::kUndefined) {
    install_info().display_mode = manifest_->display;
  }

  if (!manifest_->display_override.empty()) {
    install_info().display_override = manifest_->display_override;
  }

  UpdateWebAppInstallInfoIconsFromManifestIfNeeded(manifest_->icons,
                                                   &install_info());
  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon)) {
    if (options_.use_manifest_icons_as_trusted) {
      install_info().trusted_icons = install_info().manifest_icons;
    } else {
      std::optional<apps::IconInfo> primary_icon_metadata =
          GetTrustedIconsFromManifest(manifest_->icons);
      if (primary_icon_metadata) {
        install_info().trusted_icons = {*primary_icon_metadata};
      }
    }
  }

  // TODO(crbug.com/40185556): Confirm incoming icons to write to install_info_.
  PopulateFileHandlerInfoFromManifest(manifest_->file_handlers,
                                      install_info().scope, &install_info());

  install_info().borderless_url_patterns = manifest_->borderless_url_patterns;

  install_info().share_target = ToWebAppShareTarget(manifest_->share_target);

  install_info().protocol_handlers =
      ToWebAppProtocolHandlers(manifest_->protocol_handlers);

  install_info().scope_extensions =
      ToWebAppScopeExtensions(manifest_->scope_extensions);

  GURL inferred_scope = install_info().scope.is_valid()
                            ? install_info().scope
                            : install_info().start_url().GetWithoutFilename();
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppManifestLockScreen) &&
      manifest_->lock_screen && manifest_->lock_screen->start_url.is_valid() &&
      IsInScope(manifest_->lock_screen->start_url, inferred_scope)) {
    install_info().lock_screen_start_url = manifest_->lock_screen->start_url;
  }

  if (manifest_->note_taking &&
      manifest_->note_taking->new_note_url.is_valid() &&
      IsInScope(manifest_->note_taking->new_note_url, inferred_scope)) {
    install_info().note_taking_new_note_url =
        manifest_->note_taking->new_note_url;
  }

  CHECK(install_info().shortcuts_menu_item_infos.empty());
  PopulateWebAppShortcutsMenuItemInfos(manifest_->shortcuts, &install_info());

  if (manifest_->manifest_url.is_valid()) {
    install_info().manifest_url = manifest_->manifest_url;
  }

  install_info().launch_handler = manifest_->launch_handler;
  if (manifest_->description.has_value()) {
    install_info().description = manifest_->description.value();
  }

  install_info().translations = ToWebAppTranslations(manifest_->translations);

  install_info().permissions_policy.clear();
  for (const auto& decl : manifest_->permissions_policy) {
    network::ParsedPermissionsPolicyDeclaration copy;
    copy.feature = decl.feature;
    copy.self_if_matches = decl.self_if_matches;
    for (const auto& origin : decl.allowed_origins) {
      copy.allowed_origins.push_back(origin);
    }
    copy.matches_all_origins = decl.matches_all_origins;
    copy.matches_opaque_src = decl.matches_opaque_src;
    install_info().permissions_policy.push_back(std::move(copy));
  }

  install_info().tab_strip = manifest_->tab_strip;

  if (HomeTabIconsExistInTabStrip(*install_info_)) {
    PopulateHomeTabIconsFromHomeTabManifestParams(&install_info());
  }

  install_info().related_applications = manifest_->related_applications;
}

void ManifestToWebAppInstallInfoJob::OnIconsFetchedGetInstallInfo(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  icon_fetch_result_ = result;
  base::Value::Dict* icons_downloaded =
      debug_data_->EnsureDict("icons_retrieved");
  for (const auto& [url, bitmap_vector] : icons_map) {
    base::Value::List* sizes = icons_downloaded->EnsureList(url.spec());
    for (const SkBitmap& bitmap : bitmap_vector) {
      sizes->Append(bitmap.width());
    }
  }
  debug_data_->Set("icon_download_result", base::ToString(result));

  // TODO(crbug.com/429929887): Return results via callback using a result
  // struct/class.
  if (options_.record_icon_results_on_update) {
    RecordIconUpdateMetrics(result, icons_map, icons_http_results);
  }

  // Bypass populating product icons, even generated ones, if icons have not
  // been downloaded.
  PopulateProductIcons(&install_info(), &icons_map);
  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon)) {
    if (options_.use_manifest_icons_as_trusted) {
      install_info().trusted_icon_bitmaps = install_info().icon_bitmaps;
    } else {
      PopulateTrustedIconBitmaps(install_info(), icons_map);
    }
  }
  PopulateOtherIcons(&install_info(), icons_map);
  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  install_error_log_entry_.LogDownloadedIconsErrors(
      install_info(), result, icons_map, icons_http_results);
  CompleteJobAndRunCallback();
}

void ManifestToWebAppInstallInfoJob::CompleteJobAndRunCallback() {
  install_info_for_icon_fetch_ = nullptr;
  std::move(creation_callback_).Run(std::move(install_info_));
}

}  // namespace web_app
