// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_utils.h"

#include <ostream>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/skia_util.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os, ManifestUpdateResult result) {
  switch (result) {
    case ManifestUpdateResult::kNoAppInScope:
      return os << "kNoAppInScope";
    case ManifestUpdateResult::kThrottled:
      return os << "kThrottled";
    case ManifestUpdateResult::kWebContentsDestroyed:
      return os << "kWebContentsDestroyed";
    case ManifestUpdateResult::kAppUninstalling:
      return os << "kAppUninstalling";
    case ManifestUpdateResult::kAppIsPlaceholder:
      return os << "kAppIsPlaceholder";
    case ManifestUpdateResult::kAppUpToDate:
      return os << "kAppUpToDate";
    case ManifestUpdateResult::kAppNotEligible:
      return os << "kAppNotEligible";
    case ManifestUpdateResult::kAppUpdateFailed:
      return os << "kAppUpdateFailed";
    case ManifestUpdateResult::kAppUpdated:
      return os << "kAppUpdated";
    case ManifestUpdateResult::kAppIsSystemWebApp:
      return os << "kAppIsSystemWebApp";
    case ManifestUpdateResult::kIconDownloadFailed:
      return os << "kIconDownloadFailed";
    case ManifestUpdateResult::kIconReadFromDiskFailed:
      return os << "kIconReadFromDiskFailed";
    case ManifestUpdateResult::kAppIdMismatch:
      return os << "kAppIdMismatch";
    case ManifestUpdateResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ManifestUpdateResult::kAppIdentityUpdateRejectedAndUninstalled:
      return os << "kAppIdentityUpdateRejectedAndUninstalled";
    case ManifestUpdateResult::kAppIsIsolatedWebApp:
      return os << "kAppIsIsolatedWebApp";
    case ManifestUpdateResult::kCancelledDueToMainFrameNavigation:
      return os << "kCancelledDueToMainFrameNavigation";
    case ManifestUpdateResult::kShortcutIgnoresManifest:
      return os << "kkShortcutIgnoresManifest";
  }
}

std::ostream& operator<<(std::ostream& os, ManifestUpdateCheckStage stage) {
  switch (stage) {
    case ManifestUpdateCheckStage::kPendingAppLock:
      return os << "kPendingAppLock";
    case ManifestUpdateCheckStage::kDownloadingNewManifestData:
      return os << "kDownloadingNewManifestData";
    case ManifestUpdateCheckStage::kLoadingExistingManifestData:
      return os << "kLoadingExistingManifestData";
    case ManifestUpdateCheckStage::kComparingManifestData:
      return os << "kComparingManifestData";
    case ManifestUpdateCheckStage::kResolvingIdentityChanges:
      return os << "kResolvingIdentityChanges";
    case ManifestUpdateCheckStage::kComplete:
      return os << "kComplete";
  }
}

std::ostream& operator<<(std::ostream& os, ManifestUpdateCheckResult stage) {
  switch (stage) {
    case ManifestUpdateCheckResult::kAppIdMismatch:
      return os << "kAppIdMismatch";
    case ManifestUpdateCheckResult::kAppNotEligible:
      return os << "kAppNotEligible";
    case ManifestUpdateCheckResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ManifestUpdateCheckResult::kAppUpdateNeeded:
      return os << "kAppUpdateNeeded";
    case ManifestUpdateCheckResult::kAppIdentityUpdateRejectedAndUninstalled:
      return os << "kAppIdentityUpdateRejectedAndUninstalled";
    case ManifestUpdateCheckResult::kAppUpToDate:
      return os << "kAppUpToDate";
    case ManifestUpdateCheckResult::kIconDownloadFailed:
      return os << "kIconDownloadFailed";
    case ManifestUpdateCheckResult::kIconReadFromDiskFailed:
      return os << "kIconReadFromDiskFailed";
    case ManifestUpdateCheckResult::kWebContentsDestroyed:
      return os << "kWebContentsDestroyed";
    case ManifestUpdateCheckResult::kCancelledDueToMainFrameNavigation:
      return os << "kCancelledDueToMainFrameNavigation";
  }
}

ManifestUpdateResult FinalResultFromManifestUpdateCheckResult(
    ManifestUpdateCheckResult check_result) {
  switch (check_result) {
    case ManifestUpdateCheckResult::kAppIdMismatch:
      return ManifestUpdateResult::kAppIdMismatch;
    case ManifestUpdateCheckResult::kAppNotEligible:
      return ManifestUpdateResult::kAppNotEligible;
    case ManifestUpdateCheckResult::kSystemShutdown:
      return ManifestUpdateResult::kSystemShutdown;
    case ManifestUpdateCheckResult::kAppUpdateNeeded:
      // The manifest needs to be applied before the overall update process is
      // considered complete.
      NOTREACHED_IN_MIGRATION();
      return ManifestUpdateResult::kAppUpdated;
    case ManifestUpdateCheckResult::kAppUpToDate:
      return ManifestUpdateResult::kAppUpToDate;
    case ManifestUpdateCheckResult::kAppIdentityUpdateRejectedAndUninstalled:
      return ManifestUpdateResult::kAppIdentityUpdateRejectedAndUninstalled;
    case ManifestUpdateCheckResult::kIconDownloadFailed:
      return ManifestUpdateResult::kIconDownloadFailed;
    case ManifestUpdateCheckResult::kIconReadFromDiskFailed:
      return ManifestUpdateResult::kIconReadFromDiskFailed;
    case ManifestUpdateCheckResult::kWebContentsDestroyed:
      return ManifestUpdateResult::kWebContentsDestroyed;
    case ManifestUpdateCheckResult::kCancelledDueToMainFrameNavigation:
      return ManifestUpdateResult::kCancelledDueToMainFrameNavigation;
  }
}

std::optional<AppIconIdentityChange> CompareIdentityIconBitmaps(
    const IconBitmaps& existing_app_icon_bitmaps,
    const IconBitmaps& new_app_icon_bitmaps) {
  for (IconPurpose purpose : kIconPurposes) {
    const std::map<SquareSizePx, SkBitmap>& existing_bitmaps =
        existing_app_icon_bitmaps.GetBitmapsForPurpose(purpose);
    const std::map<SquareSizePx, SkBitmap>& new_bitmaps =
        new_app_icon_bitmaps.GetBitmapsForPurpose(purpose);

    for (SquareSizePx size : kIdentitySizes) {
      auto existing_it = existing_bitmaps.find(size);
      if (existing_it == existing_bitmaps.end()) {
        continue;
      }
      auto new_it = new_bitmaps.find(size);
      if (new_it == new_bitmaps.end()) {
        continue;
      }

      const SkBitmap& existing_bitmap = existing_it->second;
      const SkBitmap& new_bitmap = new_it->second;

      if (!gfx::BitmapsAreEqual(existing_bitmap, new_bitmap)) {
        return AppIconIdentityChange{.before = existing_bitmap,
                                     .after = new_bitmap};
      }
    }
  }
  return std::nullopt;
}

void RecordIconDownloadMetrics(IconsDownloadedResult result,
                               DownloadedIconsHttpResults icons_http_results) {
  // TODO(crbug.com/40193545): Report `result` and `icons_http_results` in
  // internals.
  base::UmaHistogramEnumeration("WebApp.Icon.DownloadedResultOnUpdate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnUpdate", icons_http_results);
  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnUpdate", result, icons_http_results);
}

bool CanWebAppSilentlyUpdateIdentity(const WebApp& web_app) {
  if (web_app.IsPolicyInstalledApp() &&
      base::FeatureList::IsEnabled(
          features::kWebAppManifestPolicyAppIdentityUpdate)) {
    return true;
  }

  // WebAppChromeOsData::oem_installed is not included in this statement as
  // we would like to keep WebAppManagement::kOem and
  // WebAppChromeOsData::oem_installed separate.
  // WebAppChromeOsData::oem_installed will be migrated to
  // WebAppManagement::kOem eventually.
  return web_app.IsPreinstalledApp() || web_app.IsKioskInstalledApp() ||
         web_app.GetSources().HasAny(
             {WebAppManagement::kOem, WebAppManagement::kApsDefault});
}

bool CanShowIdentityUpdateConfirmationDialog(const WebAppRegistrar& registrar,
                                             const WebApp& web_app) {
  DCHECK(!CanWebAppSilentlyUpdateIdentity(web_app));

  if (web_app.IsPolicyInstalledApp()) {
    return false;
  }

  // Shortcut apps may immediately trigger the identity updating if the user
  // has overridden the title of the app, see: https://crbug.com/1366600
  // Don't show the update prompt for shortcut apps and always revert.
  // Also, ideally we should just use IsShortcutApp here instead of checking the
  // install source, but as per https://crbug.com/1368592 there is a bug with
  // that where it returns the wrong thing for Shortcut apps that specify
  // `scope`.
  bool is_shortcut_app =
      registrar.IsShortcutApp(web_app.app_id()) ||
      registrar.GetLatestAppInstallSource(web_app.app_id()) ==
          webapps::WebappInstallSource::MENU_CREATE_SHORTCUT;
  if (is_shortcut_app) {
    return false;
  }

  return true;
}

ManifestDataChanges::ManifestDataChanges() = default;
ManifestDataChanges::ManifestDataChanges(ManifestDataChanges&&) = default;
ManifestDataChanges& ManifestDataChanges::operator=(ManifestDataChanges&&) =
    default;
ManifestDataChanges::~ManifestDataChanges() = default;

ManifestDataChanges GetManifestDataChanges(
    const WebApp& existing_web_app,
    const IconBitmaps* existing_app_icon_bitmaps,
    const ShortcutsMenuIconBitmaps* existing_shortcuts_menu_icon_bitmaps,
    const WebAppInstallInfo& new_install_info) {
  ManifestDataChanges result;

  // TODO(crbug.com/40201597): Check whether translations have been updated.
  result.app_name_changed =
      new_install_info.title !=
      base::UTF8ToUTF16(existing_web_app.untranslated_name());

  // TODO(crbug.com/40254036): Run these bitmap comparisons off the UI thread.
  if (existing_app_icon_bitmaps) {
    result.app_icon_identity_change = CompareIdentityIconBitmaps(
        *existing_app_icon_bitmaps, new_install_info.icon_bitmaps);
  }

  result.any_app_icon_changed =
      result.app_icon_identity_change ||
      existing_web_app.manifest_icons() != new_install_info.manifest_icons ||
      (existing_app_icon_bitmaps &&
       *existing_app_icon_bitmaps != new_install_info.icon_bitmaps);

  result.other_fields_changed = [&] {
    if (existing_web_app.manifest_id() != new_install_info.manifest_id()) {
      return true;
    }
    if (existing_web_app.start_url() != new_install_info.start_url()) {
      return true;
    }
    if (existing_web_app.theme_color() != new_install_info.theme_color) {
      return true;
    }
    if (existing_web_app.scope() != new_install_info.scope) {
      return true;
    }
    if (existing_web_app.display_mode() != new_install_info.display_mode) {
      return true;
    }
    if (existing_web_app.display_mode_override() !=
        new_install_info.display_override) {
      return true;
    }
    if (existing_web_app.shortcuts_menu_item_infos() !=
        new_install_info.shortcuts_menu_item_infos) {
      return true;
    }
    if (existing_web_app.share_target() != new_install_info.share_target) {
      return true;
    }
    if (existing_web_app.protocol_handlers() !=
        new_install_info.protocol_handlers) {
      return true;
    }
    if (existing_web_app.url_handlers() != new_install_info.url_handlers) {
      return true;
    }
    if (base::FeatureList::IsEnabled(
            blink::features::kWebAppManifestLockScreen) &&
        existing_web_app.lock_screen_start_url() !=
            new_install_info.lock_screen_start_url) {
      return true;
    }
    if (existing_web_app.note_taking_new_note_url() !=
        new_install_info.note_taking_new_note_url) {
      return true;
    }
    if (existing_web_app.capture_links() != new_install_info.capture_links) {
      return true;
    }
    if (existing_web_app.file_handlers() != new_install_info.file_handlers) {
      return true;
    }
    if (existing_web_app.background_color() !=
        new_install_info.background_color) {
      return true;
    }
    if (existing_web_app.dark_mode_theme_color() !=
        new_install_info.dark_mode_theme_color) {
      return true;
    }
    if (existing_web_app.dark_mode_background_color() !=
        new_install_info.dark_mode_background_color) {
      return true;
    }
    if (existing_web_app.manifest_url() != new_install_info.manifest_url) {
      return true;
    }
    if (existing_web_app.launch_handler() != new_install_info.launch_handler) {
      return true;
    }
    if (existing_web_app.permissions_policy() !=
        new_install_info.permissions_policy) {
      return true;
    }
    if (existing_shortcuts_menu_icon_bitmaps &&
        *existing_shortcuts_menu_icon_bitmaps !=
            new_install_info.shortcuts_menu_icon_bitmaps) {
      return true;
    }
    if (existing_web_app.scope_extensions() !=
        new_install_info.scope_extensions) {
      return true;
    }
    if (new_install_info.validated_scope_extensions.has_value() &&
        existing_web_app.validated_scope_extensions() !=
            new_install_info.validated_scope_extensions.value()) {
      return true;
    }
    if (existing_web_app.tab_strip() != new_install_info.tab_strip) {
      return true;
    }
    // TODO(crbug.com/40611449): Check more manifest fields.
    return false;
  }();

  return result;
}

void RecordIdentityConfirmationMetrics(
    const ManifestDataChanges& manifest_data_changes,
    const WebApp& web_app) {
  // This is used for metrics, so do not remove or reorder existing entries.
  enum AppIdentityDisplayMetric {
    kNoAppIdentityChange = 0,
    kIconChanging = 1,
    // Values 2 and 3 are reserved for Android (icon mask).
    kAppNameChanging = 4,
    kAppNameAndIconChanging = 5,
    // Values 6 through 15 (inclusive) are reserved for Android (icon mask/app
    // short name).
    kLastAndroidSpecificValue = 15,

    // Add any new values above this one, and update kMaxValue to the highest
    // enumerator value.
    kMaxValue = 15
  };

  AppIdentityDisplayMetric app_id_changes = [&] {
    if (manifest_data_changes.app_name_changed &&
        manifest_data_changes.app_icon_identity_change) {
      return kAppNameAndIconChanging;
    }
    if (manifest_data_changes.app_name_changed) {
      return kAppNameChanging;
    }
    if (manifest_data_changes.app_icon_identity_change) {
      return kIconChanging;
    }
    return kNoAppIdentityChange;
  }();

  if (manifest_data_changes.RequiresConfirmation()) {
    base::UmaHistogramEnumeration("Webapp.AppIdentityDialog.Showing",
                                  app_id_changes);
    return;
  }

  if (manifest_data_changes.app_name_identity_update_decision ==
          IdentityUpdateDecision::kSilentlyAllow ||
      manifest_data_changes.app_icon_identity_update_decision ==
          IdentityUpdateDecision::kSilentlyAllow) {
    base::UmaHistogramEnumeration("Webapp.AppIdentityDialog.AlreadyApproved",
                                  app_id_changes);
    return;
  }

  base::UmaHistogramEnumeration("Webapp.AppIdentityDialog.NotShowing",
                                app_id_changes);
}

}  // namespace web_app
