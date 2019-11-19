// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_item.h"

#include <stddef.h>
#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_url_handlers.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/skia_util.h"

using extensions::Extension;

ExtensionAppItem::ExtensionAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_platform_app)
    : ChromeAppListItem(profile, extension_id),
      extension_name_(extension_name),
      installing_icon_(CreateDisabledIcon(installing_icon)),
      is_platform_app_(is_platform_app) {
  Reload();
  if (sync_item && sync_item->item_ordinal.IsValid())
    UpdateFromSync(sync_item);
  else
    SetDefaultPositionIfApplicable(model_updater);

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

ExtensionAppItem::~ExtensionAppItem() = default;

void ExtensionAppItem::Reload() {
  const Extension* extension = GetExtension();
  bool is_installing = !extension;
  SetIsInstalling(is_installing);
  if (is_installing) {
    SetName(extension_name_);
    SetIcon(installing_icon_);
    return;
  }
  SetNameAndShortName(extension->name(), extension->short_name());
  if (!icon_) {
    icon_ = extensions::ChromeAppIconService::Get(profile())->CreateIcon(
        this, extension_id(),
        ash::AppListConfig::instance().grid_icon_dimension(),
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd));
  } else {
    icon_->Reload();
  }
}

void ExtensionAppItem::OnIconUpdated(extensions::ChromeAppIcon* icon) {
  SetIcon(icon->IsValid() ? icon->image_skia() : installing_icon_);
}

const Extension* ExtensionAppItem::GetExtension() const {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const Extension* extension = registry->GetInstalledExtension(extension_id());
  return extension;
}

bool ExtensionAppItem::RunExtensionEnableFlow() {
  if (extensions::util::IsAppLaunchableWithoutEnabling(extension_id(),
                                                       profile()))
    return false;

  if (!extension_enable_flow_) {
    extension_enable_flow_ =
        std::make_unique<ExtensionEnableFlow>(profile(), extension_id(), this);
    extension_enable_flow_->StartForNativeWindow(nullptr);
  }
  return true;
}

void ExtensionAppItem::Launch(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  // Don't auto-enable apps that cannot be launched.
  if (!extensions::util::IsAppLaunchable(extension_id(), profile()))
    return;

  if (RunExtensionEnableFlow())
    return;

  GetController()->LaunchApp(profile(), extension,
                             AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                             event_flags, display::kInvalidDisplayId);
}

void ExtensionAppItem::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();
  // Automatically launch app after enabling.
  Launch(ui::EF_NONE);
}

void ExtensionAppItem::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
}

void ExtensionAppItem::Activate(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  // Don't auto-enable apps that cannot be launched.
  if (!extensions::util::IsAppLaunchable(extension_id(), profile()))
    return;

  if (RunExtensionEnableFlow())
    return;

  extensions::RecordAppListMainLaunch(extension);
  GetController()->ActivateApp(profile(), extension,
                               AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                               event_flags);
}

void ExtensionAppItem::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<app_list::ExtensionAppContextMenu>(
      this, profile(), extension_id(), GetController(), is_platform_app_);
  context_menu_->GetMenuModel(std::move(callback));
}

// static
const char ExtensionAppItem::kItemType[] = "ExtensionAppItem";

const char* ExtensionAppItem::GetItemType() const {
  return ExtensionAppItem::kItemType;
}

bool ExtensionAppItem::IsBadged() const {
  return icon_ && icon_->icon_is_badged();
}

app_list::AppContextMenu* ExtensionAppItem::GetAppContextMenu() {
  return context_menu_.get();
}

void ExtensionAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags);
}
