// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_menu_model.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#include "ui/views/widget/widget.h"
#endif

namespace {

bool ShouldAllowOpenInChrome(Browser* browser) {
  // Isolated Web Apps shouldn't be opened in Chrome.
  const bool is_isolated_web_app =
      browser->app_controller() &&
      browser->app_controller()->IsIsolatedWebApp();
  // Web Apps with enabled prevent close shouldn't be opened in Chrome.
  const bool prevent_close_enabled =
      browser->app_controller() &&
      browser->app_controller()->IsPreventCloseEnabled();
  return !is_isolated_web_app && !prevent_close_enabled;
}

}  // namespace

constexpr int WebAppMenuModel::kUninstallAppCommandId;
constexpr int WebAppMenuModel::kExtensionsMenuCommandId;

WebAppMenuModel::WebAppMenuModel(ui::AcceleratorProvider* provider,
                                 Browser* browser)
    : AppMenuModel(provider, browser) {}

WebAppMenuModel::~WebAppMenuModel() = default;

bool WebAppMenuModel::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case kUninstallAppCommandId:
      return browser()->app_controller()->CanUserUninstall();
    case kExtensionsMenuCommandId:
      return base::FeatureList::IsEnabled(
                 features::kDesktopPWAsElidedExtensionsMenu) &&
             browser()->window()->GetExtensionsContainer() &&
             browser()->window()->GetExtensionsContainer()->HasAnyExtensions();
    case IDC_OPEN_IN_CHROME: {
      return ShouldAllowOpenInChrome(browser());
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case chromeos::MoveToDesksMenuModel::kMenuCommandId:
      return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
          browser()->window()->GetNativeWindow());
#endif
    default:
      return AppMenuModel::IsCommandIdEnabled(command_id);
  }
}

bool WebAppMenuModel::IsCommandIdVisible(int command_id) const {
  switch (command_id) {
    case IDC_OPEN_IN_CHROME:
      return ShouldAllowOpenInChrome(browser());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case chromeos::MoveToDesksMenuModel::kMenuCommandId:
      return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
          browser()->window()->GetNativeWindow());
#endif
    default:
      return AppMenuModel::IsCommandIdVisible(command_id);
  }
}

void WebAppMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kUninstallAppCommandId:
      LogMenuAction(MENU_ACTION_UNINSTALL_APP);
      browser()->app_controller()->Uninstall(
          webapps::WebappUninstallSource::kAppMenu);
      break;
    case kExtensionsMenuCommandId:
      browser()->window()->GetExtensionsContainer()->ToggleExtensionsMenu();
      break;
    case IDC_OPEN_IN_CHROME:
      if (ShouldAllowOpenInChrome(browser())) {
        AppMenuModel::ExecuteCommand(command_id, event_flags);
      }
      break;
    default:
      AppMenuModel::ExecuteCommand(command_id, event_flags);
      break;
  }
}

void WebAppMenuModel::Build() {
  AddItemWithStringId(IDC_WEB_APP_MENU_APP_INFO,
                      IDS_APP_CONTEXT_MENU_SHOW_INFO);
  size_t app_info_index = GetItemCount() - 1;

  CHECK(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool is_isolated_web_app = browser()->app_controller() &&
                             browser()->app_controller()->IsIsolatedWebApp();

  if (web_contents) {
    std::u16string display_text =
        web_app::AppBrowserController::FormatUrlOrigin(
            web_contents->GetVisibleURL());

    // For Isolated Web Apps the origin's host name is a non-human-readable
    // string of characters, so instead of displaying the origin, the short name
    // of the app will be displayed.
    if (is_isolated_web_app) {
      std::u16string short_name =
          browser()->app_controller()->GetAppShortName();
      // For Isolated Web Apps, |GetAppShortName()| must be non-empty.
      display_text = short_name;
    }
    SetMinorText(app_info_index, display_text);
  }

  SetIcon(app_info_index,
          ui::ImageModel::FromVectorIcon(
              browser()->location_bar_model()->GetVectorIcon()));

  AddSeparator(ui::NORMAL_SEPARATOR);

  if (IsCommandIdEnabled(kExtensionsMenuCommandId)) {
    AddItemWithStringId(kExtensionsMenuCommandId, IDS_SHOW_EXTENSIONS);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (browser()->app_controller() &&
      browser()->app_controller()->has_tab_strip() &&
      !browser()->app_controller()->ShouldHideNewTabButton()) {
    AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  }
  AddItemWithStringId(IDC_COPY_URL, IDS_COPY_URL);

  AddItemWithStringId(IDC_OPEN_IN_CHROME, IDS_OPEN_IN_CHROME);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
          browser()->window()->GetNativeWindow())) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    move_to_desks_submenu_ = std::make_unique<chromeos::MoveToDesksMenuModel>(
        std::make_unique<chromeos::MoveToDesksMenuDelegate>(
            views::Widget::GetWidgetForNativeWindow(
                browser()->window()->GetNativeWindow())));
    AddSubMenuWithStringId(chromeos::MoveToDesksMenuModel::kMenuCommandId,
                           IDS_MOVE_TO_DESKS_MENU,
                           move_to_desks_submenu_.get());
  }
#endif

// Chrome OS's app list is prominent enough to not need a separate uninstall
// option in the app menu.
#if !BUILDFLAG(IS_CHROMEOS)
  DCHECK(browser()->app_controller());
  if (browser()->app_controller()->IsInstalled()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kUninstallAppCommandId,
            l10n_util::GetStringFUTF16(
                IDS_UNINSTALL_FROM_OS_LAUNCH_SURFACE,
                ui::EscapeMenuLabelAmpersands(
                    browser()->app_controller()->GetAppShortName())));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  AddSeparator(ui::NORMAL_SEPARATOR);
  CreateZoomMenu();
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  CreateFindAndEditSubMenu();

  if (media_router::MediaRouterEnabled(browser()->profile())) {
    AddItemWithStringId(IDC_ROUTE_MEDIA, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  }

  SetCommandIcon(this, kExtensionsMenuCommandId,
                 vector_icons::kExtensionChromeRefreshIcon);
  SetCommandIcon(this, kUninstallAppCommandId, kTrashCanRefreshIcon);
  SetCommandIcon(this, IDC_NEW_TAB, kNewTabRefreshIcon);
  SetCommandIcon(this, IDC_COPY_URL, kLinkChromeRefreshIcon);
  SetCommandIcon(this, IDC_OPEN_IN_CHROME, kBrowserLogoIcon);
  SetCommandIcon(this, IDC_ZOOM_MENU, kZoomInIcon);
  SetCommandIcon(this, IDC_PRINT, kPrintMenuIcon);
  SetCommandIcon(this, IDC_FIND_AND_EDIT_MENU, kSearchMenuIcon);
  SetCommandIcon(this, IDC_ROUTE_MEDIA, kCastChromeRefreshIcon);
}
