// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_default.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/extensions/extension_installed_notification.h"
#else
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#endif

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;

namespace {

Browser* FindOrCreateVisibleBrowser(Profile* profile) {
  // TODO(mpcomplete): remove this workaround for http://crbug.com/244246
  // after fixing http://crbug.com/38676.
  if (!IncognitoModePrefs::CanOpenBrowser(profile))
    return nullptr;
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  if (browser->tab_strip_model()->count() == 0)
    chrome::AddTabAt(browser, GURL(), -1, true);
  return browser;
}

}  // namespace

ExtensionInstallUIDefault::ExtensionInstallUIDefault(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      skip_post_install_ui_(false),
      use_app_installed_bubble_(false) {}

ExtensionInstallUIDefault::~ExtensionInstallUIDefault() {}

void ExtensionInstallUIDefault::OnInstallSuccess(
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap* icon) {
  if (disable_ui_for_tests() || skip_post_install_ui_ || extension->is_theme())
    return;

  if (!profile_) {
    // TODO(zelidrag): Figure out what exact conditions cause crash
    // http://crbug.com/159437 and write browser test to cover it.
    NOTREACHED();
    return;
  }

  // Extensions aren't enabled by default in incognito so we confirm
  // the install in a normal window.
  Profile* current_profile = profile_->GetOriginalProfile();
  Browser* browser = FindOrCreateVisibleBrowser(current_profile);
  if (extension->is_app()) {
    if (use_app_installed_bubble_) {
      if (browser)
        ShowPlatformBubble(extension, browser, *icon);
      return;
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ExtensionInstalledNotification::Show(extension.get(), current_profile);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    OpenAppInstalledUI(extension->id());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return;
  }

  ShowPlatformBubble(extension, browser, *icon);
}

void ExtensionInstallUIDefault::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (disable_ui_for_tests() || skip_post_install_ui_)
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)  // Can be nullptr in unittests.
    return;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  InstallationErrorInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents), error);
}

void ExtensionInstallUIDefault::OpenAppInstalledUI(const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Notification always enabled on ChromeOS, so always handled in
  // OnInstallSuccess.
  NOTREACHED();
#else
  Profile* current_profile = profile_->GetOriginalProfile();
  Browser* browser = FindOrCreateVisibleBrowser(current_profile);
  if (browser) {
    NavigateParams params(
        GetSingletonTabNavigateParams(browser, GURL(chrome::kChromeUIAppsURL)));
    Navigate(&params);
  }
#endif
}

void ExtensionInstallUIDefault::SetUseAppInstalledBubble(bool use_bubble) {
  use_app_installed_bubble_ = use_bubble;
}

void ExtensionInstallUIDefault::SetSkipPostInstallUI(bool skip_ui) {
  skip_post_install_ui_ = skip_ui;
}

gfx::NativeWindow ExtensionInstallUIDefault::GetDefaultInstallDialogParent() {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return contents->GetTopLevelNativeWindow();
  }
  return nullptr;
}
