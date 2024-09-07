// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#else
#include "chrome/common/url_constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/functional/callback_helpers.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#endif

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;

namespace {

static bool g_disable_ui_for_tests = false;

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
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ShowToast(const std::string& id,
               ash::ToastCatalogName catalog_name,
               const std::u16string& text) {
  ash::ToastManager::Get()->Show(ash::ToastData(id, catalog_name, text));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ShowInstalledNotification(
    scoped_refptr<const extensions::Extension> extension,
    Profile* profile) {
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      std::string(kExtensionInstallSuccessToastId),
      /*title=*/std::u16string(),
      /*message=*/
      l10n_util::GetStringFUTF16(IDS_EXTENSION_NOTIFICATION_INSTALLED,
                                 base::UTF8ToUTF16(extension->name())),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 extension->id()),
      /*optional_fields=*/message_center::RichNotificationData(),
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::DoNothingAs<void()>()));

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}
#endif

void ShowAppInstalledNotification(
    scoped_refptr<const extensions::Extension> extension,
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowToast(kExtensionInstallSuccessToastId,
            ash::ToastCatalogName::kExtensionInstallSuccess,
            l10n_util::GetStringFUTF16(IDS_EXTENSION_NOTIFICATION_INSTALLED,
                                       base::UTF8ToUTF16(extension->name())));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowInstalledNotification(extension, profile);
#elif BUILDFLAG(IS_CHROMEOS)
  // chrome://apps/ is not available on ChromeOS.
  NOTREACHED_IN_MIGRATION();
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

ExtensionInstallUI::ExtensionInstallUI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      skip_post_install_ui_(false),
      use_app_installed_bubble_(false) {}

ExtensionInstallUI::~ExtensionInstallUI() {}

void ExtensionInstallUI::OnInstallSuccess(
    scoped_refptr<const extensions::Extension> extension,
    const SkBitmap* icon) {
  if (g_disable_ui_for_tests || skip_post_install_ui_ ||
      extension->is_theme()) {
    return;
  }

  if (!profile_) {
    // TODO(zelidrag): Figure out what exact conditions cause crash
    // http://crbug.com/159437 and write browser test to cover it.
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // Extensions aren't enabled by default in incognito so we confirm
  // the install in a normal window.
  Profile* current_profile = profile_->GetOriginalProfile();
  Browser* browser = FindOrCreateVisibleBrowser(current_profile);

  if (!extension->is_app()) {
    ShowBubble(extension, browser, *icon);
    return;
  }

  if (!use_app_installed_bubble_) {
    ShowAppInstalledNotification(extension, profile_);
    return;
  }

  if (browser) {
    ShowBubble(extension, browser, *icon);
  }
}

void ExtensionInstallUI::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_disable_ui_for_tests || skip_post_install_ui_) {
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
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

void ExtensionInstallUI::SetUseAppInstalledBubble(bool use_bubble) {
  use_app_installed_bubble_ = use_bubble;
}

void ExtensionInstallUI::SetSkipPostInstallUI(bool skip_ui) {
  skip_post_install_ui_ = skip_ui;
}

// static
base::AutoReset<bool> ExtensionInstallUI::disable_ui_for_tests(bool disable) {
  CHECK_IS_TEST();
  return base::AutoReset<bool>(&g_disable_ui_for_tests, disable);
}
