// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/chrome_bluetooth_chooser_controller.h"

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

Browser* GetBrowser() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetLastUsedProfileAllowedByPolicy());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

}  // namespace

ChromeBluetoothChooserController::ChromeBluetoothChooserController(
    content::RenderFrameHost* owner,
    const content::BluetoothChooser::EventHandler& event_handler)
    : permissions::BluetoothChooserController(
          owner,
          event_handler,
          CreateChooserTitle(owner, IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT)) {}

ChromeBluetoothChooserController::~ChromeBluetoothChooserController() = default;

void ChromeBluetoothChooserController::OpenAdapterOffHelpUrl() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS can directly link to the OS setting to turn on the adapter.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      GetBrowser()->profile(),
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath);
#else
  // For other operating systems, show a help center page in a tab.
  GetBrowser()->OpenURL(
      content::OpenURLParams(GURL(chrome::kBluetoothAdapterOffHelpURL),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             false /* is_renderer_initialized */),
      /*navigation_handle_callback=*/{});
#endif
}

void ChromeBluetoothChooserController::OpenPermissionPreferences() const {
#if BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Bluetooth);
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void ChromeBluetoothChooserController::OpenHelpCenterUrl() const {
  GetBrowser()->OpenURL(
      content::OpenURLParams(GURL(permissions::kChooserBluetoothOverviewURL),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             false /* is_renderer_initialized */),
      /*navigation_handle_callback=*/{});
}
