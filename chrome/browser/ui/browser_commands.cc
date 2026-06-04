// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/lens/region_search/lens_region_search_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/focus/browser_focus_controller.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/sharing_hub/screenshot/screenshot_captured_bubble_controller.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/new_tab_grouping_user_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/google/core/common/google_util.h"
#include "components/language_detection/core/constants.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/services/app_service/public/cpp/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
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
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/extensions/launch.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

class BrowserWindowInterface;
class CommandObserver;
class GURL;
class Profile;
enum class DevToolsOpenedByAction;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace bookmarks {
class BookmarkModel;
}

namespace split_tabs {
enum class SplitTabCreatedSource;
enum class SplitTabLayout;
}  // namespace split_tabs

namespace chrome {

bool IsCommandEnabled(BrowserWindowInterface* browser, int command) {
  return BrowserCommands::From(browser)->IsCommandEnabled(command);
}

bool SupportsCommand(BrowserWindowInterface* browser, int command) {
  return BrowserCommands::From(browser)->SupportsCommand(command);
}

bool ExecuteCommand(BrowserWindowInterface* browser,
                    int command,
                    base::TimeTicks time_stamp) {
  return BrowserCommands::From(browser)->ExecuteCommand(command, time_stamp);
}

bool ExecuteCommandWithDisposition(BrowserWindowInterface* browser,
                                   int command,
                                   WindowOpenDisposition disposition) {
  return BrowserCommands::From(browser)->ExecuteCommandWithDisposition(
      command, disposition);
}

void UpdateCommandEnabled(BrowserWindowInterface* browser,
                          int command,
                          bool enabled) {
  BrowserCommands::From(browser)->UpdateCommandEnabled(command, enabled);
}

void AddCommandObserver(BrowserWindowInterface* browser,
                        int command,
                        CommandObserver* observer) {
  BrowserCommands::From(browser)->AddCommandObserver(command, observer);
}

void RemoveCommandObserver(BrowserWindowInterface* browser,
                           int command,
                           CommandObserver* observer) {
  BrowserCommands::From(browser)->RemoveCommandObserver(command, observer);
}

int GetContentRestrictions(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->GetContentRestrictions();
}

void NewEmptyWindow(Profile* profile, bool should_trigger_session_restore) {
  BrowserCommands::NewEmptyWindow(profile, should_trigger_session_restore);
}

BrowserWindowInterface* OpenEmptyWindow(Profile* profile,
                                        bool should_trigger_session_restore) {
  return BrowserCommands::OpenEmptyWindow(profile,
                                          should_trigger_session_restore);
}

void OpenWindowWithRestoredTabs(Profile* profile) {
  BrowserCommands::OpenWindowWithRestoredTabs(profile);
}

void OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  BrowserCommands::OpenURLOffTheRecord(profile, url);
}

bool CanGoBack(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanGoBack();
}

bool CanGoBack(content::WebContents* web_contents) {
  return BrowserCommands::CanGoBack(web_contents);
}

bool ShouldEnableBackButton(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->ShouldEnableBackButton();
}

void GoBack(BrowserWindowInterface* browser,
            WindowOpenDisposition disposition) {
  BrowserCommands::From(browser)->GoBack(disposition);
}

void GoBack(content::WebContents* web_contents) {
  BrowserCommands::GoBack(web_contents);
}

bool CanGoForward(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanGoForward();
}

bool CanGoForward(content::WebContents* web_contents) {
  return BrowserCommands::CanGoForward(web_contents);
}

bool ShouldEnableForwardButton(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->ShouldEnableForwardButton();
}

void GoForward(BrowserWindowInterface* browser,
               WindowOpenDisposition disposition) {
  BrowserCommands::From(browser)->GoForward(disposition);
}

void GoForward(content::WebContents* web_contents) {
  BrowserCommands::GoForward(web_contents);
}

void NavigateToIndexWithDisposition(BrowserWindowInterface* browser,
                                    int index,
                                    WindowOpenDisposition disposition) {
  BrowserCommands::From(browser)->NavigateToIndexWithDisposition(index,
                                                                 disposition);
}

void Reload(BrowserWindowInterface* browser,
            WindowOpenDisposition disposition) {
  BrowserCommands::From(browser)->Reload(disposition);
}

void ReloadBypassingCache(BrowserWindowInterface* browser,
                          WindowOpenDisposition disposition) {
  BrowserCommands::From(browser)->ReloadBypassingCache(disposition);
}

bool CanReload(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanReload();
}

void Home(BrowserWindowInterface* browser, WindowOpenDisposition disposition) {
  BrowserCommands::From(browser)->Home(disposition);
}

base::WeakPtr<content::NavigationHandle> OpenCurrentURL(
    BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->OpenCurrentURL();
}

void Stop(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->Stop();
}

void NewWindow(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->NewWindow();
}

void NewIncognitoWindow(Profile* profile) {
  BrowserCommands::NewIncognitoWindow(profile);
}

void CloseWindow(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseWindow();
}

content::WebContents& NewTab(BrowserWindowInterface* browser,
                             NewTabTypes context) {
  return BrowserCommands::From(browser)->NewTab(context);
}

void NewTabToRight(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->NewTabToRight();
}

void NewTabFromClipboardURL(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->NewTabFromClipboardURL();
}

void CloseTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseTab();
}

bool CanZoomIn(content::WebContents* contents) {
  return BrowserCommands::CanZoomIn(contents);
}

bool CanZoomOut(content::WebContents* contents) {
  return BrowserCommands::CanZoomOut(contents);
}

bool CanResetZoom(content::WebContents* contents) {
  return BrowserCommands::CanResetZoom(contents);
}

void SelectNextTab(BrowserWindowInterface* browser,
                   TabStripUserGestureDetails gesture_detail) {
  BrowserCommands::From(browser)->SelectNextTab(gesture_detail);
}

void SelectPreviousTab(BrowserWindowInterface* browser,
                       TabStripUserGestureDetails gesture_detail) {
  BrowserCommands::From(browser)->SelectPreviousTab(gesture_detail);
}

void MoveTabNext(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->MoveTabNext();
}

void MoveTabPrevious(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->MoveTabPrevious();
}

void SelectNumberedTab(BrowserWindowInterface* browser,
                       int index,
                       TabStripUserGestureDetails gesture_detail) {
  BrowserCommands::From(browser)->SelectNumberedTab(index, gesture_detail);
}

void SelectLastTab(BrowserWindowInterface* browser,
                   TabStripUserGestureDetails gesture_detail) {
  BrowserCommands::From(browser)->SelectLastTab(gesture_detail);
}

void DuplicateTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->DuplicateTab();
}

bool CanDuplicateTab(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanDuplicateTab();
}

bool CanDuplicateKeyboardFocusedTab(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanDuplicateKeyboardFocusedTab();
}

bool CanMoveActiveTabToNewWindow(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanMoveActiveTabToNewWindow();
}

void MoveActiveTabToNewWindow(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->MoveActiveTabToNewWindow();
}

bool CanMoveTabsToNewWindow(BrowserWindowInterface* browser,
                            const std::vector<int>& tab_indices) {
  return BrowserCommands::From(browser)->CanMoveTabsToNewWindow(tab_indices);
}

void MoveGroupToNewWindow(BrowserWindowInterface* browser,
                          tab_groups::TabGroupId group) {
  BrowserCommands::From(browser)->MoveGroupToNewWindow(group);
}

void MoveTabsToNewWindow(BrowserWindowInterface* browser,
                         const std::vector<int>& tab_indices) {
  BrowserCommands::From(browser)->MoveTabsToNewWindow(tab_indices);
}

bool CanCloseTabsToRight(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanCloseTabsToRight();
}

bool CanCloseOtherTabs(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanCloseOtherTabs();
}

content::WebContents* DuplicateTabAt(BrowserWindowInterface* browser,
                                     int index) {
  return BrowserCommands::From(browser)->DuplicateTabAt(index);
}

void DuplicateSplit(BrowserWindowInterface* browser,
                    split_tabs::SplitTabId split) {
  BrowserCommands::From(browser)->DuplicateSplit(split);
}

bool CanDuplicateTabAt(const BrowserWindowInterface* browser, int index) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanDuplicateTabAt(index);
}

void MoveTabsToExistingWindow(BrowserWindowInterface* source,
                              BrowserWindowInterface* target,
                              const std::vector<int>& tab_indices) {
  BrowserCommands::From(source)->MoveTabsToExistingWindow(target, tab_indices);
}

void MoveGroupToExistingWindow(BrowserWindowInterface* source,
                               BrowserWindowInterface* target,
                               tab_groups::TabGroupId group) {
  BrowserCommands::From(source)->MoveGroupToExistingWindow(target, group);
}

void PinTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->PinTab();
}

void GroupTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->GroupTab();
}

void NewSplitTab(BrowserWindowInterface* browser,
                 split_tabs::SplitTabLayout layout,
                 split_tabs::SplitTabCreatedSource source) {
  BrowserCommands::From(browser)->NewSplitTab(layout, source);
}

void AddNewTabToGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->AddNewTabToGroup();
}

void CreateNewTabGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CreateNewTabGroup();
}

void CloseTabGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseTabGroup();
}

void FocusNextTabGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusNextTabGroup();
}

void FocusPreviousTabGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusPreviousTabGroup();
}

bool GroupAllUngroupedTabs(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->GroupAllUngroupedTabs();
}

void AddNewTabToRecentGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->AddNewTabToRecentGroup();
}

void UnfocusTabGroup(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->UnfocusTabGroup();
}

void MuteSite(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->MuteSite();
}

void MuteSiteForKeyboardFocusedTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->MuteSiteForKeyboardFocusedTab();
}

void PinKeyboardFocusedTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->PinKeyboardFocusedTab();
}

void GroupKeyboardFocusedTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->GroupKeyboardFocusedTab();
}

void DuplicateKeyboardFocusedTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->DuplicateKeyboardFocusedTab();
}

bool HasKeyboardFocusedTab(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->HasKeyboardFocusedTab();
}

void ConvertPopupToTabbedBrowser(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ConvertPopupToTabbedBrowser();
}

void CloseTabsToRight(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseTabsToRight();
}

void CloseOtherTabs(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseOtherTabs();
}

void Exit() {
  BrowserCommands::Exit();
}

void BookmarkCurrentTab(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->BookmarkCurrentTab();
}

void BookmarkCurrentTabInFolder(BrowserWindowInterface* browser,
                                bookmarks::BookmarkModel* model,
                                int64_t folder_id) {
  BrowserCommands::From(browser)->BookmarkCurrentTabInFolder(model, folder_id);
}

bool CanBookmarkCurrentTab(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanBookmarkCurrentTab();
}

void BookmarkAllTabs(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->BookmarkAllTabs();
}

bool CanBookmarkAllTabs(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanBookmarkAllTabs();
}

bool CanMoveActiveTabToReadLater(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanMoveActiveTabToReadLater();
}

void MoveCurrentTabToReadLater(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->MoveCurrentTabToReadLater();
}

void MoveTabsToReadLater(BrowserWindowInterface* browser,
                         std::vector<content::WebContents*> web_contentses) {
  BrowserCommands::From(browser)->MoveTabsToReadLater(web_contentses);
}

bool MarkCurrentTabAsReadInReadLater(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->MarkCurrentTabAsReadInReadLater();
}

bool IsCurrentTabUnreadInReadLater(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->IsCurrentTabUnreadInReadLater();
}

void ShowOffersAndRewardsForPage(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowOffersAndRewardsForPage();
}

void SaveCreditCard(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->SaveCreditCard();
}

void SaveIban(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->SaveIban();
}

void ShowMandatoryReauthOptInPrompt(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowMandatoryReauthOptInPrompt();
}

void SaveAutofillAddress(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->SaveAutofillAddress();
}

void ShowFilledCardInformationBubble(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowFilledCardInformationBubble();
}

void ShowVirtualCardEnrollBubble(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowVirtualCardEnrollBubble();
}

void ShowTranslateBubble(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowTranslateBubble();
}

void ManagePasswordsForPage(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ManagePasswordsForPage();
}

bool CanSendTabToSelf(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanSendTabToSelf();
}

void SendTabToSelf(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->SendTabToSelf();
}

bool CanGenerateQrCode(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanGenerateQrCode();
}

void GenerateQRCode(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->GenerateQRCode();
}

void SharingHub(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->SharingHub();
}

void ScreenshotCapture(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ScreenshotCapture();
}

void SavePage(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->SavePage();
}

bool CanSavePage(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->CanSavePage();
}

void Print(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->Print();
}

bool CanPrint(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanPrint();
}

#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->BasicPrint();
}

bool CanBasicPrint(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanBasicPrint();
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

bool CanRouteMedia(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanRouteMedia();
}

void RouteMediaInvokedFromAppMenu(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->RouteMediaInvokedFromAppMenu();
}

void Find(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->Find();
}

void FindNext(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FindNext();
}

void FindPrevious(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FindPrevious();
}

void FindInPage(BrowserWindowInterface* browser,
                bool find_next,
                bool forward_direction) {
  BrowserCommands::From(browser)->FindInPage(find_next, forward_direction);
}

void ShowTabSearch(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowTabSearch();
}

void CloseTabSearch(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseTabSearch();
}

void ToggleTabSearchPin(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleTabSearchPin();
}

void ToggleContextualTasksSidePanel(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleContextualTasksSidePanel();
}

void ToggleVerticalTabs(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleVerticalTabs();
}

void ToggleVerticalTabsExpandOnHover(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleVerticalTabsExpandOnHover();
}

bool CanCloseFind(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanCloseFind();
}

void CloseFind(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->CloseFind();
}

void Zoom(BrowserWindowInterface* browser, content::PageZoom zoom) {
  BrowserCommands::From(browser)->Zoom(zoom);
}

void FocusToolbar(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusToolbar();
}

void FocusLocationBar(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusLocationBar();
}

void FocusSearch(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusSearch();
}

void FocusAppMenu(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusAppMenu();
}

void FocusBookmarksToolbar(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusBookmarksToolbar();
}

void FocusInactivePopupForAccessibility(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusInactivePopupForAccessibility();
}

void FocusNextPane(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusNextPane();
}

void FocusPreviousPane(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusPreviousPane();
}

void FocusWebContentsPane(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->FocusWebContentsPane();
}

void ToggleDevToolsWindow(BrowserWindowInterface* browser,
                          DevToolsToggleAction action,
                          DevToolsOpenedByAction opened_by) {
  BrowserCommands::From(browser)->ToggleDevToolsWindow(action, opened_by);
}

bool CanOpenTaskManager() {
  return BrowserCommands::CanOpenTaskManager();
}

void OpenTaskManager(BrowserWindowInterface* browser,
                     task_manager::StartAction start_action) {
  BrowserCommands::OpenTaskManager(browser, start_action);
}

void OpenFeedbackDialog(BrowserWindowInterface* browser,
                        feedback::FeedbackSource source,
                        const std::string& description_template,
                        const std::string& category_tag) {
  BrowserCommands::From(browser)->OpenFeedbackDialog(
      source, description_template, category_tag);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void OpenReportUnsafeSiteDialog(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->OpenReportUnsafeSiteDialog();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void ToggleBookmarkBar(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleBookmarkBar();
}

void ToggleShowFullURLs(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleShowFullURLs();
}

void ToggleShowGoogleLensShortcut(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleShowGoogleLensShortcut();
}

void ToggleShowAiModeOmniboxButton(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleShowAiModeOmniboxButton();
}

void ToggleShowSearchTools(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleShowSearchTools();
}

void ShowAppMenu(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowAppMenu();
}

void ShowAvatarMenu(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowAvatarMenu();
}

void OpenUpdateChromeDialog(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->OpenUpdateChromeDialog();
}

bool CanRequestTabletSite(content::WebContents* current_tab) {
  return BrowserCommands::CanRequestTabletSite(current_tab);
}

bool IsRequestingTabletSite(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->IsRequestingTabletSite();
}

void ToggleRequestTabletSite(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleRequestTabletSite();
}

void SetAndroidOsForTabletSite(content::WebContents* current_tab) {
  BrowserCommands::SetAndroidOsForTabletSite(current_tab);
}

void ToggleFullscreenMode(BrowserWindowInterface* browser,
                          bool user_initiated) {
  BrowserCommands::From(browser)->ToggleFullscreenMode(user_initiated);
}

void ClearCache(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ClearCache();
}

bool IsDebuggerAttachedToCurrentTab(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->IsDebuggerAttachedToCurrentTab();
}

void CopyURL(BrowserWindowInterface* browser,
             content::WebContents* web_contents) {
  BrowserCommands::From(browser)->CopyURL(web_contents);
}

bool CanCopyUrl(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanCopyUrl();
}

bool IsWebAppOrCustomTab(const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->IsWebAppOrCustomTab();
}

BrowserWindowInterface* OpenInChrome(
    BrowserWindowInterface* hosted_app_browser) {
  return BrowserCommands::From(hosted_app_browser)->OpenInChrome();
}

bool CanViewSource(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanViewSource();
}

bool CanToggleCaretBrowsing(BrowserWindowInterface* browser) {
  return BrowserCommands::From(browser)->CanToggleCaretBrowsing();
}

void ToggleCaretBrowsing(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleCaretBrowsing();
}

void PromptToNameWindow(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->PromptToNameWindow();
}

#if BUILDFLAG(IS_CHROMEOS)
void ToggleMultitaskMenu(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ToggleMultitaskMenu();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !defined(TOOLKIT_VIEWS)
std::optional<int> GetKeyboardFocusedTabIndex(
    const BrowserWindowInterface* browser) {
  return BrowserCommands::From(const_cast<BrowserWindowInterface*>(browser))
      ->GetKeyboardFocusedTabIndex();
}
#endif

void ShowIncognitoClearBrowsingDataDialog(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowIncognitoClearBrowsingDataDialog();
}

void ShowIncognitoHistoryDisclaimerDialog(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ShowIncognitoHistoryDisclaimerDialog();
}

bool ShouldInterceptChromeURLNavigationInIncognito(
    BrowserWindowInterface* browser,
    const GURL& url) {
  return BrowserCommands::From(browser)
      ->ShouldInterceptChromeURLNavigationInIncognito(url);
}

void ProcessInterceptedChromeURLNavigationInIncognito(
    BrowserWindowInterface* browser,
    const GURL& url) {
  BrowserCommands::From(browser)
      ->ProcessInterceptedChromeURLNavigationInIncognito(url);
}

void ExecLensOverlay(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ExecLensOverlay();
}

void ExecLensRegionSearch(BrowserWindowInterface* browser) {
  BrowserCommands::From(browser)->ExecLensRegionSearch();
}

}  // namespace chrome

#if BUILDFLAG(ENABLE_EXTENSIONS)
#endif

#if BUILDFLAG(ENABLE_PDF)
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_RLZ)
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#endif

#if BUILDFLAG(IS_MAC)
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#endif

#if !BUILDFLAG(IS_ANDROID)
#endif

namespace chrome {

DEFINE_USER_DATA(BrowserCommands);

// static
BrowserCommands* BrowserCommands::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

BrowserCommands::BrowserCommands(BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(std::in_place,
                                browser->GetUnownedUserDataHost(),
                                *this) {}

BrowserCommands::~BrowserCommands() = default;

}  // namespace chrome

namespace {

const char kOsOverrideForTabletSite[] = "Linux; Android 9; Chrome tablet";
const char kChPlatformOverrideForTabletSite[] = "Android";

// Creates a new tabbed browser window, with the same size, type and profile as
// |original_browser|'s window, inserts |contents| into it, and shows it.
void CreateAndShowNewWindowWithContents(
    std::unique_ptr<content::WebContents> contents,
    BrowserWindowInterface* original_browser) {
  Browser* new_browser = nullptr;
  DCHECK(original_browser->GetType() != BrowserWindowInterface::TYPE_APP_POPUP);
  if (original_browser->GetType() == BrowserWindowInterface::TYPE_APP) {
    const Browser* browser = original_browser->GetBrowserForMigrationOnly();
    new_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        browser->app_name(), browser->is_trusted_source(), gfx::Rect(),
        original_browser->GetProfile(), true));
  } else {
    new_browser = Browser::Create(Browser::CreateParams(
        original_browser->GetType(), original_browser->GetProfile(), true));
  }
  // Preserve the size of the original window. The new window has already
  // been given an offset by the OS, so we shouldn't copy the old bounds.
  ui::BaseWindow* new_window = new_browser->GetWindow();
  new_window->SetBounds(
      gfx::Rect(new_window->GetRestoredBounds().origin(),
                original_browser->GetWindow()->GetRestoredBounds().size()));

  // We need to show the browser now.  Otherwise ContainerWin assumes the
  // WebContents is invisible and won't size it.
  new_window->Show();

  // The page transition below is only for the purpose of inserting the tab.
  new_browser->tab_strip_model()->AddWebContents(std::move(contents), -1,
                                                 ui::PAGE_TRANSITION_LINK,
                                                 AddTabTypes::ADD_ACTIVE);
}

bool GetTabURLAndTitleToSave(content::WebContents* web_contents,
                             GURL* url,
                             std::u16string* title) {
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/40557069
  if (!web_contents) {
    return false;
  }
  return chrome::GetURLAndTitleToBookmark(web_contents, url, title);
}

ReadingListModel* GetReadingListModel(BrowserWindowInterface* browser) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser->GetProfile());
  if (!model || !model->loaded()) {
    return nullptr;  // Ignore requests until model has loaded.
  }
  return model;
}

bool CanMoveWebContentsToReadLater(BrowserWindowInterface* browser,
                                   content::WebContents* web_contents,
                                   ReadingListModel* model,
                                   GURL* url,
                                   std::u16string* title) {
  return model && GetTabURLAndTitleToSave(web_contents, url, title) &&
         model->IsUrlSupported(*url) &&
         !browser->GetProfile()->IsGuestSession();
}

bool BookmarkCurrentTabHelper(BrowserWindowInterface* browser,
                              bookmarks::BookmarkModel* model,
                              GURL* url,
                              std::u16string* title) {
  if (!model || !model->loaded()) {
    return false;  // Ignore requests until bookmarks are loaded.
  }

  content::WebContents* const web_contents =
      browser->GetTabStripModel()->GetActiveWebContents();
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/40557069
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

content::WebContents* DuplicateTabAt(BrowserWindowInterface* browser,
                                     int index,
                                     int dst_index) {
  content::WebContents* contents =
      browser->GetTabStripModel()->GetWebContentsAt(index);
  CHECK(contents);
  std::unique_ptr<content::WebContents> contents_dupe = contents->Clone();
  content::WebContents* raw_contents_dupe = contents_dupe.get();

  bool pinned = false;
  if (browser->GetBrowserForMigrationOnly()->CanSupportWindowFeature(
          Browser::WindowFeature::kFeatureTabStrip)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    TabStripModel* tab_strip_model = browser->GetTabStripModel();
    pinned = tab_strip_model->IsTabPinned(index);
    int add_types = AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER |
                    (pinned ? AddTabTypes::ADD_PINNED : 0);
    const auto old_group = tab_strip_model->GetTabGroupForTab(index);
    tab_strip_model->InsertWebContentsAt(dst_index, std::move(contents_dupe),
                                         add_types, old_group);
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

void RecordTabCloseCount(int count) {
  base::UmaHistogramCounts100("TabStrip.Tab.HotkeyClosedCount", count);
}

void CloseSelectedTabAndRecordTabCountMetric(BrowserWindowInterface* browser) {
  const int selected_tabs_count =
      browser->GetTabStripModel()->selection_model().size();
  RecordTabCloseCount(selected_tabs_count);

  browser->GetTabStripModel()->CloseSelectedTabs();
}

void MoveGroupToWindowImpl(BrowserWindowInterface* source,
                           BrowserWindowInterface* target,
                           tab_groups::TabGroupId group) {
  CHECK(source->GetTabStripModel()->group_model()->ContainsTabGroup(group));

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          source->GetProfile());

  std::unique_ptr<tab_groups::ScopedLocalObservationPauser> observation_pauser;
  if (tab_group_service && tab_group_service->GetGroup(group)) {
    observation_pauser = tab_group_service->CreateScopedLocalObserverPauser();
  }

  std::unique_ptr<DetachedTabCollection> detached_group =
      source->GetTabStripModel()->DetachTabGroupForInsertion(group);
  target->GetTabStripModel()->InsertDetachedTabGroupAt(
      std::move(detached_group), 0);

  target->GetWindow()->Show();
}

void MoveTabsToWindowImpl(BrowserWindowInterface* source,
                          BrowserWindowInterface* target,
                          const std::vector<int>& tab_indices) {
  if (tab_indices.empty()) {
    return;
  }

  TabStripModel* source_model = source->GetTabStripModel();
  TabStripModel* target_model = target->GetTabStripModel();

  // Store the active tab from the source tab strip since this will change as
  // tabs are detached. If the active tab from `source_model` isn't moving,
  // default to activating the first tab being moved.
  const tabs::TabInterface* active_tab =
      std::find(tab_indices.begin(), tab_indices.end(),
                source_model->active_index()) != tab_indices.end()
          ? source_model->GetActiveTab()
          : source_model->GetTabAtIndex(tab_indices[0]);

  for (auto& tab_or_collection :
       source_model->DetachTabsAndCollectionsForInsertion(tab_indices)) {
    if (auto tab =
            std::get_if<std::unique_ptr<DetachedTab>>(&tab_or_collection)) {
      bool active = active_tab == tab->get()->tab.get();
      bool pinned = tab->get()->was_pinned_at_time_of_removal;
      int add_types = (active ? AddTabTypes::ADD_ACTIVE : 0) |
                      (pinned ? AddTabTypes::ADD_PINNED : 0);
      target_model->InsertDetachedTabAt(target_model->count(),
                                        std::move(tab->get()->tab), add_types);
    } else if (auto collection =
                   std::get_if<std::unique_ptr<DetachedTabCollection>>(
                       &tab_or_collection)) {
      if (std::holds_alternative<std::unique_ptr<tabs::TabGroupTabCollection>>(
              collection->get()->collection_)) {
        target_model->InsertDetachedTabGroupAt(std::move(*collection),
                                               target_model->count());
      } else {
        bool pinned = collection->get()->pinned_;
        target_model->InsertDetachedSplitTabAt(
            std::move(*collection),
            pinned ? target_model->IndexOfFirstNonPinnedTab()
                   : target_model->count(),
            pinned);
      }
    }
  }
  target->GetWindow()->Show();
}

Browser* CreateNewBrowser(Browser* browser, bool user_gesture) {
  auto params = Browser::CreateParams(browser->profile(), user_gesture);
  return Browser::Create(params);
}

struct MruTabResult {
  raw_ptr<BrowserWindowInterface> browser;
  int index;
};

std::optional<MruTabResult> GetGlobalMruTab(
    BrowserCollection* collection,
    BrowserWindowInterface* active_browser) {
  BrowserWindowInterface* mru_browser = nullptr;
  int mru_idx = -1;
  base::Time max_time = base::Time::Min();

  collection->ForEach(
      [&](BrowserWindowInterface* b) {
        TabStripModel* model = b->GetTabStripModel();
        int i = 0;
        for (auto it = model->begin(); it != model->end(); ++it, ++i) {
          if (b == active_browser && i == model->active_index()) {
            continue;
          }
          content::WebContents* contents = (*it)->GetContents();
          auto* lifecycle_unit =
              resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
                  contents);
          if (!lifecycle_unit) {
            continue;
          }
          base::Time last_active = lifecycle_unit->GetLastFocusedTime();
          if (last_active > max_time) {
            max_time = last_active;
            mru_browser = b;
            mru_idx = i;
          }
        }
        return true;
      },
      BrowserCollection::Order::kActivation);

  if (mru_browser) {
    return MruTabResult{mru_browser, mru_idx};
  }
  return std::nullopt;
}

void ActivateTab(TabStripModel* model,
                 int index,
                 TabStripUserGestureDetails gesture_detail) {
  std::optional<tab_groups::TabGroupId> group_id =
      model->GetTabGroupForTab(index);
  if (group_id.has_value() && model->IsGroupCollapsed(group_id.value())) {
    TabGroup* group = model->group_model()->GetTabGroup(group_id.value());
    tab_groups::TabGroupVisualData new_visual_data(
        group->visual_data()->title(), group->visual_data()->color(),
        /*is_collapsed=*/false);
    model->ChangeTabGroupVisuals(group_id.value(), std::move(new_visual_data));
  }
  model->ActivateTabAt(index, gesture_detail);
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
const extensions::Extension* GetExtensionForBrowser(
    BrowserWindowInterface* browser) {
  auto* controller = web_app::AppBrowserController::From(browser);
  if (!controller) {
    return nullptr;
  }
  return extensions::ExtensionRegistry::Get(browser->GetProfile())
      ->GetExtensionById(controller->app_id(),
                         extensions::ExtensionRegistry::EVERYTHING);
}
#endif

// Based on |disposition|, creates a new tab as necessary, and returns the
// appropriate tab to navigate.  If that tab is the |current_tab|, reverts the
// location bar contents, since all browser-UI-triggered navigations should
// revert any omnibox edits in the |current_tab|.
WebContents* GetTabAndRevertIfNecessaryHelper(BrowserWindowInterface* browser,
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
          browser->GetTabStripModel()->GetIndexOfWebContents(current_tab);
      const auto group = browser->GetTabStripModel()->GetTabGroupForTab(index);
      browser->GetTabStripModel()->AddWebContents(
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
          Browser::Create(Browser::CreateParams(browser->GetProfile(), true));
      new_browser->tab_strip_model()->AddWebContents(std::move(new_tab), -1,
                                                     ui::PAGE_TRANSITION_LINK,
                                                     AddTabTypes::ADD_ACTIVE);
      new_browser->GetWindow()->Show();
      return raw_new_tab;
    }
    default:
      browser->GetBrowserForMigrationOnly()
          ->window()
          ->GetLocationBar()
          ->Revert();
      return current_tab;
  }
}

// Like the above, but auto-computes the current tab
WebContents* GetTabAndRevertIfNecessary(BrowserWindowInterface* browser,
                                        WindowOpenDisposition disposition) {
  WebContents* activate_tab =
      browser->GetTabStripModel()->GetActiveWebContents();
  return GetTabAndRevertIfNecessaryHelper(browser, disposition, activate_tab);
}

void ReloadInternal(BrowserWindowInterface* browser,
                    WindowOpenDisposition disposition,
                    bool bypass_cache) {
  TabStripModel* const tab_strip_model = browser->GetTabStripModel();
  tabs::TabInterface* const active_tab = tab_strip_model->GetActiveTab();
  WebContents* const active_contents = tab_strip_model->GetActiveWebContents();

  std::vector<WebContents*> tabs_to_reload;

  // When using split view, both tabs composing the split view are considered
  // selected by the `selection_model` and `selection_model().size()` returns 2;
  // even though visually, both tabs are represented by a single UI tab in the
  // tab strip. To detect whether the user has selected multiple UI tabs,
  // compare the number of model selected tabs wither either 2 or 1 depending on
  // whether the active tab is split.
  bool multiple_ui_tabs_selected =
      active_tab && tab_strip_model->selection_model().size() >
                        (active_tab->IsSplit() ? 2 : 1);

  if (multiple_ui_tabs_selected) {
    // Reloading a tab may change the selection (see crbug.com/339061099), so
    // take
    // a defensive copy into a more stable form before we begin. We take
    // WebContents* so we can follow the tabs as they shift within the same
    // tabstrip (e.g. if `disposition` is NEW_BACKGROUND_TAB).
    for (tabs::TabInterface* t :
         tab_strip_model->selection_model().selected_tabs()) {
      tabs_to_reload.push_back(t->GetContents());
    }
  } else {
    tabs_to_reload.push_back(active_contents);
  }

  base::UmaHistogramCounts100("TabStrip.Tab.ReloadCount",
                              tabs_to_reload.size());

  for (WebContents* const tab : tabs_to_reload) {
    // Skip this tab if it is no longer part of this tabstrip. N.B. we do this
    // instead of using WeakPtr<WebContents> because we do not want to reload
    // tabs that move to another browser.
    if (tab_strip_model->GetIndexOfWebContents(tab) == TabStripModel::kNoTab) {
      continue;
    }

    WebContents* const new_tab =
        GetTabAndRevertIfNecessaryHelper(browser, disposition, tab);

    // If the `tab` is the activated page, give the focus to it, as this is
    // caused by a user action
    if (tab == active_contents && !new_tab->FocusLocationBarByDefault()) {
      new_tab->Focus();
    }

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

bool IsShowingWebContentsModalDialog(BrowserWindowInterface* browser) {
  WebContents* const web_contents =
      browser->GetTabStripModel()->GetActiveWebContents();
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
bool PrintPreviewShowing(const BrowserWindowInterface* browser) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  WebContents* contents = browser->GetTabStripModel()->GetActiveWebContents();
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

bool BrowserCommands::IsCommandEnabled(int command) {
  return browser_->GetFeatures().browser_command_controller()->IsCommandEnabled(
      command);
}

bool BrowserCommands::SupportsCommand(int command) {
  return browser_->GetFeatures().browser_command_controller()->SupportsCommand(
      command);
}

bool BrowserCommands::ExecuteCommand(int command, base::TimeTicks time_stamp) {
  return browser_->GetFeatures().browser_command_controller()->ExecuteCommand(
      command, time_stamp);
}

bool BrowserCommands::ExecuteCommandWithDisposition(
    int command,
    WindowOpenDisposition disposition) {
  return browser_->GetFeatures()
      .browser_command_controller()
      ->ExecuteCommandWithDisposition(command, disposition);
}

void BrowserCommands::UpdateCommandEnabled(int command, bool enabled) {
  browser_->GetFeatures().browser_command_controller()->UpdateCommandEnabled(
      command, enabled);
}

void BrowserCommands::AddCommandObserver(int command,
                                         CommandObserver* observer) {
  browser_->GetFeatures().browser_command_controller()->AddCommandObserver(
      command, observer);
}

void BrowserCommands::RemoveCommandObserver(int command,
                                            CommandObserver* observer) {
  browser_->GetFeatures().browser_command_controller()->RemoveCommandObserver(
      command, observer);
}

int BrowserCommands::GetContentRestrictions() {
  int content_restrictions = 0;
  WebContents* const current_tab =
      browser_->GetTabStripModel()->GetActiveWebContents();
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

void BrowserCommands::NewEmptyWindow(Profile* profile,
                                     bool should_trigger_session_restore) {
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

BrowserWindowInterface* BrowserCommands::OpenEmptyWindow(
    Profile* profile,
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

  base::TimeTicks now = base::TimeTicks::Now();
  Browser* browser = Browser::Create(params);
  if (auto* manager = InitialWebUIWindowMetricsManager::From(browser)) {
    manager->SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated, now);
  }

  // Startup tabs could be created during browser creation. Add an empty tab
  // only if no tabs are created.
  if (browser->tab_strip_model()->empty()) {
    AddTabAt(browser, GURL(), -1, true);
  }

  browser->GetWindow()->Show();
  return browser;
}

void BrowserCommands::OpenWindowWithRestoredTabs(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (service) {
    service->RestoreMostRecentEntry(nullptr);
  }
}

void BrowserCommands::OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  ScopedTabbedBrowserDisplayer displayer(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  AddSelectedTabWithURL(displayer.browser(), url, ui::PAGE_TRANSITION_LINK);
}

bool BrowserCommands::CanGoBack() {
  return CanGoBack(browser_->GetTabStripModel()->GetActiveWebContents());
}

bool BrowserCommands::CanGoBack(content::WebContents* web_contents) {
  return web_contents &&
         (web_contents->GetController().CanGoBack() ||
          back_to_opener::BackToOpenerController::CanGoBackToOpener(
              web_contents));
}

bool BrowserCommands::ShouldEnableBackButton() {
  content::WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  // Check for regular back navigation first.
  if (web_contents->GetController().ShouldEnableBackButton()) {
    return true;
  }

  // If no regular back navigation, check for back-to-opener.
  return back_to_opener::BackToOpenerController::CanGoBackToOpener(
      web_contents);
}

enum class BackNavigationMenuIPHTrigger : int {
  kUserPerformsManyBackNavigation = 0,
  kUserPerformsChainedBackNavigation,
  kUserPerformsChainedBackNavigationWithBackButton
};

const char kBackNavigationMenuIPHExperimentParamName[] = "x_experiment";

void MaybeShowFeatureBackNavigationMenuPromo(BrowserWindowInterface* browser,
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
    BrowserUserEducationInterface::From(browser)->MaybeShowFeaturePromo(
        feature_engagement::kIPHBackNavigationMenuFeature);
  }
}

void BrowserCommands::GoBack(WindowOpenDisposition disposition) {
  GoBack(GetTabAndRevertIfNecessary(browser_, disposition));
}

void BrowserCommands::GoBack(content::WebContents* web_contents) {
  base::RecordAction(UserMetricsAction("Back"));

  if (!web_contents) {
    return;
  }

  // Try regular back navigation first.
  if (web_contents->GetController().CanGoBack()) {
    web_contents->GetController().GoBack();
    BrowserWindowInterface* browser =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents);
    if (browser) {
      MaybeShowFeatureBackNavigationMenuPromo(browser, web_contents);
    }
    return;
  }

  // If no regular back navigation, try back-to-opener.
  back_to_opener::BackToOpenerController::GoBackToOpener(web_contents);
}

bool BrowserCommands::CanGoForward() {
  return browser_->GetTabStripModel()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoForward();
}

bool BrowserCommands::CanGoForward(content::WebContents* web_contents) {
  return web_contents->GetController().CanGoForward();
}

bool BrowserCommands::ShouldEnableForwardButton() {
  return browser_->GetTabStripModel()
      ->GetActiveWebContents()
      ->GetController()
      .ShouldEnableForwardButton();
}

void BrowserCommands::GoForward(WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward()) {
    GetTabAndRevertIfNecessary(browser_, disposition)
        ->GetController()
        .GoForward();
  }
}

void BrowserCommands::GoForward(content::WebContents* web_contents) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(web_contents)) {
    web_contents->GetController().GoForward();
  }
}

void BrowserCommands::NavigateToIndexWithDisposition(
    int index,
    WindowOpenDisposition disposition) {
  NavigationController* controller =
      &GetTabAndRevertIfNecessary(browser_, disposition)->GetController();
  DCHECK_GE(index, 0);
  DCHECK_LT(index, controller->GetEntryCount());
  controller->GoToIndex(index);
}

void BrowserCommands::Reload(WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(browser_, disposition, false);
}

void BrowserCommands::ReloadBypassingCache(WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("ReloadBypassingCache"));
  ReloadInternal(browser_, disposition, true);
}

bool BrowserCommands::CanReload() {
  return browser_ &&
         browser_->GetType() != BrowserWindowInterface::TYPE_DEVTOOLS &&
         browser_->GetType() != BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE;
}

void BrowserCommands::Home(WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Home"));

  std::string extra_headers;
#if BUILDFLAG(ENABLE_RLZ)
  // If the home page is a Google home page, add the RLZ header to the request.
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();
  if (pref_service) {
    if (google_util::IsGoogleHomePageUrl(
            GURL(pref_service->GetString(prefs::kHomePage)))) {
      extra_headers = rlz::RLZTracker::GetAccessPointHttpHeader(
          rlz::RLZTracker::ChromeHomePage());
    }
  }
#endif  // BUILDFLAG(ENABLE_RLZ)

  GURL url = browser_->GetProfile()->GetHomePage();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // With bookmark apps enabled, hosted apps should return to their launch page
  // when the home button is pressed.
  if (browser_->GetType() == BrowserWindowInterface::TYPE_APP ||
      browser_->GetType() == BrowserWindowInterface::TYPE_APP_POPUP) {
    const extensions::Extension* extension = GetExtensionForBrowser(browser_);
    if (!extension) {
      return;
    }
    url = extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    extensions::MaybeShowExtensionControlledHomeNotification(
        browser_, browser_->GetTabStripModel()->GetActiveWebContents());
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
  browser_->OpenURL(params, /*navigation_handle_callback=*/{});
}

base::WeakPtr<content::NavigationHandle> BrowserCommands::OpenCurrentURL() {
  base::RecordAction(UserMetricsAction("LoadURL"));
  // TODO(crbug.com/40820294): Eliminate extra checks once source of
  //  bad pointer dereference is identified. See also TODO comment below.
  CHECK(browser_);
  BrowserWindow* window = browser_->GetBrowserForMigrationOnly()->window();
  CHECK(window);
  LocationBar* location_bar = window->GetLocationBar();
  if (!location_bar) {
    return nullptr;
  }

  GURL url(location_bar->navigation_params().destination_url);
  TRACE_EVENT1("navigation", "chrome::OpenCurrentURL", "url", url);

  if (ShouldInterceptChromeURLNavigationInIncognito(url)) {
    ProcessInterceptedChromeURLNavigationInIncognito(url);
    return nullptr;
  }

  NavigateParams params(browser_, url,
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
  Profile* profile = browser_->GetProfile();
  DCHECK(extensions::ExtensionSystem::Get(profile)->extension_service());
  // TODO(crbug.com/40820294): Eliminate extra checks once source of
  //  bad pointer dereference is identified. See also TODO comment above.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
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

void BrowserCommands::Stop() {
  base::RecordAction(UserMetricsAction("Stop"));
  browser_->GetTabStripModel()->GetActiveWebContents()->Stop();
}

void BrowserCommands::NewWindow() {
  Profile* const profile = browser_->GetProfile();
#if BUILDFLAG(IS_MAC)
  // Web apps should open a window to their launch page.
  if (auto* const app_browser_controller =
          web_app::AppBrowserController::From(browser_)) {
    const webapps::AppId app_id = app_browser_controller->app_id();

    auto launch_container = apps::LaunchContainer::kLaunchContainerWindow;

    auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
    if (provider && provider->registrar_unsafe().GetAppEffectiveDisplayMode(
                        app_id) == blink::mojom::DisplayMode::kBrowser) {
      launch_container = apps::LaunchContainer::kLaunchContainerTab;
    }
    apps::AppLaunchParams params = apps::AppLaunchParams(
        app_id, launch_container, WindowOpenDisposition::NEW_WINDOW,
        apps::LaunchSource::kFromKeyboard);
    web_app::LaunchExtensionOrWebApp(profile, std::move(params),
                                     base::DoNothing());
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Hosted apps should open a window to their launch page.
  const extensions::Extension* extension = GetExtensionForBrowser(browser_);
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

void BrowserCommands::NewIncognitoWindow(Profile* profile) {
  NewEmptyWindow(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
}

void BrowserCommands::CloseWindow() {
  base::RecordAction(UserMetricsAction("CloseWindow"));
  browser_->GetWindow()->Close();
}

content::WebContents& BrowserCommands::NewTab(NewTabTypes context) {
  if (context != NewTabTypes::kNoUserAction) {
    base::RecordAction(base::UserMetricsAction("NewTab"));
  }

  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", context,
                            NewTabTypes::kNewTabEnumCount);

  browser_->GetProfile()->SetUserData(
      NewTabGroupingUserData::kNewTabGroupingUserDataKey,
      std::make_unique<NewTabGroupingUserData>(
          browser_->GetTabStripModel()->GetActiveTabGroupId()));

  if (browser_->GetBrowserForMigrationOnly()->SupportsWindowFeature(
          Browser::WindowFeature::kFeatureTabStrip)) {
    std::optional<tab_groups::TabGroupId> group_id;

    if (features::IsNewTabAddsToActiveGroupEnabled()) {
      const int index = browser_->GetTabStripModel()->active_index();
      group_id = browser_->GetTabStripModel()->GetTabGroupForTab(index);
    }

    return *AddAndReturnTabAt(browser_, GURL(), -1, true, group_id);
  }

  ScopedTabbedBrowserDisplayer displayer(browser_->GetProfile());
  BrowserWindowInterface* displayer_browser = displayer.browser();
  auto* contents = AddAndReturnTabAt(displayer_browser, GURL(), -1, true);
  displayer_browser->GetWindow()->Show();
  // The call to AddBlankTabAt above did not set the focus to the tab as its
  // window was not active, so we have to do it explicitly.
  // See http://crbug.com/41270207.
  displayer_browser->GetTabStripModel()->GetActiveWebContents()->RestoreFocus();

  return *contents;
}

void BrowserCommands::NewTabToRight() {
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::CommandNewTabToRight);
}

void BrowserCommands::NewTabFromClipboardURL() {
#if BUILDFLAG(IS_LINUX)
  if (ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    CHECK(clipboard)
        << "Clipboard instance is not available, cannot proceed with "
           "middle mouse button action.";
    clipboard->ReadText(
        ui::ClipboardBuffer::kSelection, /* data_dst = */ std::nullopt,
        base::BindOnce(
            [](base::WeakPtr<Browser> browser_weak, std::u16string text) {
              if (!browser_weak || text.empty()) {
                return;
              }
              base::RecordAction(
                  base::UserMetricsAction("NewTabButton_PasteAndNavigate"));
              AutocompleteMatch match;
              AutocompleteClassifierFactory::GetForProfile(
                  browser_weak->profile())
                  ->Classify(text, false, false,
                             metrics::OmniboxEventProto::BLANK, &match,
                             nullptr);
              if (match.destination_url.is_valid()) {
                browser_weak->tab_strip_model()->delegate()->AddTabAt(
                    match.destination_url, -1, true);
              }
            },
            browser_->GetBrowserForMigrationOnly()->AsWeakPtr()));
  }
#endif
}

void BrowserCommands::CloseTab() {
  base::RecordAction(UserMetricsAction("CloseTab_Accelerator"));

  // If the selection model consists of only the indices of a single split tab,
  // decide if just the active tab in the split is closed instead of all tabs in
  // the split.
  const bool only_active_split_tab_selected =
      browser_->GetTabStripModel()->IsActiveTabSplit() &&
      browser_->GetTabStripModel()->selection_model().size() == 2;
  if (only_active_split_tab_selected) {
    RecordTabCloseCount(1);

    content::WebContents* active_web_contents =
        browser_->GetTabStripModel()->GetActiveWebContents();
    active_web_contents->Close();

    return;
  }

  ToastController* toast_controller =
      browser_->GetFeatures().toast_controller();
  if (!toast_controller) {
    CloseSelectedTabAndRecordTabCountMetric(browser_);
    return;
  }

  tabs::TabInterface* tab = browser_->GetTabStripModel()->GetActiveTab();
  const bool single_pinned_tab_selected =
      tab->IsPinned() &&
      browser_->GetTabStripModel()->selection_model().size() == 1;
  if (single_pinned_tab_selected &&
      toast_controller->GetCurrentToastId() != ToastId::kClosePinnedTab) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    CHECK(browser_view);
    ui::Accelerator accelerator;
    CHECK(
        browser_view->GetAcceleratorForCommandId(IDC_CLOSE_TAB, &accelerator));

    ToastParams params(ToastId::kClosePinnedTab);
    params.body_string_replacement_params.emplace_back(
        accelerator.GetShortcutText());
    toast_controller->MaybeShowToast(std::move(params));
  } else {
    CloseSelectedTabAndRecordTabCountMetric(browser_);
    if (single_pinned_tab_selected) {
      base::RecordAction(
          UserMetricsAction("Tab.PinnedTabToastClosedAfterConfirmation"));
    }
  }
}

bool BrowserCommands::CanZoomIn(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMaximumZoomPercent();
}

bool BrowserCommands::CanZoomOut(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMinimumZoomPercent();
}

bool BrowserCommands::CanResetZoom(content::WebContents* contents) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  return !zoom_controller->IsAtDefaultZoom() ||
         !zoom_controller->PageScaleFactorIsOne();
}

void BrowserCommands::SelectNextTab(TabStripUserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectNextTab"));

  if (base::FeatureList::IsEnabled(features::kCtrlTabMru) &&
      browser_->GetProfile()->GetPrefs()->GetBoolean(prefs::kCtrlTabMru)) {
    auto mru_result = GetGlobalMruTab(
        ProfileBrowserCollection::GetForProfile(browser_->GetProfile()),
        browser_);
    if (mru_result) {
      if (mru_result->browser != browser_) {
        mru_result->browser->GetWindow()->Activate();
      }
      ActivateTab(mru_result->browser->GetTabStripModel(), mru_result->index,
                  gesture_detail);
      return;
    }
  }

  browser_->GetTabStripModel()->SelectNextTab(gesture_detail);
}

void BrowserCommands::SelectPreviousTab(
    TabStripUserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectPrevTab"));
  browser_->GetTabStripModel()->SelectPreviousTab(gesture_detail);
}

void BrowserCommands::MoveTabNext() {
  base::RecordAction(UserMetricsAction("MoveTabNext"));
  browser_->GetTabStripModel()->MoveTabNext();
}

void BrowserCommands::MoveTabPrevious() {
  base::RecordAction(UserMetricsAction("MoveTabPrevious"));
  browser_->GetTabStripModel()->MoveTabPrevious();
}

void BrowserCommands::SelectNumberedTab(
    int index,
    TabStripUserGestureDetails gesture_detail) {
  int visible_count = 0;
  for (int i = 0; i < browser_->GetTabStripModel()->count(); i++) {
    if (browser_->GetTabStripModel()->IsTabCollapsed(i)) {
      continue;
    }
    if (visible_count == index) {
      base::RecordAction(UserMetricsAction("SelectNumberedTab"));
      browser_->GetTabStripModel()->ActivateTabAt(i, gesture_detail);
      break;
    }
    visible_count += 1;
  }
}

void BrowserCommands::SelectLastTab(TabStripUserGestureDetails gesture_detail) {
  for (int i = browser_->GetTabStripModel()->count() - 1; i >= 0; i--) {
    if (!browser_->GetTabStripModel()->IsTabCollapsed(i)) {
      base::RecordAction(UserMetricsAction("SelectLastTab"));
      browser_->GetTabStripModel()->ActivateTabAt(i, gesture_detail);
      break;
    }
  }
}

void BrowserCommands::DuplicateTab() {
  base::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateTabAt(browser_->GetTabStripModel()->active_index());
}

bool BrowserCommands::CanDuplicateTab() {
  return CanDuplicateTabAt(browser_->GetTabStripModel()->active_index());
}

bool BrowserCommands::CanDuplicateKeyboardFocusedTab() {
  if (!HasKeyboardFocusedTab()) {
    return false;
  }
  return CanDuplicateTabAt(*chrome::GetKeyboardFocusedTabIndex(browser_));
}

bool BrowserCommands::CanMoveActiveTabToNewWindow() {
  const ui::ListSelectionModel::SelectedIndices selection =
      browser_->GetTabStripModel()
          ->selection_model()
          .GetListSelectionModel()
          .selected_indices();
  return CanMoveTabsToNewWindow(
      std::vector<int>(selection.begin(), selection.end()));
}

// TODO(crbug.com/435178910) Remove this usage of ListSelectionModel.
void BrowserCommands::MoveActiveTabToNewWindow() {
  const ui::ListSelectionModel::SelectedIndices selection =
      browser_->GetTabStripModel()
          ->selection_model()
          .GetListSelectionModel()
          .selected_indices();
  MoveTabsToNewWindow(std::vector<int>(selection.begin(), selection.end()));
}

bool BrowserCommands::CanMoveTabsToNewWindow(
    const std::vector<int>& tab_indices) {
  if (browser_->GetType() == BrowserWindowInterface::TYPE_APP) {
    for (int index : tab_indices) {
      if (web_app::IsPinnedHomeTab(browser_->GetTabStripModel(), index)) {
        return false;
      }
    }
  }
  return browser_->GetTabStripModel()->count() >
         static_cast<int>(tab_indices.size());
}

void BrowserCommands::MoveGroupToNewWindow(tab_groups::TabGroupId group) {
  Browser* current_browser = browser_->GetBrowserForMigrationOnly();
  Browser* new_browser;
  if (current_browser->is_type_app() &&
      current_browser->app_controller()->has_tab_strip()) {
    new_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        current_browser->app_name(), current_browser->is_trusted_source(),
        gfx::Rect(), current_browser->profile(), true));
    web_app::MaybeAddPinnedHomeTab(new_browser,
                                   new_browser->app_controller()->app_id());
  } else {
    new_browser = CreateNewBrowser(current_browser, true);
  }

  MoveGroupToWindowImpl(browser_, new_browser, group);
}

void BrowserCommands::MoveTabsToNewWindow(const std::vector<int>& tab_indices) {
  if (tab_indices.empty()) {
    return;
  }

  Browser* current_browser = browser_->GetBrowserForMigrationOnly();
  Browser* new_browser;
  base::TimeTicks now = base::TimeTicks::Now();
  if (current_browser->is_type_app() &&
      current_browser->app_controller()->has_tab_strip()) {
    new_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        current_browser->app_name(), current_browser->is_trusted_source(),
        gfx::Rect(), current_browser->profile(), true));
    web_app::MaybeAddPinnedHomeTab(new_browser,
                                   new_browser->app_controller()->app_id());
  } else {
    new_browser = CreateNewBrowser(current_browser, true);
  }
  if (auto* manager = InitialWebUIWindowMetricsManager::From(new_browser)) {
    manager->SetWindowCreationInfo(
        waap::NewWindowCreationSource::kBrowserInitiated, now);
  }

  MoveTabsToWindowImpl(browser_, new_browser, tab_indices);
}

bool BrowserCommands::CanCloseTabsToRight() {
  return browser_->GetTabStripModel()->IsContextMenuCommandEnabled(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

bool BrowserCommands::CanCloseOtherTabs() {
  return browser_->GetTabStripModel()->IsContextMenuCommandEnabled(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

WebContents* BrowserCommands::DuplicateTabAt(int index) {
  return ::DuplicateTabAt(browser_, index, index + 1);
}

void BrowserCommands::DuplicateSplit(split_tabs::SplitTabId split) {
  CHECK(browser_->GetBrowserForMigrationOnly()->CanSupportWindowFeature(
      Browser::WindowFeature::kFeatureTabStrip));

  TabStripModel* model = browser_->GetTabStripModel();
  split_tabs::SplitTabData* split_data = model->GetSplitData(split);
  gfx::Range split_indices_range = split_data->GetIndexRange();

  std::vector<int> duplicated_tab_indices;
  for (size_t split_index = split_indices_range.GetMin();
       split_index < split_indices_range.GetMax(); split_index++) {
    size_t dst_index = split_index + split_indices_range.length();
    ::DuplicateTabAt(browser_, split_index, dst_index);
    duplicated_tab_indices.push_back(dst_index);
  }

  // Activate the tab that was last active in the old split, and then
  // create the new split with the same visual data.
  // TODO(418015278): Revisit if we should store last active tab in the visual
  // data, to make copying it easier.
  int active_index = split_tabs::GetIndexOfLastActiveTab(model, split) +
                     split_indices_range.length();
  model->ActivateTabAt(active_index);
  // AddToNewSplit always creates a split with the active index so remove it
  // from the passed in indices.
  duplicated_tab_indices.erase(std::find(duplicated_tab_indices.begin(),
                                         duplicated_tab_indices.end(),
                                         active_index));
  model->AddToNewSplit(duplicated_tab_indices,
                       split_tabs::SplitTabVisualData(
                           *(model->GetSplitData(split)->visual_data())),
                       split_tabs::SplitTabCreatedSource::kDuplicateSplit);
}

bool BrowserCommands::CanDuplicateTabAt(int index) {
  if (browser_->GetType() == BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE) {
    return false;
  }
  WebContents* contents = browser_->GetTabStripModel()->GetWebContentsAt(index);
  return contents;
}

void BrowserCommands::MoveTabsToExistingWindow(
    BrowserWindowInterface* target,
    const std::vector<int>& tab_indices) {
  MoveTabsToWindowImpl(browser_, target, tab_indices);
}

void BrowserCommands::MoveGroupToExistingWindow(BrowserWindowInterface* target,
                                                tab_groups::TabGroupId group) {
  MoveGroupToWindowImpl(browser_, target, group);
}

void BrowserCommands::PinTab() {
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void BrowserCommands::GroupTab() {
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleGrouped);
}

void BrowserCommands::NewSplitTab(split_tabs::SplitTabLayout layout,
                                  split_tabs::SplitTabCreatedSource source) {
  TabStripModel* const tab_strip_model = browser_->GetTabStripModel();
  const int active_index = tab_strip_model->active_index();
  // In Incognito mode, we can't show the regular Split View NTP so default to
  // the regular NTP which renders special content when in Incognito.
  const GURL new_tab_url = browser_->GetProfile()->IsIncognitoProfile()
                               ? chrome::ChromeUINewTabURLAsGURL()
                               : GURL(chrome::kChromeUISplitViewNewTabPageURL);
  tab_strip_model->delegate()->AddTabAt(
      new_tab_url, active_index + 1, true,
      tab_strip_model->GetTabGroupForTab(active_index),
      tab_strip_model->IsTabPinned(active_index));
  tab_strip_model->AddToNewSplit(
      {active_index}, split_tabs::SplitTabVisualData(layout), source);

  if (content::WebContents* active_contents =
          tab_strip_model->GetActiveWebContents()) {
    active_contents->Focus();
  }
}

void BrowserCommands::AddNewTabToGroup() {
  if (!browser_->GetTabStripModel()->SupportsTabGroups()) {
    return;
  }

  int index = browser_->GetTabStripModel()->active_index();
  std::optional<tab_groups::TabGroupId> group_id =
      browser_->GetTabStripModel()->GetTabGroupForTab(index);
  if (!group_id) {
    return;
  }

  AddTabAt(browser_, GURL(), -1, true, group_id);
}

void BrowserCommands::CreateNewTabGroup() {
  NewTab(NewTabTypes::kNewTabCommand);
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::ContextMenuCommand::CommandAddToNewGroupFromMenuItem);
}

void BrowserCommands::CloseTabGroup() {
  const int index = browser_->GetTabStripModel()->active_index();
  std::optional<tab_groups::TabGroupId> group_id =
      browser_->GetTabStripModel()->GetTabGroupForTab(index);
  if (!group_id) {
    return;
  }

  const int num_tabs_in_group = browser_->GetTabStripModel()
                                    ->group_model()
                                    ->GetTabGroup(group_id.value())
                                    ->tab_count();
  if (num_tabs_in_group == browser_->GetTabStripModel()->count()) {
    // If the group about to be closed has all of the tabs in the browser, add a
    // new tab outside the group to prevent the browser from closing.
    browser_->GetTabStripModel()->delegate()->AddTabAt(GURL(), -1, true);
  }

  browser_->GetTabStripModel()->CloseAllTabsInGroup(group_id.value());
}

void BrowserCommands::FocusNextTabGroup() {
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  if (!tab_strip_model->SupportsTabGroups()) {
    return;
  }

  int current_index = tab_strip_model->active_index();
  std::optional<tab_groups::TabGroupId> current_group_id =
      tab_strip_model->GetTabGroupForTab(current_index);

  // Find the next tab group and focus its first tab.
  int count = tab_strip_model->count();
  for (int i = 1; i < count; ++i) {
    int new_index = (current_index + i) % count;
    std::optional<tab_groups::TabGroupId> new_group_id =
        tab_strip_model->GetTabGroupForTab(new_index);
    if (new_group_id && new_group_id != current_group_id) {
      tab_strip_model->ActivateTabAt(
          new_index, TabStripUserGestureDetails(
                         TabStripUserGestureDetails::GestureType::kKeyboard));
      return;
    }
  }
}

void BrowserCommands::FocusPreviousTabGroup() {
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  if (!tab_strip_model->SupportsTabGroups()) {
    return;
  }

  int current_index = tab_strip_model->active_index();
  std::optional<tab_groups::TabGroupId> current_group_id =
      tab_strip_model->GetTabGroupForTab(current_index);

  // Find the next tab group and focus its first tab.
  int count = tab_strip_model->count();
  for (int i = 1; i < count; ++i) {
    int offset = count - i;
    int new_index = (current_index + offset) % count;
    std::optional<tab_groups::TabGroupId> new_group_id =
        tab_strip_model->GetTabGroupForTab(new_index);
    if (new_group_id && new_group_id != current_group_id) {
      tabs::TabInterface* first_tab_of_group =
          tab_strip_model->group_model()
              ->GetTabGroup(new_group_id.value())
              ->GetFirstTab();
      CHECK(first_tab_of_group);
      tab_strip_model->ActivateTabAt(
          tab_strip_model->GetIndexOfTab(first_tab_of_group),
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kKeyboard));
      return;
    }
  }
}

bool BrowserCommands::GroupAllUngroupedTabs() {
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  if (!tab_strip_model->SupportsTabGroups()) {
    return false;
  }

  int i = 0;
  std::vector<int> indices;
  for (const tabs::TabInterface* t : *tab_strip_model) {
    if (!t->GetGroup() && !t->IsPinned()) {
      indices.push_back(i);
    }
    ++i;
  }
  if (indices.size() == 0) {
    return false;
  }

  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup(indices);
  tab_strip_model->OpenTabGroupEditor(group);
  return true;
}

void BrowserCommands::AddNewTabToRecentGroup() {
  if (!features::IsTabGroupMenuMoreEntryPointsEnabled()) {
    return;
  }

  TabStripModel* tab_strip_model = browser_->GetTabStripModel();

  if (!tab_strip_model->SupportsTabGroups()) {
    return;
  }

  std::optional<tab_groups::TabGroupId> group_id;

  // Add the new tab to the most recently active group.
  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  CHECK(tab_group_model);
  group_id = tab_group_model->GetMostRecentTabGroupId();

  if (!group_id) {
    return;
  }

  AddTabAt(browser_, GURL(), -1, true, group_id);
}

void BrowserCommands::UnfocusTabGroup() {
  if (base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    browser_->GetTabStripModel()->SetFocusedGroup(std::nullopt);
  }
}

void BrowserCommands::MuteSite() {
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void BrowserCommands::MuteSiteForKeyboardFocusedTab() {
  if (!HasKeyboardFocusedTab()) {
    return;
  }
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      *chrome::GetKeyboardFocusedTabIndex(browser_),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void BrowserCommands::PinKeyboardFocusedTab() {
  if (!HasKeyboardFocusedTab()) {
    return;
  }
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      *chrome::GetKeyboardFocusedTabIndex(browser_),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void BrowserCommands::GroupKeyboardFocusedTab() {
  if (!HasKeyboardFocusedTab()) {
    return;
  }
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      *chrome::GetKeyboardFocusedTabIndex(browser_),
      TabStripModel::ContextMenuCommand::CommandToggleGrouped);
}

void BrowserCommands::DuplicateKeyboardFocusedTab() {
  if (HasKeyboardFocusedTab()) {
    DuplicateTabAt(*chrome::GetKeyboardFocusedTabIndex(browser_));
  }
}

bool BrowserCommands::HasKeyboardFocusedTab() {
  return chrome::GetKeyboardFocusedTabIndex(browser_).has_value();
}

void BrowserCommands::ConvertPopupToTabbedBrowser() {
  base::RecordAction(UserMetricsAction("ShowAsTab"));
  TabStripModel* tab_strip = browser_->GetTabStripModel();
  // If this popup is the last browser object, removing it from the browser-list
  // will trigger OnShutdownStarting for Window close. Create the new browser
  // object first, before removing the existing object from the browser-list in
  // order to avoid incorrectly triggering a shutdown.
  BrowserWindowInterface* new_tabbed_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
          *browser_->GetProfile(), /*from_user_gesture=*/true));
  // This method moves a WebContents from a non-normal browser window to a
  // normal browser window. We cannot move the Tab over directly since TabModel
  // enforces the requirement that it cannot move between window types.
  // https://crbug.com/334281979): Non-normal browser windows should not have a
  // tab to begin with.
  std::unique_ptr<content::WebContents> contents_move =
      tab_strip->DetachWebContentsAtForInsertion(tab_strip->active_index());

  // This method moves a WebContents from a non-normal browser window to a
  // normal browser window. We cannot move the Tab over directly since TabModel
  // enforces the requirement that it cannot move between window types.
  // https://crbug.com/334281979): Non-normal browser windows should not have a
  // tab to begin with.
  new_tabbed_browser->GetTabStripModel()->AppendWebContents(
      std::move(contents_move), true);
  new_tabbed_browser->GetWindow()->Show();
}

void BrowserCommands::CloseTabsToRight() {
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

void BrowserCommands::CloseOtherTabs() {
  browser_->GetTabStripModel()->ExecuteContextMenuCommand(
      browser_->GetTabStripModel()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

void BrowserCommands::Exit() {
  base::RecordAction(UserMetricsAction("Exit"));
  chrome::AttemptUserExit();
}

void BrowserCommands::BookmarkCurrentTab() {
  base::RecordAction(base::UserMetricsAction("Star"));
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  GURL url;
  std::u16string title;
  if (!BookmarkCurrentTabHelper(browser_, model, &url, &title)) {
    return;
  }
  bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
  bookmarks::AddIfNotBookmarked(model, url, title);
  bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (browser_->GetWindow()->IsActive() && is_bookmarked_by_user) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations where the bubble is deleted as soon as it is shown.
    browser_->GetBrowserForMigrationOnly()->window()->ShowBookmarkBubble(
        url, was_bookmarked_by_user);
  }

  if (!was_bookmarked_by_user && is_bookmarked_by_user) {
    RecordBookmarksAdded(browser_->GetProfile());
  }
}

void BrowserCommands::BookmarkCurrentTabInFolder(BookmarkModel* model,
                                                 int64_t folder_id) {
  GURL url;
  std::u16string title;
  if (!BookmarkCurrentTabHelper(browser_, model, &url, &title)) {
    return;
  }
  const bookmarks::BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, folder_id);
  if (parent) {
    bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
    model->AddNewURL(parent, 0, title, url);
    bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
    if (!was_bookmarked_by_user && is_bookmarked_by_user) {
      RecordBookmarksAdded(browser_->GetProfile());
    }
  }
}

bool BrowserCommands::CanBookmarkCurrentTab() {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  return browser_defaults::bookmarks_enabled &&
         browser_->GetProfile()->GetPrefs()->GetBoolean(
             bookmarks::prefs::kEditBookmarksEnabled) &&
         model && model->loaded() &&
         browser_->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

void BrowserCommands::BookmarkAllTabs() {
  base::RecordAction(UserMetricsAction("BookmarkAllTabs"));

  bookmarks::ShowBookmarkAllTabsDialog(browser_);
}

bool BrowserCommands::CanBookmarkAllTabs() {
  return browser_->GetTabStripModel()->count() > 1 && CanBookmarkCurrentTab();
}

bool BrowserCommands::CanMoveActiveTabToReadLater() {
  GURL url;
  std::u16string title;
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  ReadingListModel* model = GetReadingListModel(browser_);
  return CanMoveWebContentsToReadLater(browser_, web_contents, model, &url,
                                       &title);
}

void BrowserCommands::MoveCurrentTabToReadLater() {
  MoveTabsToReadLater({browser_->GetTabStripModel()->GetActiveWebContents()});
}

void BrowserCommands::MoveTabsToReadLater(
    std::vector<content::WebContents*> web_contentses) {
  int added_to_read_later = 0;
  for (WebContents* const web_contents : web_contentses) {
    GURL url;
    std::u16string title;
    ReadingListModel* model = GetReadingListModel(browser_);
    if (!CanMoveWebContentsToReadLater(browser_, web_contents, model, &url,
                                       &title)) {
      continue;
    }
    model->AddOrReplaceEntry(url, base::UTF16ToUTF8(title),
                             reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);
    BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingListDiscoveryFeature);
    added_to_read_later += 1;
  }

  if (added_to_read_later == 0) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (toast_features::IsEnabled(toast_features::kReadingListToast)) {
    // Don't show the reading list toast if the side panel is visible.
    if (browser_->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
            SidePanelEntryKey(SidePanelEntryId::kReadingList))) {
      return;
    }

    ToastController* const toast_controller =
        browser_->GetFeatures().toast_controller();
    if (toast_controller) {
      ToastParams params = ToastParams(ToastId::kAddedToReadingList);
      params.body_string_cardinality_param = added_to_read_later;
      toast_controller->MaybeShowToast(std::move(params));
    }
  }
#endif
}

bool BrowserCommands::MarkCurrentTabAsReadInReadLater() {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser_);
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
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

bool BrowserCommands::IsCurrentTabUnreadInReadLater() {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser_);
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!model || !GetTabURLAndTitleToSave(web_contents, &url, &title)) {
    return false;
  }
  scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
  return entry && !entry->IsRead();
}

void BrowserCommands::ShowOffersAndRewardsForPage() {
  WebContents* const web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  autofill::OfferNotificationBubbleControllerImpl* controller =
      autofill::OfferNotificationBubbleControllerImpl::FromWebContents(
          web_contents);
  DCHECK(controller);
  controller->ReshowBubble();
}

void BrowserCommands::SaveCreditCard() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble(/*is_user_gesture=*/true);
}

void BrowserCommands::SaveIban() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  autofill::IbanBubbleControllerImpl* controller =
      autofill::IbanBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble();
}

void BrowserCommands::ShowMandatoryReauthOptInPrompt() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  autofill::MandatoryReauthBubbleControllerImpl* controller =
      autofill::MandatoryReauthBubbleControllerImpl::FromWebContents(
          web_contents);
  controller->ReshowBubble();
}

void BrowserCommands::SaveAutofillAddress() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  autofill::AddressBubblesController* controller =
      autofill::AddressBubblesController::FromWebContents(web_contents);
  controller->OnIconClicked();
}

void BrowserCommands::ShowFilledCardInformationBubble() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  auto* controller =
      autofill::FilledCardInformationBubbleControllerImpl::FromWebContents(
          web_contents);
  if (controller) {
    controller->ReshowBubble();
  }
}

void BrowserCommands::ShowVirtualCardEnrollBubble() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  autofill::VirtualCardEnrollBubbleControllerImpl* controller =
      autofill::VirtualCardEnrollBubbleControllerImpl::FromWebContents(
          web_contents);
  if (controller) {
    controller->ReshowBubble();
  }
}

void BrowserCommands::ShowTranslateBubble() {
  if (!browser_->GetWindow()->IsActive()) {
    return;
  }

  WebContents* const web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
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
    source_language = language_detection::kUnknownLanguageCode;
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
  browser_->GetBrowserForMigrationOnly()->window()->ShowTranslateBubble(
      web_contents, step, source_language, target_language,
      translate::TranslateErrors::NONE, true);
}

void BrowserCommands::ManagePasswordsForPage() {
  auto* const user_education = BrowserUserEducationInterface::From(browser_);
  user_education->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  user_education->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  user_education->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordManagerShortcutFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  WebContents* const web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  controller->QueueOrShowBubble(
      /*user_action=*/!controller->IsAutomaticallyOpeningBubble());
}

bool BrowserCommands::CanSendTabToSelf() {
  return send_tab_to_self::ShouldDisplayEntryPoint(
      browser_->GetTabStripModel()->GetActiveWebContents());
}

void BrowserCommands::SendTabToSelf() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  send_tab_to_self::ShowBubble(web_contents);
}

bool BrowserCommands::CanGenerateQrCode() {
  return !sharing_hub::SharingIsDisabledByPolicy(browser_->GetProfile()) &&
         qrcode_generator::QRCodeGeneratorBubbleController::
             IsGeneratorAvailable(browser_->GetTabStripModel()
                                      ->GetActiveWebContents()
                                      ->GetController()
                                      .GetLastCommittedEntry()
                                      ->GetURL());
}

void BrowserCommands::GenerateQRCode() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  qrcode_generator::QRCodeGeneratorBubbleController* controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  controller->ShowBubble(entry->GetURL());
}

void BrowserCommands::SharingHub() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  sharing_hub::SharingHubBubbleController* controller =
      sharing_hub::SharingHubBubbleController::CreateOrGetFromWebContents(
          web_contents);
  controller->ShowBubble(share::ShareAttempt(web_contents));
}

void BrowserCommands::ScreenshotCapture() {
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  sharing_hub::ScreenshotCapturedBubbleController* controller =
      sharing_hub::ScreenshotCapturedBubbleController::Get(web_contents);
  controller->Capture(browser_);
}

void BrowserCommands::SavePage() {
  base::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab =
      browser_->GetTabStripModel()->GetActiveWebContents();
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

bool BrowserCommands::CanSavePage() {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(
          prefs::kAllowFileSelectionDialogs)) {
    return false;
  }
  if (static_cast<policy::DownloadRestriction>(
          browser_->GetProfile()->GetPrefs()->GetInteger(
              policy::policy_prefs::kDownloadRestrictions)) ==
      policy::DownloadRestriction::ALL_FILES) {
    return false;
  }
  return (browser_->GetType() != BrowserWindowInterface::Type::TYPE_DEVTOOLS) &&
         !(GetContentRestrictions() & CONTENT_RESTRICTION_SAVE);
}

void BrowserCommands::Print() {
#if BUILDFLAG(ENABLE_PRINTING)
  auto* const web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();

  printing::StartPrint(web_contents,
#if BUILDFLAG(IS_CHROMEOS)
                       /*print_renderer=*/mojo::NullAssociatedRemote(),
#endif
                       browser_->GetProfile()->GetPrefs()->GetBoolean(
                           prefs::kPrintPreviewDisabled),
                       /*has_selection=*/false);
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

bool BrowserCommands::CanPrint() {
#if BUILDFLAG(ENABLE_PRINTING)
  // Do not print when printing is disabled via pref or policy.
  // Do not print when a page has crashed.
  // Do not print when a constrained window is showing. It's confusing.
  // TODO(gbillock): Need to re-assess the call to
  // IsShowingWebContentsModalDialog after a popup management policy is
  // refined -- we will probably want to just queue the print request, not
  // block it.
  WebContents* const current_tab =
      browser_->GetTabStripModel()->GetActiveWebContents();
  return browser_->GetProfile()->GetPrefs()->GetBoolean(
             prefs::kPrintingEnabled) &&
         (current_tab && !current_tab->IsCrashed()) &&
         !(IsShowingWebContentsModalDialog(browser_) ||
           GetContentRestrictions() & CONTENT_RESTRICTION_PRINT);
#else   // BUILDFLAG(ENABLE_PRINTING)
  return false;
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

#if BUILDFLAG(ENABLE_PRINTING)
void BrowserCommands::BasicPrint() {
  printing::StartBasicPrint(
      browser_->GetTabStripModel()->GetActiveWebContents());
}

bool BrowserCommands::CanBasicPrint() {
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // If printing is not disabled via pref or policy, it is always possible to
  // advanced print when the print preview is visible.
  return browser_->GetProfile()->GetPrefs()->GetBoolean(
             prefs::kPrintingEnabled) &&
         (PrintPreviewShowing(browser_) || CanPrint());
#else
  return false;  // The print dialog is disabled.
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

bool BrowserCommands::CanRouteMedia() {
  // Do not allow user to open Media Router dialog when there is already an
  // active modal dialog. This avoids overlapping dialogs.
  return media_router::MediaRouterEnabled(browser_->GetProfile()) &&
         !IsShowingWebContentsModalDialog(browser_);
}

void BrowserCommands::RouteMediaInvokedFromAppMenu() {
  DCHECK(CanRouteMedia());

  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          browser_->GetTabStripModel()->GetActiveWebContents());
  if (!dialog_controller) {
    return;
  }

  dialog_controller->ShowMediaRouterDialog(
      media_router::MediaRouterDialogActivationLocation::APP_MENU);
}

void BrowserCommands::Find() {
  base::RecordAction(UserMetricsAction("Find"));
  FindInPage(false, true);
}

void BrowserCommands::FindNext() {
  base::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(true, true);
}

void BrowserCommands::FindPrevious() {
  base::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(true, false);
}

void BrowserCommands::FindInPage(bool find_next, bool forward_direction) {
  browser_->GetFeatures().GetFindBarController()->Show(find_next,
                                                       forward_direction);
}

void BrowserCommands::ShowTabSearch() {
  browser_->GetBrowserForMigrationOnly()->window()->CreateTabSearchBubble();
}

void BrowserCommands::CloseTabSearch() {
  browser_->GetBrowserForMigrationOnly()->window()->CloseTabSearchBubble();
}

void BrowserCommands::ToggleTabSearchPin() {
  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  const bool is_pinned = prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip);
  base::RecordAction(base::UserMetricsAction(
      is_pinned ? "TabStripComboButton.TabSearch.Unpinned"
                : "TabStripComboButton.TabSearch.Pinned"));
  prefs->SetBoolean(prefs::kTabSearchPinnedToTabstrip, !is_pinned);
}

void BrowserCommands::ToggleContextualTasksSidePanel() {
  auto* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser_);
  CHECK(controller);
  if (controller->IsPanelOpenForContextualTask()) {
    controller->Close();
  } else {
    controller->Show();
  }
}

void BrowserCommands::ToggleVerticalTabs() {
  tabs::VerticalTabStripStateController* controller =
      tabs::VerticalTabStripStateController::From(browser_);
  if (!controller) {
    return;
  }
  controller->SetVerticalTabsEnabled(!controller->ShouldDisplayVerticalTabs());
}

void BrowserCommands::ToggleVerticalTabsExpandOnHover() {
  tabs::VerticalTabStripStateController* controller =
      tabs::VerticalTabStripStateController::From(browser_);
  if (!controller) {
    return;
  }
  controller->SetExpandOnHoverEnabled(!controller->IsExpandOnHoverEnabled());
}

bool BrowserCommands::CanCloseFind() {
  WebContents* current_tab =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!current_tab) {
    return false;
  }

  find_in_page::FindTabHelper* find_helper =
      find_in_page::FindTabHelper::FromWebContents(current_tab);
  return find_helper ? find_helper->find_ui_active() : false;
}

void BrowserCommands::CloseFind() {
  browser_->GetFeatures().GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
}

void BrowserCommands::Zoom(content::PageZoom zoom) {
  zoom::PageZoom::Zoom(browser_->GetTabStripModel()->GetActiveWebContents(),
                       zoom);
}

void BrowserCommands::FocusToolbar() {
  base::RecordAction(UserMetricsAction("FocusToolbar"));
  browser_->GetBrowserForMigrationOnly()->window()->FocusToolbar();
}

void BrowserCommands::FocusLocationBar() {
  base::RecordAction(UserMetricsAction("FocusLocation"));
  browser_->GetBrowserForMigrationOnly()->window()->SetFocusToLocationBar(true);
}

void BrowserCommands::FocusSearch() {
  // TODO(beng): replace this with FocusLocationBar
  base::RecordAction(UserMetricsAction("FocusSearch"));
  browser_->GetBrowserForMigrationOnly()
      ->window()
      ->GetLocationBar()
      ->FocusSearch();
}

void BrowserCommands::FocusAppMenu() {
  base::RecordAction(UserMetricsAction("FocusAppMenu"));
  browser_->GetBrowserForMigrationOnly()->window()->FocusAppMenu();
}

void BrowserCommands::FocusBookmarksToolbar() {
  base::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  BookmarkBarController::From(browser_)->FocusBookmarksToolbar();
}

void BrowserCommands::FocusInactivePopupForAccessibility() {
  base::RecordAction(UserMetricsAction("FocusInactivePopupForAccessibility"));
  BrowserFocusController::From(browser_)->FocusInactivePopupForAccessibility();
}

void BrowserCommands::FocusNextPane() {
  base::RecordAction(UserMetricsAction("FocusNextPane"));
  BrowserFocusController::From(browser_)->RotatePaneFocus(true);
}

void BrowserCommands::FocusPreviousPane() {
  base::RecordAction(UserMetricsAction("FocusPreviousPane"));
  BrowserFocusController::From(browser_)->RotatePaneFocus(false);
}

void BrowserCommands::FocusWebContentsPane() {
  base::RecordAction(UserMetricsAction("FocusWebContentsPane"));
  BrowserFocusController::From(browser_)->FocusWebContentsPane();
}

void BrowserCommands::ToggleDevToolsWindow(DevToolsToggleAction action,
                                           DevToolsOpenedByAction opened_by) {
  if (action.type() == DevToolsToggleAction::kShowConsolePanel) {
    base::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  } else {
    base::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));
  }
  DevToolsWindow::ToggleDevToolsWindow(browser_, action, opened_by);
}

bool BrowserCommands::CanOpenTaskManager() {
#if !BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

void BrowserCommands::OpenTaskManager(BrowserWindowInterface* browser,
                                      task_manager::StartAction start_action) {
#if !BUILDFLAG(IS_ANDROID)
  base::RecordAction(UserMetricsAction("TaskManager"));
  chrome::ShowTaskManager(
      browser ? browser->GetBrowserForMigrationOnly() : nullptr, start_action);
#else
  NOTREACHED();
#endif
}

void BrowserCommands::OpenFeedbackDialog(
    feedback::FeedbackSource source,
    const std::string& description_template,
    const std::string& category_tag) {
  base::RecordAction(UserMetricsAction("Feedback"));
  chrome::ShowFeedbackPage(browser_, source, description_template,
                           std::string() /* description_placeholder_text */,
                           category_tag, std::string() /* extra_diagnostics */);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void BrowserCommands::OpenReportUnsafeSiteDialog() {
  base::RecordAction(UserMetricsAction("ReportUnsafeSite"));
  feedback::ReportUnsafeSiteDialog::Show(
      browser_->GetBrowserForMigrationOnly());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void BrowserCommands::ToggleBookmarkBar() {
  base::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  ToggleBookmarkBarWhenVisible(browser_->GetProfile());
}

void BrowserCommands::ToggleShowFullURLs() {
  bool pref_enabled = browser_->GetProfile()->GetPrefs()->GetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox);
  browser_->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox, !pref_enabled);
}

void BrowserCommands::ToggleShowGoogleLensShortcut() {
  bool pref_enabled = browser_->GetProfile()->GetPrefs()->GetBoolean(
      omnibox::kShowGoogleLensShortcut);
  browser_->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kShowGoogleLensShortcut, !pref_enabled);
}

void BrowserCommands::ToggleShowAiModeOmniboxButton() {
  bool pref_enabled = browser_->GetProfile()->GetPrefs()->GetBoolean(
      omnibox::kShowAiModeOmniboxButton);
  browser_->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kShowAiModeOmniboxButton, !pref_enabled);
}

void BrowserCommands::ToggleShowSearchTools() {
  bool pref_enabled =
      browser_->GetProfile()->GetPrefs()->GetBoolean(omnibox::kShowSearchTools);
  browser_->GetProfile()->GetPrefs()->SetBoolean(omnibox::kShowSearchTools,
                                                 !pref_enabled);
}

void BrowserCommands::ShowAppMenu() {
  // We record the user metric for this event in AppMenu::RunMenu.
  browser_->GetBrowserForMigrationOnly()->window()->ShowAppMenu();
}

void BrowserCommands::ShowAvatarMenu() {
  browser_->GetBrowserForMigrationOnly()
      ->window()
      ->ShowAvatarBubbleFromAvatarButton(
          /*is_source_accelerator=*/true);
}

// TODO(crbug.com/345770406): Rename the function name.
// We removed the extra confirmation step in the Chrome update flow. After the
// full rollout of the code, this name will be misleading. We will clean up the
// code and its related source enums.
void BrowserCommands::OpenUpdateChromeDialog() {
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  if (detector->is_outdated_install()) {
    ShowOutdatedUpgradeBubble(browser_, browser_,
                              /*auto_update_enabled=*/true);
  } else if (detector->is_outdated_install_no_au()) {
    ShowOutdatedUpgradeBubble(browser_, browser_,
                              /*auto_update_enabled=*/false);
  } else {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    if (base::FeatureList::IsEnabled(features::kFewerUpdateConfirmations)) {
      chrome::AttemptRelaunch();
      return;
    }
#endif
    base::RecordAction(UserMetricsAction("UpdateChrome"));
    browser_->GetBrowserForMigrationOnly()->window()->ShowUpdateChromeDialog();
  }
}

bool BrowserCommands::CanRequestTabletSite(WebContents* current_tab) {
  return current_tab &&
         current_tab->GetController().GetLastCommittedEntry() != nullptr;
}

bool BrowserCommands::IsRequestingTabletSite() {
  WebContents* current_tab =
      browser_->GetTabStripModel()->GetActiveWebContents();
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

void BrowserCommands::ToggleRequestTabletSite() {
  WebContents* current_tab =
      browser_->GetTabStripModel()->GetActiveWebContents();
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

void BrowserCommands::SetAndroidOsForTabletSite(
    content::WebContents* current_tab) {
  DCHECK(current_tab);
  NavigationEntry* entry = current_tab->GetController().GetLastCommittedEntry();
  if (entry) {
    entry->SetIsOverridingUserAgent(true);
    std::string product = embedder_support::GetProductAndVersion() + " Mobile";
    blink::UserAgentOverride ua_override;
    ua_override.ua_string_override =
        embedder_support::BuildUserAgentFromOSAndProduct(
            kOsOverrideForTabletSite, product);
    ua_override.ua_metadata_override = embedder_support::GetUserAgentMetadata();
    ua_override.ua_metadata_override->mobile = true;
    ua_override.ua_metadata_override->form_factors = {blink::kTabletFormFactor};
    ua_override.ua_metadata_override->platform =
        kChPlatformOverrideForTabletSite;
    ua_override.ua_metadata_override->platform_version = std::string();
    current_tab->SetUserAgentOverride(ua_override, false);
  }
}

void BrowserCommands::ToggleFullscreenMode(bool user_initiated) {
  DCHECK(browser_);
  browser_->GetFeatures()
      .exclusive_access_manager()
      ->fullscreen_controller()
      ->ToggleBrowserFullscreenMode(user_initiated);
}

void BrowserCommands::ClearCache() {
  content::BrowsingDataRemover* remover =
      browser_->GetProfile()->GetBrowsingDataRemover();
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

bool BrowserCommands::IsDebuggerAttachedToCurrentTab() {
  WebContents* contents = browser_->GetTabStripModel()->GetActiveWebContents();
  return contents ? content::DevToolsAgentHost::IsDebuggerAttached(contents)
                  : false;
}

void BrowserCommands::CopyURL(content::WebContents* web_contents) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(web_contents->GetVisibleURL().spec()));

#if !BUILDFLAG(IS_ANDROID)
  if (toast_features::IsEnabled(toast_features::kLinkCopiedToast)) {
    ToastController* const toast_controller =
        browser_->GetFeatures().toast_controller();
    if (toast_controller) {
      toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied));
    }
  }
#endif
}

bool BrowserCommands::CanCopyUrl() {
  return IsWebAppOrCustomTab() ||
         !sharing_hub::SharingIsDisabledByPolicy(browser_->GetProfile());
}

bool BrowserCommands::IsWebAppOrCustomTab() {
  return web_app::AppBrowserController::IsWebApp(browser_);
}

BrowserWindowInterface* BrowserCommands::OpenInChrome() {
  // Find a non-incognito browser.
  BrowserWindowInterface* target_browser =
      ProfileBrowserCollection::GetForProfile(browser_->GetProfile())
          ->FindTabbedBrowser();

  if (!target_browser) {
    target_browser =
        Browser::Create(Browser::CreateParams(browser_->GetProfile(), true));
  }

  web_app::ReparentWebContentsIntoBrowserImpl(
      browser_, browser_->GetTabStripModel()->GetActiveWebContents(),
      target_browser);
  return target_browser;
}

bool BrowserCommands::CanViewSource() {
  if (browser_->GetType() == BrowserWindowInterface::TYPE_DEVTOOLS) {
    return false;
  }

  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();

  // Disallow ViewSource if DevTools are disabled.
  if (!DevToolsWindow::AllowDevToolsFor(browser_->GetProfile(), web_contents)) {
    return false;
  }
  return web_contents->GetController().CanViewSource();
}

bool BrowserCommands::CanToggleCaretBrowsing() {
#if BUILDFLAG(IS_MAC)
  // On Mac, ignore the keyboard shortcut unless web contents is focused,
  // because the keyboard shortcut interferes with a Japenese IME when the
  // omnibox is focused.  See https://crbug.com/40725478
  WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  return rwhv && rwhv->HasFocus();
#else
  return true;
#endif  // BUILDFLAG(IS_MAC)
}

void BrowserCommands::ToggleCaretBrowsing() {
  if (!CanToggleCaretBrowsing()) {
    return;
  }

  PrefService* prefService = browser_->GetProfile()->GetPrefs();
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
    browser_->GetBrowserForMigrationOnly()->window()->ShowCaretBrowsingDialog();
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CaretBrowsing.EnableWithKeyboard"));
    prefService->SetBoolean(prefs::kCaretBrowsingEnabled, true);
  }
}

void BrowserCommands::PromptToNameWindow() {
  chrome::ShowWindowNamePrompt(browser_->GetBrowserForMigrationOnly());
}

#if BUILDFLAG(IS_CHROMEOS)
void BrowserCommands::ToggleMultitaskMenu() {
  browser_->GetBrowserForMigrationOnly()->window()->ToggleMultitaskMenu();
}
#endif

#if !defined(TOOLKIT_VIEWS)
std::optional<int> BrowserCommands::GetKeyboardFocusedTabIndex() {
  return std::nullopt;
}
#endif

void BrowserCommands::ShowIncognitoClearBrowsingDataDialog() {
  browser_->GetBrowserForMigrationOnly()
      ->window()
      ->ShowIncognitoClearBrowsingDataDialog();
}

void BrowserCommands::ShowIncognitoHistoryDisclaimerDialog() {
  browser_->GetBrowserForMigrationOnly()
      ->window()
      ->ShowIncognitoHistoryDisclaimerDialog();
}

bool BrowserCommands::ShouldInterceptChromeURLNavigationInIncognito(
    const GURL& url) {
  if (!browser_ || !browser_->GetProfile()->IsIncognitoProfile()) {
    return false;
  }

  bool show_clear_browsing_data_dialog =
      url == GURL(chrome::kChromeUISettingsURL)
                 .Resolve(chrome::kClearBrowserDataSubPage);

  bool show_history_disclaimer_dialog =
      url == GURL(chrome::kChromeUIHistoryURL);

  return show_clear_browsing_data_dialog || show_history_disclaimer_dialog;
}

void BrowserCommands::ProcessInterceptedChromeURLNavigationInIncognito(
    const GURL& url) {
  if (url == GURL(chrome::kChromeUISettingsURL)
                 .Resolve(chrome::kClearBrowserDataSubPage)) {
    ShowIncognitoClearBrowsingDataDialog();
  } else if (url == GURL(chrome::kChromeUIHistoryURL)) {
    ShowIncognitoHistoryDisclaimerDialog();
  } else {
    NOTREACHED();
  }
}

void BrowserCommands::ExecLensOverlay() {
  content::WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  CHECK(web_contents);

  LensSearchController* const controller =
      LensSearchController::FromTabWebContents(web_contents);
  CHECK(controller);
  controller->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);
  BrowserUserEducationInterface::From(browser_)->NotifyNewBadgeFeatureUsed(
      lens::features::kLensOverlay);
}

void BrowserCommands::ExecLensRegionSearch() {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  Profile* profile = browser_->GetProfile();
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  WebContents* contents = browser_->GetTabStripModel()->GetActiveWebContents();
  GURL url = contents->GetController().GetLastCommittedEntry()->GetURL();

  if (lens::IsRegionSearchEnabled(browser_, profile, service, url)) {
    const bool is_google_dsp = search::DefaultSearchProviderIsGoogle(profile);
    const lens::AmbientSearchEntryPoint entry_point =
        is_google_dsp ? lens::AmbientSearchEntryPoint::
                            CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS
                      : lens::AmbientSearchEntryPoint::
                            CONTEXT_MENU_SEARCH_REGION_WITH_WEB;
    browser_->GetFeatures().lens_region_search_controller()->Start(
        contents,
        /*use_fullscreen_capture=*/false, is_google_dsp, entry_point);
  }
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
}

}  // namespace chrome
