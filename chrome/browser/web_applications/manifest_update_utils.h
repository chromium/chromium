// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_UTILS_H_

#include <iosfwd>
#include <string>

#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebApp;
class WebAppRegistrar;

constexpr SquareSizePx kIdentitySizes[] = {kInstallIconSize, kLauncherIconSize};

// This enum is recorded by UMA, the numeric values must not change.
enum class ManifestUpdateResult {
  kNoAppInScope = 0,
  kThrottled = 1,
  kWebContentsDestroyed = 2,
  kAppUninstalling = 3,
  kAppIsPlaceholder = 4,
  kAppUpToDate = 5,
  kAppNotEligible = 6,
  kAppUpdateFailed = 7,
  kAppUpdated = 8,
  kAppIsSystemWebApp = 9,
  kIconDownloadFailed = 10,
  kIconReadFromDiskFailed = 11,
  kAppIdMismatch = 12,
  // kAppAssociationsUpdateFailed = 13,
  // kAppAssociationsUpdated = 14,
  kSystemShutdown = 15,
  kAppIdentityUpdateRejectedAndUninstalled = 16,
  kAppIsIsolatedWebApp = 17,
  kCancelledDueToMainFrameNavigation = 18,
  kShortcutIgnoresManifest = 19,
  kMaxValue = kShortcutIgnoresManifest,
};

std::ostream& operator<<(std::ostream& os, ManifestUpdateResult result);

// Not actually used in production logic. This is just for async code
// organisation and debugging output.
enum class ManifestUpdateCheckStage {
  kPendingAppLock,
  kDownloadingNewManifestData,
  kLoadingExistingManifestData,
  kComparingManifestData,
  kResolvingIdentityChanges,
  kComplete,
};

std::ostream& operator<<(std::ostream& os, ManifestUpdateCheckStage stage);

enum class ManifestUpdateCheckResult {
  kAppIdMismatch,
  kAppNotEligible,
  kSystemShutdown,
  kAppUpdateNeeded,
  kAppIdentityUpdateRejectedAndUninstalled,
  kAppUpToDate,
  kIconDownloadFailed,
  kIconReadFromDiskFailed,
  kWebContentsDestroyed,
  kCancelledDueToMainFrameNavigation,
};

std::ostream& operator<<(std::ostream& os, ManifestUpdateCheckResult result);

ManifestUpdateResult FinalResultFromManifestUpdateCheckResult(
    ManifestUpdateCheckResult check_result);

void RecordIconDownloadMetrics(IconsDownloadedResult result,
                               DownloadedIconsHttpResults icons_http_results);

bool CanWebAppSilentlyUpdateIdentity(const WebApp& web_app);
bool CanShowIdentityUpdateConfirmationDialog(const WebAppRegistrar& registrar,
                                             const WebApp& web_app);

struct AppIconIdentityChange {
  SkBitmap before;
  SkBitmap after;
};

enum class IdentityUpdateDecision {
  kRevert,
  kGetUserConfirmation,
  kSilentlyAllow,
};

// Represents what's different between two sets of manifest data split up by
// whether the parts are important to app identity. Also captures whether
// identity changes are allowed/pending/reverted.
struct ManifestDataChanges {
  ManifestDataChanges();
  ManifestDataChanges(ManifestDataChanges&&);
  ManifestDataChanges& operator=(ManifestDataChanges&&);
  ~ManifestDataChanges();

  bool app_name_changed = false;

  std::optional<AppIconIdentityChange> app_icon_identity_change;

  // `any_app_icon_changed` represents whether any app icon has changed
  // including identity and non-identity affecting app icons because reverting
  // changes to identity app icons will revert all app icon changes.
  bool any_app_icon_changed = false;

  bool other_fields_changed = false;

  std::optional<IdentityUpdateDecision> app_name_identity_update_decision;
  std::optional<IdentityUpdateDecision> app_icon_identity_update_decision;

  bool HasIdentityChanges() const {
    return app_name_changed || app_icon_identity_change;
  }

  bool RequiresConfirmation() const {
    return app_name_identity_update_decision ==
               IdentityUpdateDecision::kGetUserConfirmation ||
           app_icon_identity_update_decision ==
               IdentityUpdateDecision::kGetUserConfirmation;
  }

  explicit operator bool() const {
    return app_name_changed || any_app_icon_changed || other_fields_changed;
  }
};

// `existing_app_icon_bitmaps` and `existing_shortcuts_menu_icon_bitmaps` are
// optional and will not be checked if not provided.
ManifestDataChanges GetManifestDataChanges(
    const WebApp& existing_web_app,
    const IconBitmaps* existing_app_icon_bitmaps,
    const ShortcutsMenuIconBitmaps* existing_shortcuts_menu_icon_bitmaps,
    const WebAppInstallInfo& new_install_info);

std::optional<AppIconIdentityChange> CompareIdentityIconBitmaps(
    const IconBitmaps& existing_app_icon_bitmaps,
    const IconBitmaps& new_app_icon_bitmaps);

void RecordIdentityConfirmationMetrics(
    const ManifestDataChanges& manifest_data_changes,
    const WebApp& web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_UTILS_H_
