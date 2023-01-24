// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_utils.h"

#include <ostream>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

// Some apps, such as pre-installed apps, have been vetted and are therefore
// considered safe and permitted to update their icon. For others, the
// feature flag needs to be on.
bool AllowUnpromptedIconUpdate(const AppId& app_id,
                               const WebAppRegistrar& registrar) {
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app)
    return false;
  return CanWebAppUpdateIdentity(web_app) ||
         base::FeatureList::IsEnabled(features::kWebAppManifestIconUpdating);
}

}  // namespace

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
  }
}

std::ostream& operator<<(std::ostream& os, ManifestUpdateStage stage) {
  switch (stage) {
    case ManifestUpdateStage::kPendingInstallableData:
      return os << "kPendingInstallableData";
    case ManifestUpdateStage::kPendingIconDownload:
      return os << "kPendingIconDownload";
    case ManifestUpdateStage::kPendingIconReadFromDisk:
      return os << "kPendingIconReadFromDisk";
    case ManifestUpdateStage::kPendingAppIdentityCheck:
      return os << "kPendingAppIdentityCheck";
    case ManifestUpdateStage::kAppWindowsClosed:
      return os << "kAppWindowsClosed";
    case ManifestUpdateStage::kPendingFinalizerUpdate:
      return os << "kPendingFinalizerUpdate";
  }
}

bool AllowUnpromptedNameUpdate(const AppId& app_id,
                               const WebAppRegistrar& registrar) {
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app)
    return false;
  return CanWebAppUpdateIdentity(web_app);
}

bool NeedsAppIdentityUpdateDialog(bool title_changing,
                                  bool icons_changing,
                                  const AppId& app_id,
                                  const WebAppRegistrar& registrar) {
  // Shortcut apps can trigger the update check (https://crbug.com/1366600)
  // on subsequent runs of the app, if the user changed the title of the app
  // when creating the shortcut. But we should never run the App Identity dialog
  // for shortcut apps. Also, ideally we should just use IsShortcutApp here
  // instead of checking the install source, but as per
  // https://crbug.com/1368592 there is a bug with that where it returns the
  // wrong thing for Shortcut apps that specify `scope`.
  if (registrar.IsShortcutApp(app_id) ||
      registrar.GetAppInstallSourceForMetrics(app_id) ==
          webapps::WebappInstallSource::MENU_CREATE_SHORTCUT) {
    return false;
  }

  if (title_changing && !AllowUnpromptedNameUpdate(app_id, registrar))
    return true;
  if (icons_changing && !AllowUnpromptedIconUpdate(app_id, registrar))
    return true;
  return false;
}

bool IsUpdateNeededForManifest(const AppId& app_id,
                               const WebAppInstallInfo& install_info,
                               const WebAppRegistrar& registrar) {
  const WebApp* app = registrar.GetAppById(app_id);
  DCHECK(app);

  // TODO(crbug.com/1259777): Check whether translations have been updated.
  bool title_changing =
      install_info.title != base::UTF8ToUTF16(app->untranslated_name());
  bool icons_changing = install_info.manifest_icons != app->manifest_icons();
  if (!NeedsAppIdentityUpdateDialog(title_changing, icons_changing, app_id,
                                    registrar)) {
    if (title_changing && AllowUnpromptedNameUpdate(app_id, registrar)) {
      return true;
    }
    if (icons_changing && AllowUnpromptedIconUpdate(app_id, registrar)) {
      return true;
    }
  }

  // Allows updating start_url and manifest_id. Both fields are allowed to
  // change as long as the app_id generated from them doesn't change.
  {
    if (install_info.manifest_id != app->manifest_id())
      return true;
    if (install_info.start_url != app->start_url())
      return true;
  }

  if (install_info.theme_color != app->theme_color())
    return true;

  if (install_info.scope != app->scope())
    return true;

  if (install_info.display_mode != app->display_mode())
    return true;

  if (install_info.display_override != app->display_mode_override())
    return true;

  if (install_info.shortcuts_menu_item_infos !=
      app->shortcuts_menu_item_infos()) {
    return true;
  }

  if (install_info.share_target != app->share_target())
    return true;

  if (install_info.protocol_handlers != app->protocol_handlers())
    return true;

  if (install_info.url_handlers != app->url_handlers())
    return true;

  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppManifestLockScreen) &&
      install_info.lock_screen_start_url != app->lock_screen_start_url()) {
    return true;
  }

  if (install_info.note_taking_new_note_url !=
      app->note_taking_new_note_url()) {
    return true;
  }

  if (install_info.capture_links != app->capture_links())
    return true;

  if (app->file_handlers() != install_info.file_handlers)
    return true;

  if (install_info.background_color != app->background_color())
    return true;

  if (install_info.dark_mode_theme_color != app->dark_mode_theme_color()) {
    return true;
  }

  if (install_info.dark_mode_background_color !=
      app->dark_mode_background_color()) {
    return true;
  }

  if (install_info.manifest_url != app->manifest_url())
    return true;

  if (install_info.launch_handler != app->launch_handler())
    return true;

  if (install_info.permissions_policy != app->permissions_policy())
    return true;

  // TODO(crbug.com/897314): Check changes to tab_strip field once icons are
  // stored.
  // TODO(crbug.com/1212849): Handle changes to is_storage_isolated.
  // TODO(crbug.com/926083): Check more manifest fields.
  return false;
}

}  // namespace web_app
