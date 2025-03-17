// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_desktop.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#else
#include "chrome/common/url_constants.h"
#endif

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;

namespace {

Browser* FindOrCreateVisibleBrowser(Profile* profile) {
  // TODO(mpcomplete): remove this workaround for http://crbug.com/244246
  // after fixing http://crbug.com/38676.
  if (!IncognitoModePrefs::CanOpenBrowser(profile)) {
    return nullptr;
  }
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (browser->tab_strip_model()->count() == 0) {
    chrome::AddTabAt(browser, GURL(), -1, true);
  }
  return browser;
}

#if BUILDFLAG(IS_CHROMEOS)
// Toast id and duration for extension install success.
constexpr char kExtensionInstallSuccessToastId[] = "extension_install_success";

void ShowToast(const std::string& id,
               ash::ToastCatalogName catalog_name,
               const std::u16string& text) {
  ash::ToastManager::Get()->Show(ash::ToastData(id, catalog_name, text));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ShowAppInstalledNotification(
    scoped_refptr<const extensions::Extension> extension,
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  ShowToast(kExtensionInstallSuccessToastId,
            ash::ToastCatalogName::kExtensionInstallSuccess,
            l10n_util::GetStringFUTF16(IDS_EXTENSION_NOTIFICATION_INSTALLED,
                                       base::UTF8ToUTF16(extension->name())));
#else
  Profile* current_profile = profile->GetOriginalProfile();
  Browser* browser = FindOrCreateVisibleBrowser(current_profile);
  if (browser) {
    NavigateParams params(
        GetSingletonTabNavigateParams(browser, GURL(chrome::kChromeUIAppsURL)));
    Navigate(&params);
  }
#endif
}

}  // namespace

ExtensionInstallUIDesktop::ExtensionInstallUIDesktop(
    content::BrowserContext* context)
    : ExtensionInstallUI(context) {}

ExtensionInstallUIDesktop::~ExtensionInstallUIDesktop() = default;

void ExtensionInstallUIDesktop::OnInstallSuccess(
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap* icon) {
  if (IsUiDisabled() || skip_post_install_ui() || extension->is_theme()) {
    return;
  }

  if (!profile()) {
    // TODO(zelidrag): Figure out what exact conditions cause crash
    // http://crbug.com/159437 and write browser test to cover it.
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // Extensions aren't enabled by default in incognito so we confirm
  // the install in a normal window.
  Profile* current_profile = profile()->GetOriginalProfile();
  Browser* browser = FindOrCreateVisibleBrowser(current_profile);

  if (!extension->is_app()) {
    ShowBubble(extension, browser, *icon);
    return;
  }

  if (!use_app_installed_bubble()) {
    ShowAppInstalledNotification(extension, profile());
    return;
  }

  if (browser) {
    ShowBubble(extension, browser, *icon);
  }
}

void ExtensionInstallUIDesktop::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (IsUiDisabled() || skip_post_install_ui()) {
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(profile());
  if (!browser) {  // Can be nullptr in unittests.
    return;
  }
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  InstallationErrorInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents), error);
}
