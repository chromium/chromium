// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/extension_app_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/events/event_constants.h"

namespace app_list {

ExtensionAppResult::ExtensionAppResult(Profile* profile,
                                       const std::string& app_id,
                                       AppListControllerDelegate* controller,
                                       bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id).spec());

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  DCHECK(extension);

  is_platform_app_ = extension->is_platform_app();
  icon_ = extensions::ChromeAppIconService::Get(profile)->CreateIcon(
      this, app_id,
      AppListConfig::instance().GetPreferredIconDimension(display_type()),
      base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd));
  // Load an additional chip icon when it is a recommendation result
  // so that it renders clearly in both a chip and a tile.
  if (display_type() == ash::SearchResultDisplayType::kRecommendation) {
    chip_icon_ = extensions::ChromeAppIconService::Get(profile)->CreateIcon(
        this, app_id,
        AppListConfig::instance().suggestion_chip_icon_dimension(),
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd));
  }

  StartObservingExtensionRegistry();
}

ExtensionAppResult::~ExtensionAppResult() {
  StopObservingExtensionRegistry();
}

void ExtensionAppResult::Open(int event_flags) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
          app_id());
  if (!extension)
    return;

  // Don't auto-enable apps that cannot be launched.
  if (!extensions::util::IsAppLaunchable(app_id(), profile()))
    return;

  // Check if enable flow is already running or should be started.
  if (RunExtensionEnableFlow())
    return;

  // Record the search metrics if the ChromeSearchResult is not a suggested app.
  if (display_type() != ash::SearchResultDisplayType::kRecommendation) {
    RecordHistogram(APP_SEARCH_RESULT);
    extensions::RecordAppListSearchLaunch(extension);
  }

  controller()->ActivateApp(
      profile(), extension,
      AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH, event_flags);
}

void ExtensionAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  if (!context_menu_) {
    context_menu_ = std::make_unique<ExtensionAppContextMenu>(
        this, profile(), app_id(), controller());
    context_menu_->set_is_platform_app(is_platform_app_);
  }

  context_menu_->GetMenuModel(std::move(callback));
}

void ExtensionAppResult::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);

  extension_registry_ = extensions::ExtensionRegistry::Get(profile());
  extension_registry_->AddObserver(this);
}

void ExtensionAppResult::StopObservingExtensionRegistry() {
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
  extension_registry_ = NULL;
}

bool ExtensionAppResult::RunExtensionEnableFlow() {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id(), profile()))
    return false;

  if (!extension_enable_flow_) {
    extension_enable_flow_ =
        std::make_unique<ExtensionEnableFlow>(profile(), app_id(), this);
    extension_enable_flow_->StartForNativeWindow(nullptr);
  }
  return true;
}

AppContextMenu* ExtensionAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

void ExtensionAppResult::OnIconUpdated(extensions::ChromeAppIcon* icon) {
  const gfx::Size icon_size(
      AppListConfig::instance().GetPreferredIconDimension(display_type()),
      AppListConfig::instance().GetPreferredIconDimension(display_type()));
  const gfx::Size chip_icon_size(
      AppListConfig::instance().suggestion_chip_icon_dimension(),
      AppListConfig::instance().suggestion_chip_icon_dimension());
  DCHECK(icon_size != chip_icon_size);

  if (icon->image_skia().size() == icon_size) {
    SetIcon(icon->image_skia());
  } else if (icon->image_skia().size() == chip_icon_size) {
    DCHECK(display_type() == ash::SearchResultDisplayType::kRecommendation);
    SetChipIcon(icon->image_skia());
  } else {
    NOTREACHED();
  }
}

void ExtensionAppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void ExtensionAppResult::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();

  // Automatically open app after enabling.
  Open(ui::EF_NONE);
}

void ExtensionAppResult::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
}

void ExtensionAppResult::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // Old |icon_| might be invalidated for forever in case extension gets
  // updated. In this case we need re-create icon again.
  if (!icon_->IsValid())
    icon_->Reload();
  if (display_type() == ash::SearchResultDisplayType::kRecommendation &&
      !chip_icon_->IsValid()) {
    chip_icon_->Reload();
  }
}

void ExtensionAppResult::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
}

}  // namespace app_list
