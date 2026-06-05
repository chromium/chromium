// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#endif

namespace {

using gfx::GetStringWidth;

}  // namespace

DEFINE_USER_DATA(WindowMetadataController);

// static
WindowMetadataController* WindowMetadataController::From(
    BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<WindowMetadataController>::Get(
      browser->GetUnownedUserDataHost());
}

// static
const WindowMetadataController* WindowMetadataController::From(
    const BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<WindowMetadataController>::Get(
      browser->GetUnownedUserDataHost());
}

WindowMetadataController::WindowMetadataController(
    BrowserWindowInterface& browser,
    const std::string& initial_user_title)
    : browser_(browser.GetBrowserForMigrationOnly()),
      user_title_(initial_user_title),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {}

WindowMetadataController::~WindowMetadataController() = default;

gfx::Image WindowMetadataController::GetCurrentPageIcon() const {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  // |web_contents| can be NULL since GetCurrentPageIcon() is called by the
  // window during the window's creation (before tabs have been added).
  favicon::FaviconDriver* favicon_driver =
      web_contents
          ? favicon::ContentFaviconDriver::FromWebContents(web_contents)
          : nullptr;
  return favicon_driver ? favicon_driver->GetFavicon() : gfx::Image();
}

std::u16string WindowMetadataController::GetWindowTitleForCurrentTab(
    bool include_app_name) const {
  if (!user_title_.empty()) {
    return base::UTF8ToUTF16(user_title_);
  }

  // For document picture-in-picture windows, we use the title from the opener
  // WebContents instead of the picture-in-picture WebContents itself.
  content::WebContents* web_contents_for_title =
      browser_->is_type_picture_in_picture()
          ? PictureInPictureWindowManager::GetInstance()->GetWebContents()
          : browser_->tab_strip_model()->GetActiveWebContents();

  return GetWindowTitleFromWebContents(include_app_name,
                                       web_contents_for_title);
}

std::u16string WindowMetadataController::GetWindowTitleForTab(
    const tabs::TabHandle& tab) const {
  std::u16string title = base::UTF8ToUTF16(user_title_);

  if (title.empty()) {
    title = tab.Get()->GetContents()->GetTitle();
    if (browser_->is_type_picture_in_picture()) {
      content::WebContents* pip_web_contents =
          PictureInPictureWindowManager::GetInstance()->GetWebContents();
      if (pip_web_contents) {
        title = pip_web_contents->GetTitle();
      }
    }
    title = FormatTitleForDisplay(title);
  }

  if (title.empty() &&
      (browser_->is_type_normal() || browser_->is_type_popup())) {
    title = CoreTabHelper::GetDefaultTitle();
  }

  return title;
}

std::u16string WindowMetadataController::GetTitleForTab(
    const tabs::TabHandle& tab) const {
  std::u16string title =
      FormatTitleForDisplay(tab.Get()->GetContents()->GetTitle());

  if (title.empty()) {
    title = CoreTabHelper::GetDefaultTitle();
  }

  return title;
}

std::u16string WindowMetadataController::GetWindowTitleForMaxWidth(
    int max_width) const {
  static constexpr unsigned int kMinTitleCharacters = 4;
  const gfx::FontList font_list;

  if (!user_title_.empty()) {
    std::u16string title = base::UTF8ToUTF16(user_title_);
    std::u16string pixel_elided_title = gfx::ElideText(
        title, font_list, max_width, gfx::ElideBehavior::ELIDE_TAIL);
    std::u16string character_elided_title =
        gfx::TruncateString(title, kMinTitleCharacters, gfx::CHARACTER_BREAK);
    return pixel_elided_title.size() > character_elided_title.size()
               ? pixel_elided_title
               : character_elided_title;
  }

  const auto num_more_tabs = browser_->tab_strip_model()->count() - 1;
  const std::u16string format_string = l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_WINDOW_TITLE_MENU_ENTRY, num_more_tabs);

  // First, format with an empty string to see how much space we have available.
  std::u16string temp_window_title =
      base::ReplaceStringPlaceholders(format_string, std::u16string(), nullptr);
  int width = max_width - GetStringWidth(temp_window_title, font_list);

  std::u16string title;
  content::WebContents* contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  // |contents| can be NULL if GetWindowTitleForMenu is called during the
  // window's creation (before tabs have been added).
  if (contents) {
    auto* const app_browser_controller =
        web_app::AppBrowserController::From(browser_);
    title = FormatTitleForDisplay(app_browser_controller
                                      ? app_browser_controller->GetTitle()
                                      : contents->GetTitle());
  }

  // If there is no title, leave it empty for apps.
  if (title.empty() &&
      (browser_->is_type_normal() || browser_->is_type_popup())) {
    title = CoreTabHelper::GetDefaultTitle();
  }

  // Try to elide the title to fit the pixel width. If that will make the title
  // shorter than the minimum character limit, use a character elided title
  // instead.
  std::u16string pixel_elided_title =
      gfx::ElideText(title, font_list, width, gfx::ElideBehavior::ELIDE_TAIL);
  std::u16string character_elided_title =
      gfx::TruncateString(title, kMinTitleCharacters, gfx::CHARACTER_BREAK);
  title = pixel_elided_title.size() > character_elided_title.size()
              ? pixel_elided_title
              : character_elided_title;

  // Finally, add the page title.
  return base::ReplaceStringPlaceholders(format_string, title, nullptr);
}

std::u16string WindowMetadataController::GetWindowTitleFromWebContents(
    bool include_app_name,
    content::WebContents* contents) const {
  std::u16string title = base::UTF8ToUTF16(user_title_);

  // |contents| can be NULL because GetWindowTitleForCurrentTab is called by the
  // window during the window's creation (before tabs have been added).
  if (title.empty() && contents) {
    auto* const app_browser_controller =
        web_app::AppBrowserController::From(browser_);
    title = FormatTitleForDisplay(app_browser_controller
                                      ? app_browser_controller->GetTitle()
                                      : contents->GetTitle());
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
    // If the app name is requested and this is a captive portal window, the
    // title should indicate that this is a captive portal window. Captive
    // portal windows should always be pop-ups, and the is_captive_portal_window
    // condition should not change over the lifetime of a WebContents.
    if (include_app_name &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(contents) &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(contents)
            ->is_captive_portal_window()) {
      DCHECK(browser_->is_type_popup());
      return l10n_util::GetStringFUTF16(
          IDS_CAPTIVE_PORTAL_BROWSER_WINDOW_TITLE_FORMAT,
          title.empty() ? CoreTabHelper::GetDefaultTitle() : title);
    }
#endif
  }

  // If there is no title, leave it empty for apps.
  if (title.empty() &&
      (browser_->is_type_normal() || browser_->is_type_popup())) {
    title = CoreTabHelper::GetDefaultTitle();
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, we don't want to suffix the page title with the application name.
  return title;
#else
  // If there is no title and this is an app, fall back on the app name. This
  // ensures that the native window gets a title which is important for a11y,
  // for example the window selector uses the Aura window title.
  if (title.empty() &&
      (browser_->is_type_app() || browser_->is_type_app_popup() ||
       browser_->is_type_devtools()) &&
      include_app_name) {
    auto* const app_browser_controller =
        web_app::AppBrowserController::From(browser_);
    return app_browser_controller ? app_browser_controller->GetAppShortName()
                                  : base::UTF8ToUTF16(browser_->app_name());
  }
  // Include the app name in window titles for tabbed browser windows when
  // requested with |include_app_name|.
  return ((browser_->is_type_normal() || browser_->is_type_popup()) &&
          include_app_name)
             ? l10n_util::GetStringFUTF16(IDS_BROWSER_WINDOW_TITLE_FORMAT,
                                          title)
             : title;
#endif  // BUILDFLAG(IS_MAC)
}

// static
std::u16string WindowMetadataController::FormatTitleForDisplay(
    std::u16string title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title.find(L'\n', current_index)) !=
         std::u16string::npos) {
    title.replace(match_index, 1, std::u16string());
    current_index = match_index;
  }

  return title;
}

void WindowMetadataController::SetWindowUserTitle(
    const std::string& user_title) {
  user_title_ = user_title;
  browser_->window()->UpdateTitleBar();
  // See comment in Browser::OnTabGroupChanged
  DCHECK(!IsRelevantToAppSessionService(browser_->type()));
  SessionService* const session_service =
      SessionServiceFactory::GetForProfile(browser_->profile());
  if (session_service) {
    session_service->SetWindowUserTitle(browser_->session_id(), user_title);
  }
}
