// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/commerce/browser_utils.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/sharing_hub/screenshot/screenshot_captured_bubble_controller.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/media_router/browser/media_router_dialog_controller.h"  // nogncheck
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_common.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/lens/region_search/lens_region_search_helper.h"
#include "components/lens/lens_features.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#endif

namespace {

const char kOsOverrideForTabletSite[] = "Linux; Android 9; Chrome tablet";
const char kChPlatformOverrideForTabletSite[] = "Android";

// Creates a new tabbed browser window, with the same size, type and profile as
// |original_browser|'s window, inserts |contents| into it, and shows it.
void CreateAndShowNewWindowWithContents(
    std::unique_ptr<content::WebContents> contents,
    const Browser* original_browser) {
  Browser* new_browser = nullptr;
  DCHECK(!original_browser->is_type_app_popup());
  if (original_browser->is_type_app()) {
    new_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        original_browser->app_name(), original_browser->is_trusted_source(),
        gfx::Rect(), original_browser->profile(), true));
  } else {
    new_browser = Browser::Create(Browser::CreateParams(
        original_browser->type(), original_browser->profile(), true));
  }
  // Preserve the size of the original window. The new window has already
  // been given an offset by the OS, so we shouldn't copy the old bounds.
  BrowserWindow* new_window = new_browser->window();
  new_window->SetBounds(
      gfx::Rect(new_window->GetRestoredBounds().origin(),
                original_browser->window()->GetRestoredBounds().size()));

  // We need to show the browser now.  Otherwise ContainerWin assumes the
  // WebContents is invisible and won't size it.
  new_browser->window()->Show();

  // The page transition below is only for the purpose of inserting the tab.
  new_browser->tab_strip_model()->AddWebContents(std::move(contents), -1,
                                                 ui::PAGE_TRANSITION_LINK,
                                                 AddTabTypes::ADD_ACTIVE);
}

bool GetTabURLAndTitleToSave(content::WebContents* web_contents,
                             GURL* url,
                             std::u16string* title) {
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/799668
  if (!web_contents) {
    return false;
  }
  return chrome::GetURLAndTitleToBookmark(web_contents, url, title);
}

ReadingListModel* GetReadingListModel(Browser* browser) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser->profile());
  if (!model || !model->loaded()) {
    return nullptr;  // Ignore requests until model has loaded.
  }
  return model;
}

bool CanMoveWebContentsToReadLater(Browser* browser,
                                   content::WebContents* web_contents,
                                   ReadingListModel* model,
                                   GURL* url,
                                   std::u16string* title) {
  return model && GetTabURLAndTitleToSave(web_contents, url, title) &&
         model->IsUrlSupported(*url) && !browser->profile()->IsGuestSession();
}

bool BookmarkCurrentTabHelper(Browser* browser,
                              bookmarks::BookmarkModel* model,
                              GURL* url,
                              std::u16string* title) {
  if (!model || !model->loaded()) {
    return false;  // Ignore requests until bookmarks are loaded.
  }

  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/799668
  if (!web_contents) {
    return false;
  }
  if (!chrome::GetURLAndTitleToBookmark(web_contents, url, title)) {
    return false;
  }
  bool is_bookmarked_by_any = model->IsBookmarked(*url);
  if (!is_bookmarked_by_any &&
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    // If we're incognito the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    favicon::SaveFaviconEvenIfInIncognito(web_contents);
  }
  return true;
}

}  // namespace

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace chrome {
namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtensionForBrowser(Browser* browser) {
  return extensions::ExtensionRegistry::Get(browser->profile())
      ->GetExtensionById(
          web_app::GetAppIdFromApplicationName(browser->app_name()),
          extensions::ExtensionRegistry::EVERYTHING);
}
#endif

// Based on |disposition|, creates a new tab as necessary, and returns the
// appropriate tab to navigate.  If that tab is the |current_tab|, reverts the
// location bar contents, since all browser-UI-triggered navigations should
// revert any omnibox edits in the |current_tab|.
WebContents* GetTabAndRevertIfNecessaryHelper(Browser* browser,
                                              WindowOpenDisposition disposition,
                                              WebContents* current_tab) {
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      std::unique_ptr<WebContents> new_tab = current_tab->Clone();
      WebContents* raw_new_tab = new_tab.get();
      if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
        new_tab->WasHidden();
      }
      const int index =
          browser->tab_strip_model()->GetIndexOfWebContents(current_tab);
      const auto group = browser->tab_strip_model()->GetTabGroupForTab(index);
      browser->tab_strip_model()->AddWebContents(
          std::move(new_tab), -1, ui::PAGE_TRANSITION_LINK,
          (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB)
              ? AddTabTypes::ADD_ACTIVE
              : AddTabTypes::ADD_NONE,
          group);
      return raw_new_tab;
    }
    case WindowOpenDisposition::NEW_WINDOW: {
      std::unique_ptr<WebContents> new_tab = current_tab->Clone();
      WebContents* raw_new_tab = new_tab.get();
      Browser* new_browser =
          Browser::Create(Browser::CreateParams(browser->profile(), true));
      new_browser->tab_strip_model()->AddWebContents(std::move(new_tab), -1,
                                                     ui::PAGE_TRANSITION_LINK,
                                                     AddTabTypes::ADD_ACTIVE);
      new_browser->window()->Show();
      return raw_new_tab;
    }
    default:
      browser->window()->GetLocationBar()->Revert();
      return current_tab;
  }
}

// Like the above, but auto-computes the current tab
WebContents* GetTabAndRevertIfNecessary(Browser* browser,
                                        WindowOpenDisposition disposition) {
  WebContents* activate_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  return GetTabAndRevertIfNecessaryHelper(browser, disposition, activate_tab);
}

void RecordReloadWithCookieBlocking(const Browser* browser,
                                    WebContents* web_contents) {
  // Figure out if 3P cookies are blocked for this page.
  scoped_refptr<const content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(browser->profile());

  // For this metric, we define "cookies blocked in settings" based on the
  // global opt-in to third-party cookie blocking as well as no overriding
  // content setting on the top-level site.
  bool cookies_blocked_in_settings =
      cookie_settings->ShouldBlockThirdPartyCookies() &&
      !cookie_settings->IsThirdPartyAccessAllowed(
          web_contents->GetLastCommittedURL(), nullptr);

  // Also measure if 3P cookies were actually blocked on the site.
  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  bool cookies_blocked =
      pscs && pscs->blocked_browsing_data_model()->size() > 0U;

  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  ukm::builders::ThirdPartyCookies_BreakageIndicator_UserReload(source_id)
      .SetTPCBlocked(cookies_blocked)
      .SetTPCBlockedInSettings(cookies_blocked_in_settings)
      .Record(ukm::UkmRecorder::Get());
}

void ReloadInternal(Browser* browser,
                    WindowOpenDisposition disposition,
                    bool bypass_cache) {
  const WebContents* const active_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Reloading a tab may change the selection (see crbug.com/339061099), so take
  // a defensive copy into a more stable form before we begin. We take
  // WebContents* so we can follow the tabs as they shift within the same
  // tabstrip (e.g. if `disposition` is NEW_BACKGROUND_TAB).
  std::vector<WebContents*> selected_tabs;
  for (const int selected_index :
       browser->tab_strip_model()->selection_model().selected_indices()) {
    selected_tabs.push_back(
        browser->tab_strip_model()->GetWebContentsAt(selected_index));
  }

  for (WebContents* const selected_tab : selected_tabs) {
    // Skip this tab if it is no longer part of this tabstrip. N.B. we do this
    // instead of using WeakPtr<WebContents> because we do not want to reload
    // tabs that move to another browser.
    if (browser->tab_strip_model()->GetIndexOfWebContents(selected_tab) ==
        TabStripModel::kNoTab) {
      continue;
    }

    WebContents* const new_tab =
        GetTabAndRevertIfNecessaryHelper(browser, disposition, selected_tab);

    // If the selected_tab is the activated page, give the focus to it, as this
    // is caused by a user action
    if (selected_tab == active_contents &&
        !new_tab->FocusLocationBarByDefault()) {
      new_tab->Focus();
    }

    // User reloads is a possible breakage indicator from blocking 3P cookies.
    RecordReloadWithCookieBlocking(browser, selected_tab);

    DevToolsWindow* const devtools =
        DevToolsWindow::GetInstanceForInspectedWebContents(new_tab);
    constexpr content::ReloadType kBypassingType =
        content::ReloadType::BYPASSING_CACHE;
    constexpr content::ReloadType kNormalType = content::ReloadType::NORMAL;
    if (!devtools || !devtools->ReloadInspectedWebContents(bypass_cache)) {
      new_tab->GetController().Reload(
          bypass_cache ? kBypassingType : kNormalType, true);
    }
  }
}

bool IsShowingWebContentsModalDialog(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  // TODO(gbillock): This is currently called in production by the CanPrint
  // method, and may be too restrictive if we allow print preview to overlap.
  // Re-assess how to queue print preview after we know more about popup
  // management policy.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return manager && manager->IsDialogActive();
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
bool PrintPreviewShowing(const Browser* browser) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  auto* controller = printing::PrintPreviewDialogController::GetInstance();
  CHECK(controller);
  return controller->GetPrintPreviewForContents(contents) ||
         controller->is_creating_print_preview_dialog();
#else
  return false;
#endif
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

}  // namespace

bool IsCommandEnabled(Browser* browser, int command) {
  return browser->command_controller()->IsCommandEnabled(command);
}

bool SupportsCommand(Browser* browser, int command) {
  return browser->command_controller()->SupportsCommand(command);
}

bool ExecuteCommand(Browser* browser, int command, base::TimeTicks time_stamp) {
  return browser->command_controller()->ExecuteCommand(command, time_stamp);
}

bool ExecuteCommandWithDisposition(Browser* browser,
                                   int command,
                                   WindowOpenDisposition disposition) {
  return browser->command_controller()->ExecuteCommandWithDisposition(
      command, disposition);
}

void UpdateCommandEnabled(Browser* browser, int command, bool enabled) {
  browser->command_controller()->UpdateCommandEnabled(command, enabled);
}

void AddCommandObserver(Browser* browser,
                        int command,
                        CommandObserver* observer) {
  browser->command_controller()->AddCommandObserver(command, observer);
}

void RemoveCommandObserver(Browser* browser,
                           int command,
                           CommandObserver* observer) {
  browser->command_controller()->RemoveCommandObserver(command, observer);
}

int GetContentRestrictions(const Browser* browser) {
  int content_restrictions = 0;
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (current_tab) {
    CoreTabHelper* core_tab_helper =
        CoreTabHelper::FromWebContents(current_tab);
    content_restrictions = core_tab_helper->content_restrictions();
    NavigationEntry* last_committed_entry =
        current_tab->GetController().GetLastCommittedEntry();
    if (!content::IsSavableURL(last_committed_entry->GetURL())) {
      content_restrictions |= CONTENT_RESTRICTION_SAVE;
    }
  }
  return content_restrictions;
}

void NewEmptyWindow(Profile* profile, bool should_trigger_session_restore) {
  bool off_the_record = profile->IsOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  if (off_the_record) {
    if (IncognitoModePrefs::GetAvailability(prefs) ==
        policy::IncognitoModeAvailability::kDisabled) {
      off_the_record = false;
    }
  } else if (profile->IsGuestSession() ||
             IncognitoModePrefs::ShouldOpenSubsequentBrowsersInIncognito(
                 *base::CommandLine::ForCurrentProcess(), prefs)) {
    off_the_record = true;
  }

  if (off_the_record) {
    // This metric counts the Incognito and Off-The-Record Guest profiles
    // together.
    base::RecordAction(UserMetricsAction("NewIncognitoWindow"));
    if (profile->IsGuestSession()) {
      base::RecordAction(UserMetricsAction("NewGuestWindow"));
    } else {
      base::RecordAction(UserMetricsAction("NewIncognitoWindow2"));
    }
    OpenEmptyWindow(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
                    should_trigger_session_restore);
  } else if (!should_trigger_session_restore) {
    base::RecordAction(UserMetricsAction("NewWindow"));
    OpenEmptyWindow(profile->GetOriginalProfile(),
                    /*should_trigger_session_restore=*/false);
  } else {
    base::RecordAction(UserMetricsAction("NewWindow"));
    SessionService* session_service =
        SessionServiceFactory::GetForProfileForSessionRestore(
            profile->GetOriginalProfile());
    if (!session_service ||
        !session_service->RestoreIfNecessary(StartupTabs(),
                                             /* restore_apps */ false)) {
      OpenEmptyWindow(profile->GetOriginalProfile());
    }
  }
}

Browser* OpenEmptyWindow(Profile* profile,
                         bool should_trigger_session_restore) {
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  // Don't create a new window when the profile is shutting down.
  if (profile->ShutdownStarted()) {
    return nullptr;
  }

  Browser::CreateParams params =
      Browser::CreateParams(Browser::TYPE_NORMAL, profile, true);
  params.should_trigger_session_restore = should_trigger_session_restore;
  Browser* browser = Browser::Create(params);

  // Startup tabs could be created during browser creation. Add an empty tab
  // only if no tabs are created.
  if (browser->tab_strip_model()->empty()) {
    AddTabAt(browser, GURL(), -1, true);
  }

  browser->window()->Show();
  return browser;
}

void OpenWindowWithRestoredTabs(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (service) {
    service->RestoreMostRecentEntry(nullptr);
  }
}

void OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  ScopedTabbedBrowserDisplayer displayer(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  AddSelectedTabWithURL(displayer.browser(), url, ui::PAGE_TRANSITION_LINK);
}

bool CanGoBack(const Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoBack();
}

bool CanGoBack(content::WebContents* web_contents) {
  return web_contents->GetController().CanGoBack();
}

enum class BackNavigationMenuIPHTrigger : int {
  kUserPerformsManyBackNavigation = 0,
  kUserPerformsChainedBackNavigation,
  kUserPerformsChainedBackNavigationWithBackButton
};

const char kBackNavigationMenuIPHExperimentParamName[] = "x_experiment";

void MaybeShowFeatureBackNavigationMenuPromo(Browser* browser,
                                             WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHBackNavigationMenuFeature)) {
    return;
  }

  bool should_show_feature_promo;
  const ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents);
  CHECK(tracker);
  switch (static_cast<BackNavigationMenuIPHTrigger>(
      base::GetFieldTrialParamByFeatureAsInt(
          feature_engagement::kIPHBackNavigationMenuFeature,
          kBackNavigationMenuIPHExperimentParamName, 0))) {
    case BackNavigationMenuIPHTrigger::kUserPerformsChainedBackNavigation:
      should_show_feature_promo =
          tracker->IsChainedBackNavigationRecentlyPerformed();
      break;

    case BackNavigationMenuIPHTrigger::
        kUserPerformsChainedBackNavigationWithBackButton:
      should_show_feature_promo =
          tracker->IsBackButtonChainedBackNavigationRecentlyPerformed();
      break;
    default:
      should_show_feature_promo = true;
      break;
  }

  if (should_show_feature_promo) {
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHBackNavigationMenuFeature);
  }
}

void GoBack(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Back"));

  if (CanGoBack(browser)) {
    WebContents* new_tab = GetTabAndRevertIfNecessary(browser, disposition);
    new_tab->GetController().GoBack();
    MaybeShowFeatureBackNavigationMenuPromo(browser, new_tab);
  }
}

void GoBack(content::WebContents* web_contents) {
  base::RecordAction(UserMetricsAction("Back"));

  if (CanGoBack(web_contents)) {
    web_contents->GetController().GoBack();
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    if (browser) {
      MaybeShowFeatureBackNavigationMenuPromo(browser, web_contents);
    }
  }
}

bool CanGoForward(const Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoForward();
}

bool CanGoForward(content::WebContents* web_contents) {
  return web_contents->GetController().CanGoForward();
}

void GoForward(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(browser)) {
    GetTabAndRevertIfNecessary(browser, disposition)
        ->GetController()
        .GoForward();
  }
}

void GoForward(content::WebContents* web_contents) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(web_contents)) {
    web_contents->GetController().GoForward();
  }
}

void NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disposition) {
  NavigationController* controller =
      &GetTabAndRevertIfNecessary(browser, disposition)->GetController();
  DCHECK_GE(index, 0);
  DCHECK_LT(index, controller->GetEntryCount());
  controller->GoToIndex(index);
}

void Reload(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(browser, disposition, false);
}

void ReloadBypassingCache(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("ReloadBypassingCache"));
  ReloadInternal(browser, disposition, true);
}

bool CanReload(const Browser* browser) {
  return browser && !browser->is_type_devtools() &&
         !browser->is_type_picture_in_picture();
}

void Home(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Home"));

  std::string extra_headers;
#if BUILDFLAG(ENABLE_RLZ)
  // If the home page is a Google home page, add the RLZ header to the request.
  PrefService* pref_service = browser->profile()->GetPrefs();
  if (pref_service) {
    if (google_util::IsGoogleHomePageUrl(
            GURL(pref_service->GetString(prefs::kHomePage)))) {
      extra_headers = rlz::RLZTracker::GetAccessPointHttpHeader(
          rlz::RLZTracker::ChromeHomePage());
    }
  }
#endif  // BUILDFLAG(ENABLE_RLZ)

  GURL url = browser->profile()->GetHomePage();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // With bookmark apps enabled, hosted apps should return to their launch page
  // when the home button is pressed.
  if (browser->is_type_app() || browser->is_type_app_popup()) {
    const extensions::Extension* extension = GetExtensionForBrowser(browser);
    if (!extension) {
      return;
    }
    url = extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    extensions::MaybeShowExtensionControlledHomeNotification(browser);
  }
#endif

  bool is_chrome_internal = url.SchemeIs(url::kAboutScheme) ||
                            url.SchemeIs(content::kChromeUIScheme) ||
                            url.SchemeIs(chrome::kChromeNativeScheme);
  base::UmaHistogramBoolean("Navigation.Home.IsChromeInternal",
                            is_chrome_internal);
  // Log a user action for the !is_chrome_internal case. This value is used as
  // part of a high-level guiding metric, which is being migrated to user
  // actions.
  if (!is_chrome_internal) {
    base::RecordAction(
        base::UserMetricsAction("Navigation.Home.NotChromeInternal"));
  }
  OpenURLParams params(
      url, Referrer(), disposition,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false);
  params.extra_headers = extra_headers;
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
}

base::WeakPtr<content::NavigationHandle> OpenCurrentURL(Browser* browser) {
  base::RecordAction(UserMetricsAction("LoadURL"));
  // TODO(crbug.com/40820294): Eliminate extra checks once source of
  //  bad pointer dereference is identified. See also TODO comment below.
  CHECK(browser);
  BrowserWindow* window = browser->window();
  CHECK(window);
  LocationBar* location_bar = window->GetLocationBar();
  if (!location_bar) {
    return nullptr;
  }

  GURL url(location_bar->navigation_params().destination_url);
  TRACE_EVENT1("navigation", "chrome::OpenCurrentURL", "url", url);

  if (ShouldInterceptChromeURLNavigationInIncognito(browser, url)) {
    ProcessInterceptedChromeURLNavigationInIncognito(browser, url);
    return nullptr;
  }

  NavigateParams params(browser, url,
                        location_bar->navigation_params().transition);
  params.disposition = location_bar->navigation_params().disposition;
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      AddTabTypes::ADD_FORCE_INDEX | AddTabTypes::ADD_INHERIT_OPENER;
  params.input_start =
      location_bar->navigation_params().match_selection_timestamp;
  params.is_using_https_as_default_scheme =
      location_bar->navigation_params().url_typed_without_scheme;
  params.url_typed_with_http_scheme =
      location_bar->navigation_params().url_typed_with_http_scheme;
  params.extra_headers = location_bar->navigation_params().extra_headers;
  auto result = Navigate(&params);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(extensions::ExtensionSystem::Get(browser->profile())
             ->extension_service());
  // TODO(crbug.com/40820294): Eliminate extra checks once source of
  //  bad pointer dereference is identified. See also TODO comment above.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser->profile());
  CHECK(extension_registry);
  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetAppByURL(url);
  if (extension) {
    extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_OMNIBOX_LOCATION,
                                    extension->GetType());
  }
#endif
  return result;
}

void Stop(Browser* browser) {
  base::RecordAction(UserMetricsAction("Stop"));
  browser->tab_strip_model()->GetActiveWebContents()->Stop();
}

void NewWindow(Browser* browser) {
  Profile* const profile = browser->profile();
#if BUILDFLAG(IS_MAC)
  // Web apps should open a window to their launch page.
  if (browser->app_controller()) {
    const webapps::AppId app_id = browser->app_controller()->app_id();

    auto launch_container = apps::LaunchContainer::kLaunchContainerWindow;

    auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
    if (provider && provider->registrar_unsafe().GetAppEffectiveDisplayMode(
                        app_id) == blink::mojom::DisplayMode::kBrowser) {
      launch_container = apps::LaunchContainer::kLaunchContainerTab;
    }
    apps::AppLaunchParams params = apps::AppLaunchParams(
        app_id, launch_container, WindowOpenDisposition::NEW_WINDOW,
        apps::LaunchSource::kFromKeyboard);
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params), base::DoNothing());
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Hosted apps should open a window to their launch page.
  const extensions::Extension* extension = GetExtensionForBrowser(browser);
  if (extension && extension->is_hosted_app()) {
    const auto app_launch_params = CreateAppLaunchParamsUserContainer(
        profile, extension, WindowOpenDisposition::NEW_WINDOW,
        apps::LaunchSource::kFromKeyboard);
    OpenApplicationWindow(
        profile, app_launch_params,
        extensions::AppLaunchInfo::GetLaunchWebURL(extension));
    return;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // BUILDFLAG(IS_MAC)
  NewEmptyWindow(profile->GetOriginalProfile());
}

void NewIncognitoWindow(Profile* profile) {
  NewEmptyWindow(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
}

void CloseWindow(Browser* browser) {
  base::RecordAction(UserMetricsAction("CloseWindow"));
  browser->window()->Close();
}

content::WebContents& NewTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("NewTab"));
  // TODO(asvitkine): This is invoked programmatically from several places.
  // Audit the code and change it so that the histogram only gets collected for
  // user-initiated commands.
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", NewTabTypes::NEW_TAB_COMMAND,
                            NewTabTypes::NEW_TAB_ENUM_COUNT);
  if (browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)) {
    return *AddAndReturnTabAt(browser, GURL(), -1, true);
  }

  ScopedTabbedBrowserDisplayer displayer(browser->profile());
  Browser* b = displayer.browser();
  auto* contents = AddAndReturnTabAt(b, GURL(), -1, true);
  b->window()->Show();
  // The call to AddBlankTabAt above did not set the focus to the tab as its
  // window was not active, so we have to do it explicitly.
  // See http://crbug.com/6380.
  b->tab_strip_model()->GetActiveWebContents()->RestoreFocus();

  return *contents;
}

void NewTabToRight(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandNewTabToRight);
}

void CloseTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("CloseTab_Accelerator"));
  browser->tab_strip_model()->CloseSelectedTabs();
}

bool CanZoomIn(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMaximumZoomPercent();
}

bool CanZoomOut(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMinimumZoomPercent();
}

bool CanResetZoom(content::WebContents* contents) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  return !zoom_controller->IsAtDefaultZoom() ||
         !zoom_controller->PageScaleFactorIsOne();
}

void SelectNextTab(Browser* browser,
                   TabStripUserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectNextTab"));
  browser->tab_strip_model()->SelectNextTab(gesture_detail);
}

void SelectPreviousTab(Browser* browser,
                       TabStripUserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectPrevTab"));
  browser->tab_strip_model()->SelectPreviousTab(gesture_detail);
}

void MoveTabNext(Browser* browser) {
  base::RecordAction(UserMetricsAction("MoveTabNext"));
  browser->tab_strip_model()->MoveTabNext();
}

void MoveTabPrevious(Browser* browser) {
  base::RecordAction(UserMetricsAction("MoveTabPrevious"));
  browser->tab_strip_model()->MoveTabPrevious();
}

void SelectNumberedTab(Browser* browser,
                       int index,
                       TabStripUserGestureDetails gesture_detail) {
  int visible_count = 0;
  for (int i = 0; i < browser->tab_strip_model()->count(); i++) {
    if (browser->tab_strip_model()->IsTabCollapsed(i)) {
      continue;
    }
    if (visible_count == index) {
      base::RecordAction(UserMetricsAction("SelectNumberedTab"));
      browser->tab_strip_model()->ActivateTabAt(i, gesture_detail);
      break;
    }
    visible_count += 1;
  }
}

void SelectLastTab(Browser* browser,
                   TabStripUserGestureDetails gesture_detail) {
  for (int i = browser->tab_strip_model()->count() - 1; i >= 0; i--) {
    if (!browser->tab_strip_model()->IsTabCollapsed(i)) {
      base::RecordAction(UserMetricsAction("SelectLastTab"));
      browser->tab_strip_model()->ActivateTabAt(i, gesture_detail);
      break;
    }
  }
}

void DuplicateTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateTabAt(browser, browser->tab_strip_model()->active_index());
}

bool CanDuplicateTab(const Browser* browser) {
  return CanDuplicateTabAt(browser, browser->tab_strip_model()->active_index());
}

bool CanDuplicateKeyboardFocusedTab(const Browser* browser) {
  if (!HasKeyboardFocusedTab(browser)) {
    return false;
  }
  return CanDuplicateTabAt(browser, *GetKeyboardFocusedTabIndex(browser));
}

bool CanMoveActiveTabToNewWindow(Browser* browser) {
  const ui::ListSelectionModel::SelectedIndices& selection =
      browser->tab_strip_model()->selection_model().selected_indices();
  return CanMoveTabsToNewWindow(
      browser, std::vector<int>(selection.begin(), selection.end()));
}

void MoveActiveTabToNewWindow(Browser* browser) {
  const ui::ListSelectionModel::SelectedIndices& selection =
      browser->tab_strip_model()->selection_model().selected_indices();
  MoveTabsToNewWindow(browser,
                      std::vector<int>(selection.begin(), selection.end()));
}

void ToggleCompactMode(Browser* browser) {
  const bool current_pref =
      browser->profile()->GetPrefs()->GetBoolean(prefs::kCompactModeEnabled);
  browser->profile()->GetPrefs()->SetBoolean(prefs::kCompactModeEnabled,
                                             !current_pref);
}

bool ShouldUseCompactMode(Profile* profile) {
  CHECK(profile);
  return base::FeatureList::IsEnabled(features::kCompactMode) &&
         profile->GetPrefs()->GetBoolean(prefs::kCompactModeEnabled);
}

bool CanMoveTabsToNewWindow(Browser* browser,
                            const std::vector<int>& tab_indices) {
  if (browser->is_type_app()) {
    for (int index : tab_indices) {
      if (web_app::IsPinnedHomeTab(browser->tab_strip_model(), index)) {
        return false;
      }
    }
  }
  return browser->tab_strip_model()->count() >
         static_cast<int>(tab_indices.size());
}

void MoveTabsToNewWindow(Browser* browser,
                         const std::vector<int>& tab_indices,
                         std::optional<tab_groups::TabGroupId> group) {
  if (tab_indices.empty()) {
    return;
  }

  Browser* new_browser;
  if (browser->is_type_app() && browser->app_controller()->has_tab_strip()) {
    new_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        browser->app_name(), browser->is_trusted_source(), gfx::Rect(),
        browser->profile(), true));
    web_app::MaybeAddPinnedHomeTab(new_browser,
                                   new_browser->app_controller()->app_id());
  } else {
    new_browser =
        Browser::Create(Browser::CreateParams(browser->profile(), true));
  }

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser->profile());
  std::unique_ptr<tab_groups::ScopedLocalObservationPauser> observation_pauser;

  tab_groups::TabGroupVisualData visual_data;

  if (group.has_value()) {
    const tab_groups::TabGroupVisualData* old_visual_data =
        browser->tab_strip_model()
            ->group_model()
            ->GetTabGroup(group.value())
            ->visual_data();

    visual_data = tab_groups::TabGroupVisualData(old_visual_data->title(),
                                                 old_visual_data->color(),
                                                 false /* is_collapsed */);
    if (tab_group_service && tab_group_service->GetGroup(group.value())) {
      observation_pauser = tab_group_service->CreateScopedLocalObserverPauser();
    }
  }

  int indices_size = tab_indices.size();
  int active_index = browser->tab_strip_model()->active_index();
  for (int i = 0; i < indices_size; i++) {
    // Adjust tab index to account for tabs already moved.
    int adjusted_index = tab_indices[i] - i;
    bool pinned = browser->tab_strip_model()->IsTabPinned(adjusted_index);
    std::unique_ptr<tabs::TabModel> tab_model =
        browser->tab_strip_model()->DetachTabAtForInsertion(adjusted_index);

    int add_types = pinned ? AddTabTypes::ADD_PINNED : 0;
    // The last tab made active takes precedence, so activate the last active
    // tab, with a fallback for the first tab (i == 0) if the active tab isn’t
    // in the set of tabs being moved.
    if (i == 0 || tab_indices[i] == active_index) {
      add_types = add_types | AddTabTypes::ADD_ACTIVE;
    }

    new_browser->tab_strip_model()->AddTab(std::move(tab_model), -1,
                                           ui::PAGE_TRANSITION_TYPED, add_types,
                                           std::nullopt);
  }

  // Add all the tabs in the new browser to the group if it belonged in a group.
  if (group.has_value()) {
    std::vector<int> indices(new_browser->tab_strip_model()->GetTabCount());
    std::iota(indices.begin(), indices.end(), 0);
    new_browser->tab_strip_model()->AddToNewGroup(indices, group.value(),
                                                  visual_data);

    if (observation_pauser) {
      observation_pauser.reset();
    }
  }

  new_browser->window()->Show();
}

bool CanCloseTabsToRight(const Browser* browser) {
  return browser->tab_strip_model()->IsContextMenuCommandEnabled(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

bool CanCloseOtherTabs(const Browser* browser) {
  return browser->tab_strip_model()->IsContextMenuCommandEnabled(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

WebContents* DuplicateTabAt(Browser* browser, int index) {
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  CHECK(contents);
  std::unique_ptr<WebContents> contents_dupe = contents->Clone();
  WebContents* raw_contents_dupe = contents_dupe.get();

  bool pinned = false;
  if (browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    const int contents_index = tab_strip_model->GetIndexOfWebContents(contents);
    pinned = tab_strip_model->IsTabPinned(contents_index);
    int add_types = AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER |
                    (pinned ? AddTabTypes::ADD_PINNED : 0);
    const auto old_group = tab_strip_model->GetTabGroupForTab(contents_index);
    tab_strip_model->InsertWebContentsAt(
        contents_index + 1, std::move(contents_dupe), add_types, old_group);
  } else {
    CreateAndShowNewWindowWithContents(std::move(contents_dupe), browser);
  }

  SessionServiceBase* session_service =
      GetAppropriateSessionServiceIfExisting(browser);
  if (session_service) {
    session_service->TabRestored(raw_contents_dupe, pinned);
  }
  return raw_contents_dupe;
}

bool CanDuplicateTabAt(const Browser* browser, int index) {
  if (browser->is_type_picture_in_picture()) {
    return false;
  }
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  return contents;
}

void MoveTabsToExistingWindow(Browser* source,
                              Browser* target,
                              const std::vector<int>& tab_indices) {
  if (tab_indices.empty()) {
    return;
  }

  int indices_size = tab_indices.size();
  for (int i = 0; i < indices_size; i++) {
    // Adjust tab index to account for tabs already moved.
    int adjusted_index = tab_indices[i] - i;
    bool pinned = source->tab_strip_model()->IsTabPinned(adjusted_index);
    std::unique_ptr<tabs::TabModel> tab_model =
        source->tab_strip_model()->DetachTabAtForInsertion(adjusted_index);
    int add_types =
        AddTabTypes::ADD_ACTIVE | (pinned ? AddTabTypes::ADD_PINNED : 0);
    target->tab_strip_model()->AddTab(std::move(tab_model), -1,
                                      ui::PAGE_TRANSITION_TYPED, add_types);
  }
  target->window()->Show();
}

void PinTab(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void GroupTab(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleGrouped);
}

void CreateNewTabGroup(Browser* browser) {
  NewTab(browser);
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandAddToNewGroup);
}

void MuteSite(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void MuteSiteForKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser)) {
    return;
  }
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void PinKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser)) {
    return;
  }
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void GroupKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser)) {
    return;
  }
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandToggleGrouped);
}

void DuplicateKeyboardFocusedTab(Browser* browser) {
  if (HasKeyboardFocusedTab(browser)) {
    DuplicateTabAt(browser, *GetKeyboardFocusedTabIndex(browser));
  }
}

bool HasKeyboardFocusedTab(const Browser* browser) {
  return GetKeyboardFocusedTabIndex(browser).has_value();
}

void ConvertPopupToTabbedBrowser(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowAsTab"));
  TabStripModel* tab_strip = browser->tab_strip_model();
  // If this popup is the last browser object, removing it from the browser-list
  // will trigger OnShutdownStarting for Window close. Create the new browser
  // object first, before removing the existing object from the browser-list in
  // order to avoid incorrectly triggering a shutdown.
  Browser* b = Browser::Create(Browser::CreateParams(browser->profile(), true));
  std::unique_ptr<tabs::TabModel> tab_model =
      tab_strip->DetachTabAtForInsertion(tab_strip->active_index());
  // This method moves a WebContents from a non-normal browser window to a
  // normal browser window. We cannot move the Tab over directly since TabModel
  // enforces the requirement that it cannot move between window types.
  // https://crbug.com/334281979): Non-normal browser windows should not have a
  // tab to begin with.
  std::unique_ptr<content::WebContents> contents_move =
      tabs::TabModel::DestroyAndTakeWebContents(std::move(tab_model));

  // This method moves a WebContents from a non-normal browser window to a
  // normal browser window. We cannot move the Tab over directly since TabModel
  // enforces the requirement that it cannot move between window types.
  // https://crbug.com/334281979): Non-normal browser windows should not have a
  // tab to begin with.
  b->tab_strip_model()->AppendWebContents(std::move(contents_move), true);
  b->window()->Show();
}

void CloseTabsToRight(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

void CloseOtherTabs(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

void Exit() {
  base::RecordAction(UserMetricsAction("Exit"));
  chrome::AttemptUserExit();
}

void BookmarkCurrentTab(Browser* browser) {
  base::RecordAction(base::UserMetricsAction("Star"));
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  GURL url;
  std::u16string title;
  if (!BookmarkCurrentTabHelper(browser, model, &url, &title)) {
    return;
  }
  bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
#if !BUILDFLAG(IS_ANDROID)
  PrefService* prefs = browser->profile()->GetPrefs();
  if (!prefs->GetBoolean(
          bookmarks::prefs::kAddedBookmarkSincePowerBookmarksLaunch)) {
    bookmarks::AddIfNotBookmarked(model, url, title, model->other_node());
    prefs->SetBoolean(bookmarks::prefs::kAddedBookmarkSincePowerBookmarksLaunch,
                      true);
  }
#endif
  bookmarks::AddIfNotBookmarked(model, url, title);
  bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (browser->window()->IsActive() && is_bookmarked_by_user) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations where the bubble is deleted as soon as it is shown.
    browser->window()->ShowBookmarkBubble(url, was_bookmarked_by_user);
  }

  if (!was_bookmarked_by_user && is_bookmarked_by_user) {
    RecordBookmarksAdded(browser->profile());
  }
}

void BookmarkCurrentTabInFolder(Browser* browser, int64_t folder_id) {
  BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  GURL url;
  std::u16string title;
  if (!BookmarkCurrentTabHelper(browser, model, &url, &title)) {
    return;
  }
  const bookmarks::BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, folder_id);
  if (parent) {
    bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
    model->AddNewURL(parent, 0, title, url);
    bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
    if (!was_bookmarked_by_user && is_bookmarked_by_user) {
      RecordBookmarksAdded(browser->profile());
    }
  }
}

bool CanBookmarkCurrentTab(const Browser* browser) {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  return browser_defaults::bookmarks_enabled &&
         browser->profile()->GetPrefs()->GetBoolean(
             bookmarks::prefs::kEditBookmarksEnabled) &&
         model && model->loaded() && browser->is_type_normal();
}

void BookmarkAllTabs(Browser* browser) {
  base::RecordAction(UserMetricsAction("BookmarkAllTabs"));
  RecordBookmarkAllTabsWithTabsCount(browser->profile(),
                                     browser->tab_strip_model()->count());

  chrome::ShowBookmarkAllTabsDialog(browser);
}

bool CanBookmarkAllTabs(const Browser* browser) {
  return browser->tab_strip_model()->count() > 1 &&
         CanBookmarkCurrentTab(browser);
}

bool CanMoveActiveTabToReadLater(Browser* browser) {
  GURL url;
  std::u16string title;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ReadingListModel* model = GetReadingListModel(browser);
  return CanMoveWebContentsToReadLater(browser, web_contents, model, &url,
                                       &title);
}

bool MoveCurrentTabToReadLater(Browser* browser) {
  return MoveTabToReadLater(browser,
                            browser->tab_strip_model()->GetActiveWebContents());
}

bool MoveTabToReadLater(Browser* browser, content::WebContents* web_contents) {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser);
  if (!CanMoveWebContentsToReadLater(browser, web_contents, model, &url,
                                     &title)) {
    return false;
  }
  model->AddOrReplaceEntry(url, base::UTF16ToUTF8(title),
                           reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
                           /*estimated_read_time=*/base::TimeDelta());
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHReadingListDiscoveryFeature);
  base::UmaHistogramEnumeration(
      "ReadingList.BookmarkBarState.OnEveryAddToReadingList",
      browser->bookmark_bar_state());
#if !BUILDFLAG(IS_ANDROID)
  if (toast_features::IsEnabled(toast_features::kReadingListToast)) {
    // Don't show the reading list toast if the side panel is visible.
    std::optional<SidePanelEntry::Id> id =
        browser->GetFeatures().side_panel_ui()->GetCurrentEntryId();
    if (id.has_value() && id.value() == SidePanelEntryId::kReadingList) {
      return true;
    }

    ToastController* const toast_controller =
        browser->GetFeatures().toast_controller();
    if (toast_controller) {
      toast_controller->MaybeShowToast(
          ToastParams(ToastId::kAddedToReadingList));
    }
  }
#endif
  return true;
}

bool MarkCurrentTabAsReadInReadLater(Browser* browser) {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser);
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!model || !GetTabURLAndTitleToSave(web_contents, &url, &title)) {
    return false;
  }
  scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
  // Mark current tab as read.
  if (entry && !entry->IsRead()) {
    model->SetReadStatusIfExists(url, true);
  }
  return entry != nullptr;
}

bool IsCurrentTabUnreadInReadLater(Browser* browser) {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser);
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!model || !GetTabURLAndTitleToSave(web_contents, &url, &title)) {
    return false;
  }
  scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
  return entry && !entry->IsRead();
}

void ShowOffersAndRewardsForPage(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::OfferNotificationBubbleControllerImpl* controller =
      autofill::OfferNotificationBubbleControllerImpl::FromWebContents(
          web_contents);
  DCHECK(controller);
  controller->ReshowBubble();
}

void SaveCreditCard(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble(/*is_user_gesture=*/true);
}

void SaveIban(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::IbanBubbleControllerImpl* controller =
      autofill::IbanBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble();
}

void ShowMandatoryReauthOptInPrompt(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::MandatoryReauthBubbleControllerImpl* controller =
      autofill::MandatoryReauthBubbleControllerImpl::FromWebContents(
          web_contents);
  controller->ReshowBubble();
}

void MigrateLocalCards(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::ManageMigrationUiController* controller =
      autofill::ManageMigrationUiController::FromWebContents(web_contents);
  // Show migration-related Ui when the user clicks the credit card icon.
  controller->OnUserClickedCreditCardIcon();
}

void SaveAutofillAddress(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::AddressBubblesController* controller =
      autofill::AddressBubblesController::FromWebContents(web_contents);
  controller->OnIconClicked();
}

void ShowVirtualCardManualFallbackBubble(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* controller =
      autofill::VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
          web_contents);
  if (controller) {
    controller->ReshowBubble();
  }
}

void ShowVirtualCardEnrollBubble(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::VirtualCardEnrollBubbleControllerImpl* controller =
      autofill::VirtualCardEnrollBubbleControllerImpl::FromWebContents(
          web_contents);
  if (controller) {
    controller->ReshowBubble();
  }
}

void StartTabOrganizationRequest(Browser* browser) {
  TabOrganizationService* service =
      TabOrganizationServiceFactory::GetForProfile(browser->profile());
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.ThreeDotMenu.Clicked", true);
  browser->window()->NotifyNewBadgeFeatureUsed(features::kTabOrganization);

  service->RestartSessionAndShowUI(browser,
                                   TabOrganizationEntryPoint::kThreeDotMenu);
}

void ShowTranslateBubble(Browser* browser) {
  if (!browser->window()->IsActive()) {
    return;
  }

  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);

  if (!chrome_translate_client) {
    return;
  }

  // The Translate bubble will not show if a text field is focused, so we clear
  // focus here as the user has intentionally opened the bubble.
  web_contents->ClearFocusedElement();

  std::string source_language;
  std::string target_language;
  chrome_translate_client->GetTranslateLanguages(web_contents, &source_language,
                                                 &target_language);

  // If the source language matches the target language, we change the source
  // language to unknown, so that we display "Detected Language".
  if (source_language == target_language) {
    source_language = translate::kUnknownLanguageCode;
  }

  translate::TranslateStep step = translate::TRANSLATE_STEP_BEFORE_TRANSLATE;
  auto* language_state =
      chrome_translate_client->GetTranslateManager()->GetLanguageState();

  if (language_state->translation_pending()) {
    step = translate::TRANSLATE_STEP_TRANSLATING;
  } else if (language_state->translation_error()) {
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  } else if (language_state->IsPageTranslated()) {
    step = translate::TRANSLATE_STEP_AFTER_TRANSLATE;
  }
  browser->window()->ShowTranslateBubble(
      web_contents, step, source_language, target_language,
      translate::TranslateErrors::NONE, true);
}

void ManagePasswordsForPage(Browser* browser) {
  browser->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  browser->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  browser->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordManagerShortcutFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  TabDialogs::FromWebContents(web_contents)
      ->ShowManagePasswordsBubble(!controller->IsAutomaticallyOpeningBubble());
}

bool CanSendTabToSelf(const Browser* browser) {
  return send_tab_to_self::ShouldDisplayEntryPoint(
      browser->tab_strip_model()->GetActiveWebContents());
}

void SendTabToSelf(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  send_tab_to_self::ShowBubble(web_contents);
}

bool CanGenerateQrCode(const Browser* browser) {
  return !sharing_hub::SharingIsDisabledByPolicy(browser->profile()) &&
         qrcode_generator::QRCodeGeneratorBubbleController::
             IsGeneratorAvailable(browser->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetController()
                                      .GetLastCommittedEntry()
                                      ->GetURL());
}

void GenerateQRCode(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  qrcode_generator::QRCodeGeneratorBubbleController* controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  controller->ShowBubble(entry->GetURL());
}

void SharingHub(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  sharing_hub::SharingHubBubbleController* controller =
      sharing_hub::SharingHubBubbleController::CreateOrGetFromWebContents(
          web_contents);
  controller->ShowBubble(share::ShareAttempt(web_contents));
}

void ScreenshotCapture(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  sharing_hub::ScreenshotCapturedBubbleController* controller =
      sharing_hub::ScreenshotCapturedBubbleController::Get(web_contents);
  controller->Capture(browser);
}

void SavePage(Browser* browser) {
  base::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(current_tab);
  if (current_tab->GetContentsMimeType() == "application/pdf") {
    base::RecordAction(UserMetricsAction("PDF.SavePage"));
#if BUILDFLAG(ENABLE_PDF)
    // The PDF viewer may handle the event by itself.
    if (chrome_pdf::features::IsOopifPdfEnabled() &&
        pdf_extension_util::MaybeDispatchSaveEvent(
            current_tab->GetPrimaryMainFrame())) {
      return;
    }
#endif  // BUILDFLAG(ENABLE_PDF)
  }
  current_tab->OnSavePage();
}

bool CanSavePage(const Browser* browser) {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(
          prefs::kAllowFileSelectionDialogs)) {
    return false;
  }
  if (static_cast<DownloadPrefs::DownloadRestriction>(
          browser->profile()->GetPrefs()->GetInteger(
              prefs::kDownloadRestrictions)) ==
      DownloadPrefs::DownloadRestriction::ALL_FILES) {
    return false;
  }
  return !browser->is_type_devtools() &&
         !(GetContentRestrictions(browser) & CONTENT_RESTRICTION_SAVE);
}

void Print(Browser* browser) {
#if BUILDFLAG(ENABLE_PRINTING)
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();

  // Launch ChromeOS print preview only if in a ChromeOS build and
  // `kPrintPreviewCrosPrimary` enabled. Otherwise use browser print preview.
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(::features::kPrintPreviewCrosPrimary)) {
    chromeos::printing::StartPrint(
        web_contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        /*print_renderer=*/mojo::NullAssociatedRemote(),
#endif
        browser->profile()->GetPrefs()->GetBoolean(
            prefs::kPrintPreviewDisabled),
        /*has_selection=*/false);
    return;
  }
#endif

  printing::StartPrint(
      web_contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      /*print_renderer=*/mojo::NullAssociatedRemote(),
#endif
      browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled),
      /*has_selection=*/false);
#endif
}

bool CanPrint(Browser* browser) {
#if BUILDFLAG(ENABLE_PRINTING)
  // Do not print when printing is disabled via pref or policy.
  // Do not print when a page has crashed.
  // Do not print when a constrained window is showing. It's confusing.
  // TODO(gbillock): Need to re-assess the call to
  // IsShowingWebContentsModalDialog after a popup management policy is
  // refined -- we will probably want to just queue the print request, not
  // block it.
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
         (current_tab && !current_tab->IsCrashed()) &&
         !(IsShowingWebContentsModalDialog(browser) ||
           GetContentRestrictions(browser) & CONTENT_RESTRICTION_PRINT);
#else   // BUILDFLAG(ENABLE_PRINTING)
  return false;
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(Browser* browser) {
  printing::StartBasicPrint(browser->tab_strip_model()->GetActiveWebContents());
}

bool CanBasicPrint(Browser* browser) {
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // If printing is not disabled via pref or policy, it is always possible to
  // advanced print when the print preview is visible.
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
         (PrintPreviewShowing(browser) || CanPrint(browser));
#else
  return false;  // The print dialog is disabled.
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

bool CanRouteMedia(Browser* browser) {
  // Do not allow user to open Media Router dialog when there is already an
  // active modal dialog. This avoids overlapping dialogs.
  return media_router::MediaRouterEnabled(browser->profile()) &&
         !IsShowingWebContentsModalDialog(browser);
}

void RouteMediaInvokedFromAppMenu(Browser* browser) {
  DCHECK(CanRouteMedia(browser));

  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  if (!dialog_controller) {
    return;
  }

  dialog_controller->ShowMediaRouterDialog(
      media_router::MediaRouterDialogActivationLocation::APP_MENU);
}

void Find(Browser* browser) {
  base::RecordAction(UserMetricsAction("Find"));
  FindInPage(browser, false, true);
}

void FindNext(Browser* browser) {
  base::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(browser, true, true);
}

void FindPrevious(Browser* browser) {
  base::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(browser, true, false);
}

void FindInPage(Browser* browser, bool find_next, bool forward_direction) {
  browser->GetFindBarController()->Show(find_next, forward_direction);
}

void ShowTabSearch(Browser* browser) {
  const int tab_search_tab_index = 0;
  browser->window()->CreateTabSearchBubble(
      tab_search_tab_index, tab_search::mojom::TabOrganizationFeature::kNone);
}

void CloseTabSearch(Browser* browser) {
  browser->window()->CloseTabSearchBubble();
}

void ShowTabDeclutter(Browser* browser) {
  const int tab_organization_tab_index = 1;
  browser->window()->CreateTabSearchBubble(
      tab_organization_tab_index,
      tab_search::mojom::TabOrganizationFeature::kDeclutter);
}

bool CanCloseFind(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab) {
    return false;
  }

  find_in_page::FindTabHelper* find_helper =
      find_in_page::FindTabHelper::FromWebContents(current_tab);
  return find_helper ? find_helper->find_ui_active() : false;
}

void CloseFind(Browser* browser) {
  browser->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
}

void Zoom(Browser* browser, content::PageZoom zoom) {
  zoom::PageZoom::Zoom(browser->tab_strip_model()->GetActiveWebContents(),
                       zoom);
}

void FocusToolbar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusToolbar"));
  browser->window()->FocusToolbar();
}

void FocusLocationBar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusLocation"));
  browser->window()->SetFocusToLocationBar(true);
}

void FocusSearch(Browser* browser) {
  // TODO(beng): replace this with FocusLocationBar
  base::RecordAction(UserMetricsAction("FocusSearch"));
  browser->window()->GetLocationBar()->FocusSearch();
}

void FocusAppMenu(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusAppMenu"));
  browser->window()->FocusAppMenu();
}

void FocusBookmarksToolbar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  browser->window()->FocusBookmarksToolbar();
}

void FocusInactivePopupForAccessibility(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusInactivePopupForAccessibility"));
  browser->window()->FocusInactivePopupForAccessibility();
}

void FocusNextPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusNextPane"));
  browser->window()->RotatePaneFocus(true);
}

void FocusPreviousPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusPreviousPane"));
  browser->window()->RotatePaneFocus(false);
}

void FocusWebContentsPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusWebContentsPane"));
  browser->window()->FocusWebContentsPane();
}

void ToggleDevToolsWindow(Browser* browser,
                          DevToolsToggleAction action,
                          DevToolsOpenedByAction opened_by) {
  if (action.type() == DevToolsToggleAction::kShowConsolePanel) {
    base::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  } else {
    base::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));
  }
  DevToolsWindow::ToggleDevToolsWindow(browser, action, opened_by);
}

bool CanOpenTaskManager() {
#if !BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

void OpenTaskManager(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Open linux version of task manager UI if ash TaskManager
  // interface is in an old version.
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::TaskManager>() < 1) {
    base::RecordAction(UserMetricsAction("TaskManager"));
    chrome::ShowTaskManager(browser);
    return;
  }
  // Invoke task manager UI in ash, which will call chrome::OpenTaskManager()
  // in ash to run through the code path in the next section
  // (!BUILDFLAG(IS_ANDROID)).
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TaskManager>()
      ->ShowTaskManager();
#elif !BUILDFLAG(IS_ANDROID)
  base::RecordAction(UserMetricsAction("TaskManager"));
  chrome::ShowTaskManager(browser);
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void OpenFeedbackDialog(Browser* browser,
                        feedback::FeedbackSource source,
                        const std::string& description_template,
                        const std::string& category_tag) {
  base::RecordAction(UserMetricsAction("Feedback"));
  chrome::ShowFeedbackPage(browser, source, description_template,
                           std::string() /* description_placeholder_text */,
                           category_tag, std::string() /* extra_diagnostics */);
}

void ToggleBookmarkBar(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  ToggleBookmarkBarWhenVisible(browser->profile());
}

void ToggleShowFullURLs(Browser* browser) {
  bool pref_enabled = browser->profile()->GetPrefs()->GetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox);
  browser->profile()->GetPrefs()->SetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox, !pref_enabled);
}

void ToggleShowGoogleLensShortcut(Browser* browser) {
  bool pref_enabled = browser->profile()->GetPrefs()->GetBoolean(
      omnibox::kShowGoogleLensShortcut);
  browser->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                             !pref_enabled);
}

void ShowAppMenu(Browser* browser) {
  // We record the user metric for this event in AppMenu::RunMenu.
  browser->window()->ShowAppMenu();
}

void ShowAvatarMenu(Browser* browser) {
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      /*is_source_accelerator=*/true);
}

// TODO(crbug.com/345770406): Rename the function name.
// We removed the extra confirmation step in the Chrome update flow. After the
// full rollout of the code, this name will be misleading. We will clean up the
// code and its related source enums.
void OpenUpdateChromeDialog(Browser* browser) {
  if (UpgradeDetector::GetInstance()->is_outdated_install()) {
    UpgradeDetector::GetInstance()->NotifyOutdatedInstall();
  } else if (UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    UpgradeDetector::GetInstance()->NotifyOutdatedInstallNoAutoUpdate();
  } else {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    if (base::FeatureList::IsEnabled(features::kFewerUpdateConfirmations)) {
      chrome::AttemptRelaunch();
      return;
    }
#endif
    base::RecordAction(UserMetricsAction("UpdateChrome"));
    browser->window()->ShowUpdateChromeDialog();
  }
}

bool CanRequestTabletSite(WebContents* current_tab) {
  return current_tab &&
         current_tab->GetController().GetLastCommittedEntry() != nullptr;
}

bool IsRequestingTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab) {
    return false;
  }
  content::NavigationEntry* entry =
      current_tab->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }
  return entry->GetIsOverridingUserAgent();
}

void ToggleRequestTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab) {
    return;
  }
  NavigationController& controller = current_tab->GetController();
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  if (!entry) {
    return;
  }
  if (entry->GetIsOverridingUserAgent()) {
    entry->SetIsOverridingUserAgent(false);
  } else {
    SetAndroidOsForTabletSite(current_tab);
  }
  controller.LoadOriginalRequestURL();
}

void SetAndroidOsForTabletSite(content::WebContents* current_tab) {
  DCHECK(current_tab);
  NavigationEntry* entry = current_tab->GetController().GetLastCommittedEntry();
  if (entry) {
    entry->SetIsOverridingUserAgent(true);
    std::string product = embedder_support::GetProductAndVersion() + " Mobile";
    blink::UserAgentOverride ua_override;
    ua_override.ua_string_override = content::BuildUserAgentFromOSAndProduct(
        kOsOverrideForTabletSite, product);
    ua_override.ua_metadata_override = embedder_support::GetUserAgentMetadata(
        g_browser_process->local_state());
    ua_override.ua_metadata_override->mobile = true;
    ua_override.ua_metadata_override->form_factors = {blink::kTabletFormFactor};
    ua_override.ua_metadata_override->platform =
        kChPlatformOverrideForTabletSite;
    ua_override.ua_metadata_override->platform_version = std::string();
    current_tab->SetUserAgentOverride(ua_override, false);
  }
}

void ToggleFullscreenMode(Browser* browser) {
  DCHECK(browser);
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
}

void ClearCache(Browser* browser) {
  content::BrowsingDataRemover* remover =
      browser->profile()->GetBrowsingDataRemover();
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

bool IsDebuggerAttachedToCurrentTab(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  return contents ? content::DevToolsAgentHost::IsDebuggerAttached(contents)
                  : false;
}

void CopyURL(Browser* browser, content::WebContents* web_contents) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(web_contents->GetVisibleURL().spec()));

#if !BUILDFLAG(IS_ANDROID)
  if (toast_features::IsEnabled(toast_features::kLinkCopiedToast)) {
    ToastController* const toast_controller =
        browser->GetFeatures().toast_controller();
    if (toast_controller) {
      toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied));
    }
  }
#endif
}

bool CanCopyUrl(const Browser* browser) {
  return IsWebAppOrCustomTab(browser) ||
         !sharing_hub::SharingIsDisabledByPolicy(browser->profile());
}

bool IsWebAppOrCustomTab(const Browser* browser) {
  return
#if BUILDFLAG(IS_CHROMEOS_ASH)
      browser->is_type_custom_tab() ||
#endif
      web_app::AppBrowserController::IsWebApp(browser);
}

Browser* OpenInChrome(Browser* hosted_app_browser) {
  // Find a non-incognito browser.
  Browser* target_browser =
      chrome::FindTabbedBrowser(hosted_app_browser->profile(), false);

  if (!target_browser) {
    target_browser = Browser::Create(
        Browser::CreateParams(hosted_app_browser->profile(), true));
  }

  web_app::ReparentWebContentsIntoBrowserImpl(
      hosted_app_browser,
      hosted_app_browser->tab_strip_model()->GetActiveWebContents(),
      target_browser);
  return target_browser;
}

bool CanViewSource(const Browser* browser) {
  if (browser->is_type_devtools()) {
    return false;
  }

  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Disallow ViewSource if DevTools are disabled.
  if (!DevToolsWindow::AllowDevToolsFor(browser->profile(), web_contents)) {
    return false;
  }
  return web_contents->GetController().CanViewSource();
}

bool CanToggleCaretBrowsing(Browser* browser) {
#if BUILDFLAG(IS_MAC)
  // On Mac, ignore the keyboard shortcut unless web contents is focused,
  // because the keyboard shortcut interferes with a Japenese IME when the
  // omnibox is focused.  See https://crbug.com/1138475
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  return rwhv && rwhv->HasFocus();
#else
  return true;
#endif  // BUILDFLAG(IS_MAC)
}

void ToggleCaretBrowsing(Browser* browser) {
  if (!CanToggleCaretBrowsing(browser)) {
    return;
  }

  PrefService* prefService = browser->profile()->GetPrefs();
  bool enabled = prefService->GetBoolean(prefs::kCaretBrowsingEnabled);

  if (enabled) {
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CaretBrowsing.DisableWithKeyboard"));
    prefService->SetBoolean(prefs::kCaretBrowsingEnabled, false);
    return;
  }

  // Show a confirmation dialog, unless either (1) the command-line
  // flag was used, or (2) the user previously checked the box
  // indicating not to ask them next time.
  if (prefService->GetBoolean(prefs::kShowCaretBrowsingDialog) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCaretBrowsing)) {
    browser->window()->ShowCaretBrowsingDialog();
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CaretBrowsing.EnableWithKeyboard"));
    prefService->SetBoolean(prefs::kCaretBrowsingEnabled, true);
  }
}

void PromptToNameWindow(Browser* browser) {
  chrome::ShowWindowNamePrompt(browser);
}

#if BUILDFLAG(IS_CHROMEOS)
void ToggleMultitaskMenu(Browser* browser) {
  browser->window()->ToggleMultitaskMenu();
}
#endif

#if !defined(TOOLKIT_VIEWS)
std::optional<int> GetKeyboardFocusedTabIndex(const Browser* browser) {
  return std::nullopt;
}
#endif

void ShowIncognitoClearBrowsingDataDialog(Browser* browser) {
  browser->window()->ShowIncognitoClearBrowsingDataDialog();
}

void ShowIncognitoHistoryDisclaimerDialog(Browser* browser) {
  browser->window()->ShowIncognitoHistoryDisclaimerDialog();
}

bool ShouldInterceptChromeURLNavigationInIncognito(Browser* browser,
                                                   const GURL& url) {
  if (!browser || !browser->profile()->IsIncognitoProfile()) {
    return false;
  }

  bool show_clear_browsing_data_dialog =
      url == GURL(chrome::kChromeUISettingsURL)
                 .Resolve(chrome::kClearBrowserDataSubPage);

  bool show_history_disclaimer_dialog =
      url == GURL(chrome::kChromeUIHistoryURL);

  return show_clear_browsing_data_dialog || show_history_disclaimer_dialog;
}

void ProcessInterceptedChromeURLNavigationInIncognito(Browser* browser,
                                                      const GURL& url) {
  if (url == GURL(chrome::kChromeUISettingsURL)
                 .Resolve(chrome::kClearBrowserDataSubPage)) {
    ShowIncognitoClearBrowsingDataDialog(browser);
  } else if (url == GURL(chrome::kChromeUIHistoryURL)) {
    ShowIncognitoHistoryDisclaimerDialog(browser);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void ExecLensOverlay(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);

  LensOverlayController* const controller =
      LensOverlayController::GetController(web_contents);
  CHECK(controller);
  controller->ShowUI(lens::LensOverlayInvocationSource::kAppMenu);
  browser->window()->NotifyNewBadgeFeatureUsed(lens::features::kLensOverlay);
}

void ExecLensRegionSearch(Browser* browser) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  Profile* profile = browser->profile();
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  GURL url = contents->GetController().GetLastCommittedEntry()->GetURL();

  if (lens::IsRegionSearchEnabled(browser, profile, service, url)) {
    const bool is_google_dsp = search::DefaultSearchProviderIsGoogle(profile);
    const lens::AmbientSearchEntryPoint entry_point =
        is_google_dsp ? lens::AmbientSearchEntryPoint::
                            CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS
                      : lens::AmbientSearchEntryPoint::
                            CONTEXT_MENU_SEARCH_REGION_WITH_WEB;
    auto lens_region_search_controller_data =
        std::make_unique<lens::LensRegionSearchControllerData>();
    lens_region_search_controller_data->lens_region_search_controller =
        std::make_unique<lens::LensRegionSearchController>();
    lens_region_search_controller_data->lens_region_search_controller->Start(
        contents, lens::features::IsLensFullscreenSearchEnabled(),
        /*force_open_in_new_tab=*/false, is_google_dsp, entry_point);
    browser->SetUserData(lens::LensRegionSearchControllerData::kDataKey,
                         std::move(lens_region_search_controller_data));
  }
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
}

void OpenCommerceProductSpecificationsTab(Browser* browser,
                                          const std::vector<GURL>& urls,
                                          const int position) {
  if (static_cast<int>(urls.size()) <
      commerce::kProductSpecificationsMinTabsCount) {
    return;
  }

  auto* prefs = browser->profile()->GetPrefs();
  // If user has not accepted the latest disclosure, show the disclosure dialog
  // first.
  if (prefs &&
      prefs->GetInteger(
          commerce::kProductSpecificationsAcceptedDisclosureVersion) !=
          static_cast<int>(shopping_service::mojom::
                               ProductSpecificationsDisclosureVersion::kV1)) {
    commerce::DialogArgs dialog_args(urls, std::string(), /*set_id=*/"",
                                     /*in_new_tab=*/true);
    commerce::ProductSpecificationsDisclosureDialog::ShowDialog(
        browser->profile(), browser->tab_strip_model()->GetActiveWebContents(),
        std::move(dialog_args));
    return;
  }

  chrome::AddTabAt(browser, commerce::GetProductSpecsTabUrl(urls), position + 1,
                   true, std::nullopt);
}

}  // namespace chrome
