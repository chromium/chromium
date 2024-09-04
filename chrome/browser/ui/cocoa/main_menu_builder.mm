// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/main_menu_builder.h"

#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace chrome {
namespace {

using Item = internal::MenuItemBuilder;

NSMenuItem* BuildAppMenu(NSApplication* nsapp,
                         id app_delegate,
                         const std::u16string& product_name,
                         bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      // NB: The IDS_APP_MENU_PRODUCT_NAME string is not actually used to
      // determine what is displayed in bold in the menu bar as the app menu
      // title. The Info.plist's CFBundleName value is what is actually used.
      Item(IDS_APP_MENU_PRODUCT_NAME)
          .tag(IDC_CHROME_MENU)
          .submenu({
              Item(IDS_ABOUT_MAC)
                  .string_format_1(product_name)
                  .tag(IDC_ABOUT)
                  .target(app_delegate)
                  .action(@selector(orderFrontStandardAboutPanel:)),
              Item().is_separator(),
              Item(IDS_PREFERENCES)
                  .tag(IDC_OPTIONS)
                  .target(app_delegate)
                  .action(@selector(showPreferences:))
                  .remove_if(is_pwa),
              Item(IDS_PREFERENCES)
                  .command_id(IDC_WEB_APP_SETTINGS)
                  .remove_if(!is_pwa),
              Item().is_separator(),
              Item(IDS_CLEAR_BROWSING_DATA)
                  .command_id(IDC_CLEAR_BROWSING_DATA)
                  .remove_if(is_pwa),
              Item(IDS_IMPORT_SETTINGS_MENU_MAC)
                  .command_id(IDC_IMPORT_SETTINGS)
                  .remove_if(is_pwa),
              Item().is_separator(),
              Item(IDS_SERVICES_MAC)
                  .tag(-1)
                  .submenu({}),
              Item().is_separator(),
              Item(IDS_HIDE_APP_MAC)
                  .string_format_1(product_name)
                  .tag(IDC_HIDE_APP)
                  .action(@selector(hide:)),
              Item(IDS_HIDE_OTHERS_MAC)
                  .action(@selector(hideOtherApplications:))
                  .key_equivalent(@"h", NSEventModifierFlagCommand |
                                            NSEventModifierFlagOption),
              Item(IDS_SHOW_ALL_MAC)
                  .action(@selector(unhideAllApplications:)),
              Item().is_separator(),
              Item(IDS_CONFIRM_TO_QUIT_OPTION)
                  .target(app_delegate)
                  .action(@selector(toggleConfirmToQuit:))
                  .remove_if(is_pwa),
              Item().is_separator(),
              Item(IDS_EXIT_MAC)
                  .string_format_1(product_name)
                  .tag(IDC_EXIT)
                  .target(nsapp)
                  .action(@selector(terminate:)),
          })
          .Build();
  // clang-format on

  NSMenuItem* services_item = [item.submenu itemWithTag:-1];
  services_item.tag = 0;

  nsapp.servicesMenu = services_item.submenu;

  return item;
}

NSMenuItem* BuildFileMenu(NSApplication* nsapp,
                          id app_delegate,
                          const std::u16string& product_name,
                          bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      Item(IDS_FILE_MENU_MAC)
          .tag(IDC_FILE_MENU)
          .submenu({
              Item(IDS_NEW_TAB_MAC)
                  .command_id(IDC_NEW_TAB)
                  .remove_if(is_pwa),
              Item(IDS_NEW_WINDOW_MAC)
                  .command_id(IDC_NEW_WINDOW),
              Item(IDS_NEW_INCOGNITO_WINDOW_MAC)
                  .command_id(IDC_NEW_INCOGNITO_WINDOW)
                  .remove_if(is_pwa),
              Item(IDS_REOPEN_CLOSED_TABS_MAC)
                  .command_id(IDC_RESTORE_TAB)
                  .remove_if(is_pwa),
              Item(IDS_OPEN_FILE_MAC)
                  .command_id(IDC_OPEN_FILE)
                  .remove_if(is_pwa),
              Item(IDS_OPEN_LOCATION_MAC)
                  .command_id(IDC_FOCUS_LOCATION)
                  .remove_if(is_pwa),
              Item().is_separator(),
              Item(IDS_CLOSE_WINDOW_MAC)
                  .tag(IDC_CLOSE_WINDOW)
                  .action(@selector(performClose:)),
              Item(IDS_CLOSE_ALL_WINDOWS_MAC)
                  .action(@selector(closeAll:))
                  .is_alternate()
                  .key_equivalent(@"W", NSEventModifierFlagCommand |
                                            NSEventModifierFlagOption),
              Item(IDS_CLOSE_TAB_MAC)
                  .command_id(IDC_CLOSE_TAB)
                  .remove_if(is_pwa),
              Item(IDS_SAVE_PAGE_MAC)
                  .command_id(IDC_SAVE_PAGE),
              Item().is_separator()
                  .remove_if(is_pwa),
              Item(IDS_SHARE_MAC)
                  .remove_if(is_pwa),
              Item().is_separator(),
              Item(IDS_PRINT)
                  .command_id(IDC_PRINT),
              Item(IDS_PRINT_USING_SYSTEM_DIALOG_MAC)
                  .command_id(IDC_BASIC_PRINT)
                  .is_alternate()
                  .remove_if(is_pwa),
          })
          .Build();
  // clang-format on

  return item;
}

NSMenuItem* BuildEditMenu(NSApplication* nsapp,
                          id app_delegate,
                          const std::u16string& product_name,
                          bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      Item(IDS_EDIT_MENU_MAC)
          .tag(IDC_EDIT_MENU)
          .submenu({
              Item(IDS_EDIT_UNDO_MAC)
                  .tag(IDC_CONTENT_CONTEXT_UNDO)
                  .action(@selector(undo:)),
              Item(IDS_EDIT_REDO_MAC)
                  .tag(IDC_CONTENT_CONTEXT_REDO)
                  .action(@selector(redo:)),
              Item().is_separator(),
              Item(IDS_CUT_MAC)
                  .tag(IDC_CONTENT_CONTEXT_CUT)
                  .action(@selector(cut:)),
              Item(IDS_COPY_MAC)
                  .tag(IDC_CONTENT_CONTEXT_COPY)
                  .action(@selector(copy:)),
              Item(IDS_PASTE_MAC)
                  .tag(IDC_CONTENT_CONTEXT_PASTE)
                  .action(@selector(paste:)),
              Item(IDS_PASTE_MATCH_STYLE_MAC)
                  .tag(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE)
                  .action(@selector(pasteAndMatchStyle:)),
              Item(IDS_PASTE_MATCH_STYLE_MAC)
                  .action(@selector(pasteAndMatchStyle:))
                  .is_alternate()
                  .key_equivalent(@"V", NSEventModifierFlagCommand |
                                            NSEventModifierFlagOption),
              Item(IDS_EDIT_DELETE_MAC)
                  .tag(IDC_CONTENT_CONTEXT_DELETE)
                  .action(@selector(delete:)),
              Item(IDS_EDIT_SELECT_ALL_MAC)
                  .tag(IDC_CONTENT_CONTEXT_SELECTALL)
                  .action(@selector(selectAll:)),
              Item().is_separator(),
              Item(IDS_EDIT_FIND_SUBMENU_MAC)
                  .tag(IDC_FIND_MENU)
                  .submenu({
                      Item(IDS_EDIT_SEARCH_WEB_MAC)
                          .command_id(IDC_FOCUS_SEARCH),
                      Item().is_separator(),
                      Item(IDS_EDIT_FIND_MAC)
                          .command_id(IDC_FIND),
                      Item(IDS_EDIT_FIND_NEXT_MAC)
                          .command_id(IDC_FIND_NEXT),
                      Item(IDS_EDIT_FIND_PREVIOUS_MAC)
                          .command_id(IDC_FIND_PREVIOUS),
                      Item(IDS_EDIT_USE_SELECTION_MAC)
                          .action(@selector(copyToFindPboard:))
                          .key_equivalent(@"e", NSEventModifierFlagCommand),
                      Item(IDS_EDIT_JUMP_TO_SELECTION_MAC)
                          .action(@selector(centerSelectionInVisibleArea:))
                          .key_equivalent(@"j", NSEventModifierFlagCommand),
              }),
              Item(IDS_EDIT_SPELLING_GRAMMAR_MAC)
                  .tag(IDC_SPELLCHECK_MENU)
                  .submenu({
                      Item(IDS_EDIT_SHOW_SPELLING_GRAMMAR_MAC)
                          .action(@selector(showGuessPanel:))
                          .key_equivalent(@":", NSEventModifierFlagCommand),
                      Item(IDS_EDIT_CHECK_DOCUMENT_MAC)
                          .action(@selector(checkSpelling:))
                          .key_equivalent(@";", NSEventModifierFlagCommand),
                      Item(IDS_EDIT_CHECK_SPELLING_TYPING_MAC)
                          .action(@selector
                                  (toggleContinuousSpellChecking:)),
                      Item(IDS_EDIT_CHECK_GRAMMAR_MAC)
                          .action(@selector(toggleGrammarChecking:)),
                  }),
              Item(IDS_EDIT_SUBSTITUTIONS_MAC)
                  .submenu({
                      Item(IDS_EDIT_SHOW_SUBSTITUTIONS_MAC)
                          .action(@selector(orderFrontSubstitutionsPanel:)),
                      Item().is_separator(),
                      Item(IDS_EDIT_SMART_QUOTES_MAC)
                          .action(@selector(toggleAutomaticQuoteSubstitution:)),
                      Item(IDS_EDIT_SMART_DASHES_MAC)
                          .action(@selector(toggleAutomaticDashSubstitution:)),
                      Item(IDS_EDIT_TEXT_REPLACEMENT_MAC)
                          .action(@selector(toggleAutomaticTextReplacement:)),
              }),
              Item(IDS_EDIT_TRANSFORMATIONS_MAC)
                  .submenu({
                      Item(IDS_EDIT_MAKE_UPPERCASE_MAC)
                          .action(@selector(uppercaseWord:)),
                      Item(IDS_EDIT_MAKE_LOWERCASE_MAC)
                          .action(@selector(lowercaseWord:)),
                      Item(IDS_EDIT_CAPITALIZE_MAC)
                          .action(@selector(capitalizeWord:)),
              }),
              Item(IDS_SPEECH_MAC)
                  .submenu({
                      Item(IDS_SPEECH_START_SPEAKING_MAC)
                          .action(@selector(startSpeaking:)),
                      Item(IDS_SPEECH_STOP_SPEAKING_MAC)
                          .action(@selector(stopSpeaking:)),
              }),
            // The "Start Dictation..." and "Emoji & Symbols" items are
            // inserted by AppKit.
          })
          .Build();
  // clang-format on
  return item;
}

NSMenuItem* BuildViewMenu(NSApplication* nsapp,
                          id app_delegate,
                          const std::u16string& product_name,
                          bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      Item(IDS_VIEW_MENU_MAC)
          .tag(IDC_VIEW_MENU)
          .submenu({
              Item(IDS_BOOKMARK_BAR_ALWAYS_SHOW_MAC)
                  .command_id(IDC_SHOW_BOOKMARK_BAR)
                  .remove_if(is_pwa),
              Item(IDS_TOGGLE_FULLSCREEN_TOOLBAR_MAC)
                  .command_id(IDC_TOGGLE_FULLSCREEN_TOOLBAR),
              Item(IDS_CONTEXT_MENU_SHOW_FULL_URLS)
                  .command_id(IDC_SHOW_FULL_URLS),
              Item(IDS_CONTEXT_MENU_SHOW_GOOGLE_LENS_SHORTCUT)
                  .command_id(IDC_SHOW_GOOGLE_LENS_SHORTCUT),
              Item(IDS_CUSTOMIZE_TOUCH_BAR)
                  .tag(IDC_CUSTOMIZE_TOUCH_BAR)
                  .action(@selector(toggleTouchBarCustomizationPalette:))
                  .remove_if(is_pwa),
              Item().is_separator(),
              Item(IDS_STOP_MENU_MAC)
                  .command_id(IDC_STOP),
              Item(IDS_RELOAD_MENU_MAC)
                  .command_id(IDC_RELOAD),
              Item(IDS_RELOAD_BYPASSING_CACHE_MENU_MAC)
                  .command_id(IDC_RELOAD_BYPASSING_CACHE)
                  .is_alternate(),
              Item().is_separator(),
              Item(IDS_ENTER_FULLSCREEN_MAC)
                  .tag(IDC_FULLSCREEN)
                  .action(@selector(toggleFullScreen:)),
              Item(IDS_ENTER_FULLSCREEN_MAC)
                  .action(@selector(toggleFullScreen:))
                  .is_alternate()
                  .key_equivalent(@"f", NSEventModifierFlagCommand |
                                            NSEventModifierFlagControl),
              Item(IDS_TEXT_DEFAULT_MAC)
                  .command_id(IDC_ZOOM_NORMAL),
              Item(IDS_TEXT_BIGGER_MAC)
                  .command_id(IDC_ZOOM_PLUS),
              Item(IDS_TEXT_SMALLER_MAC)
                  .command_id(IDC_ZOOM_MINUS),
              Item().is_separator(),
              Item(IDS_MEDIA_ROUTER_MENU_ITEM_TITLE)
                  .command_id(IDC_ROUTE_MEDIA),
              Item().is_separator(),
              Item(IDS_DEVELOPER_MENU_MAC)
                  .tag(IDC_DEVELOPER_MENU)
                  .submenu({
                      Item(IDS_VIEW_SOURCE_MAC)
                          .command_id(IDC_VIEW_SOURCE),
                      Item(IDS_DEV_TOOLS_MAC)
                          .command_id(IDC_DEV_TOOLS),
                      Item(IDS_DEV_TOOLS_ELEMENTS_MAC)
                          .command_id(IDC_DEV_TOOLS_INSPECT),
                      Item(IDS_DEV_TOOLS_CONSOLE_MAC)
                          .command_id(IDC_DEV_TOOLS_CONSOLE),
                      Item(IDS_ALLOW_JAVASCRIPT_APPLE_EVENTS_MAC)
                          .command_id(IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS),
                  }),
        })
        .Build();
  // clang-format on
  return item;
}

NSMenuItem* BuildHistoryMenu(NSApplication* nsapp,
                             id app_delegate,
                             const std::u16string& product_name,
                             bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      Item(IDS_HISTORY_MENU_MAC)
          .tag(IDC_HISTORY_MENU)
          .submenu({
              Item(IDS_HISTORY_HOME_MAC)
                  .command_id(IDC_HOME)
                  .remove_if(is_pwa),
              Item(IDS_HISTORY_BACK_MAC)
                  .command_id(IDC_BACK),
              Item(IDS_HISTORY_FORWARD_MAC)
                  .command_id(IDC_FORWARD),
              Item().is_separator()
                  .tag(HistoryMenuBridge::kRecentlyClosedSeparator)
                  .remove_if(is_pwa),
              Item(IDS_HISTORY_CLOSED_MAC)
                  .tag(HistoryMenuBridge::kRecentlyClosedTitle)
                  .is_section_header()
                  .remove_if(is_pwa),
              Item().is_separator()
                  .tag(HistoryMenuBridge::kVisitedSeparator)
                  .remove_if(is_pwa),
              Item(IDS_HISTORY_VISITED_MAC)
                  .tag(HistoryMenuBridge::kVisitedTitle)
                  .is_section_header()
                  .remove_if(is_pwa),
              Item().is_separator()
                  .tag(HistoryMenuBridge::kShowFullSeparator)
                  .remove_if(is_pwa),
              Item(IDS_HISTORY_SHOWFULLHISTORY_LINK)
                  .command_id(IDC_SHOW_HISTORY)
                  .remove_if(is_pwa),
          })
          .Build();
  // clang-format on
  return item;
}

NSMenuItem* BuildBookmarksMenu(NSApplication* nsapp,
                               id app_delegate,
                               const std::u16string& product_name,
                               bool is_pwa) {
  if (is_pwa) {
    return nil;
  }

  // clang-format off
  NSMenuItem* item =
      Item(IDS_BOOKMARKS_MENU)
          .tag(IDC_BOOKMARKS_MENU)
          .submenu({
              Item(IDS_BOOKMARK_MANAGER)
                  .command_id(IDC_SHOW_BOOKMARK_MANAGER),
              Item().is_separator()
                  .tag(IDC_BOOKMARK_THIS_TAB),
              Item(IDS_BOOKMARK_THIS_TAB)
                  .command_id(IDC_BOOKMARK_THIS_TAB),
              Item(IDS_BOOKMARK_ALL_TABS)
                  .command_id(IDC_BOOKMARK_ALL_TABS),
              Item().is_separator()
                  .tag(IDC_BOOKMARK_THIS_TAB),
          })
          .Build();
  // clang-format on
  return item;
}

NSMenuItem* BuildPeopleMenu(NSApplication* nsapp,
                            id app_delegate,
                            const std::u16string& product_name,
                            bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      Item(IDS_PROFILES_MENU_NAME)
          .tag(IDC_PROFILE_MAIN_MENU)
          .submenu({})
          .Build();
  // clang-format on
  return item;
}

NSMenuItem* BuildWindowMenu(NSApplication* nsapp,
                            id app_delegate,
                            const std::u16string& product_name,
                            bool is_pwa) {
  // clang-format off
  NSMenuItem* item =
      Item(IDS_WINDOW_MENU_MAC)
          .tag(IDC_WINDOW_MENU)
          .submenu({
              Item(IDS_MINIMIZE_WINDOW_MAC)
                  .tag(IDC_MINIMIZE_WINDOW)
                  .action(@selector(performMiniaturize:)),
              Item(IDS_ZOOM_WINDOW_MAC)
                  .tag(IDC_MAXIMIZE_WINDOW)
                  .action(@selector(performZoom:)),
              Item().is_separator(),
              Item(IDS_SHOW_AS_TAB)
                  .command_id(IDC_SHOW_AS_TAB)
                  .remove_if(is_pwa),
              Item(IDS_NAME_WINDOW)
                  .command_id(IDC_NAME_WINDOW)
                  .remove_if(is_pwa),
              Item().is_separator()
                  .remove_if(is_pwa),
              Item(IDS_SHOW_DOWNLOADS_MAC)
                  .command_id(IDC_SHOW_DOWNLOADS)
                  .remove_if(is_pwa),
              Item(IDS_SHOW_EXTENSIONS_MAC)
                  .command_id(IDC_MANAGE_EXTENSIONS)
                  .remove_if(is_pwa),
              Item(IDS_TASK_MANAGER_MAC)
                  .command_id(IDC_TASK_MANAGER)
                  .remove_if(is_pwa),
              Item().is_separator()
                  .remove_if(is_pwa),
              Item(IDS_ALL_WINDOWS_FRONT_MAC)
                  .tag(IDC_ALL_WINDOWS_FRONT)
                  .action(@selector(arrangeInFront:)),
              Item().is_separator(),
          })
          .Build();
  // clang-format on
  nsapp.windowsMenu = item.submenu;
  return item;
}

NSMenuItem* BuildTabMenu(NSApplication* nsapp,
                         id app_delegate,
                         const std::u16string& product_name,
                         bool is_pwa) {
  if (is_pwa) {
    return nil;
  }

  // clang-format off
  NSMenuItem* item =
      Item(IDS_TAB_MENU_MAC)
          .tag(IDC_TAB_MENU)
          .submenu({
              Item(IDS_TAB_CXMENU_NEWTABTORIGHT)
                  .command_id(IDC_NEW_TAB_TO_RIGHT),
              Item(IDS_NEXT_TAB_MAC)
                  .command_id(IDC_SELECT_NEXT_TAB),
              Item(IDS_PREV_TAB_MAC)
                  .command_id(IDC_SELECT_PREVIOUS_TAB),
              Item(IDS_DUPLICATE_TAB_MAC)
                  .command_id(IDC_DUPLICATE_TAB),
              Item(IDS_DUPLICATE_TARGET_TAB_MAC)
                  .command_id(IDC_DUPLICATE_TARGET_TAB)
                  .is_alternate()
                  .key_equivalent(@"", NSEventModifierFlagOption),
              Item(IDS_MUTE_SITE_MAC)
                  .command_id(IDC_WINDOW_MUTE_SITE),
              Item(IDS_MUTE_TARGET_SITE_MAC)
                  .command_id(IDC_MUTE_TARGET_SITE)
                  .is_alternate()
                  .key_equivalent(@"", NSEventModifierFlagOption),
              Item(IDS_PIN_TAB_MAC)
                  .command_id(IDC_WINDOW_PIN_TAB),
              Item(IDS_PIN_TARGET_TAB_MAC)
                  .command_id(IDC_PIN_TARGET_TAB)
                  .is_alternate()
                  .key_equivalent(@"", NSEventModifierFlagOption),
              Item(IDS_GROUP_TAB_MAC)
                  .command_id(IDC_WINDOW_GROUP_TAB),
              Item(IDS_GROUP_TARGET_TAB_MAC)
                  .command_id(IDC_GROUP_TARGET_TAB)
                  .is_alternate()
                  .key_equivalent(@"", NSEventModifierFlagOption),
              Item(IDS_TAB_CXMENU_CLOSEOTHERTABS)
                  .command_id(IDC_WINDOW_CLOSE_OTHER_TABS),
              Item(IDS_TAB_CXMENU_CLOSETABSTORIGHT)
                  .command_id(IDC_WINDOW_CLOSE_TABS_TO_RIGHT),
              Item(IDS_MOVE_TAB_TO_NEW_WINDOW)
                  .command_id(IDC_MOVE_TAB_TO_NEW_WINDOW),
              Item(IDS_SEARCH_TABS)
                  .command_id(IDC_TAB_SEARCH),
              Item().is_separator(),
          })
          .Build();
  // clang-format on
  return item;
}

NSMenuItem* BuildHelpMenu(NSApplication* nsapp,
                          id app_delegate,
                          const std::u16string& product_name,
                          bool is_pwa) {
  if (is_pwa) {
    return nil;
  }

  // clang-format off
  NSMenuItem* item =
      Item(IDS_HELP_MENU_MAC)
          .submenu({
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
              Item(IDS_FEEDBACK_MAC)
                  .command_id(IDC_FEEDBACK),
#endif
              Item(IDS_HELP_MAC)
                  .string_format_1(product_name)
                  .command_id(IDC_HELP_PAGE_VIA_MENU),
          })
          .Build();
  // clang-format on

  nsapp.helpMenu = item.submenu;
  return item;
}

}  // namespace

NSMenu* BuildMainMenu(NSApplication* nsapp,
                      id<NSApplicationDelegate> app_delegate,
                      const std::u16string& product_name,
                      bool is_pwa) {
  AcceleratorsCocoa::CreateForPWA(is_pwa);

  NSMenu* main_menu = [[NSMenu alloc] initWithTitle:@""];
  for (auto* builder : {
           &BuildAppMenu,
           &BuildFileMenu,
           &BuildEditMenu,
           &BuildViewMenu,
           &BuildHistoryMenu,
           &BuildBookmarksMenu,
           &BuildPeopleMenu,
           &BuildTabMenu,
           &BuildWindowMenu,
           &BuildHelpMenu,
       }) {
    auto item = builder(nsapp, app_delegate, product_name, is_pwa);
    if (item) {
      [main_menu addItem:item];
    }
  }

  nsapp.mainMenu = main_menu;

  return main_menu;
}

NSMenuItem* BuildFileMenuForTesting(bool is_pwa) {
  NSMenu* mainMenu = BuildMainMenu(nil, nil, u"", is_pwa);

  // First is the App menu, then the File menu.
  const int kFileMenuItemIndex = 1;

  return [mainMenu itemArray][kFileMenuItemIndex];
}

namespace internal {

MenuItemBuilder::MenuItemBuilder(int string_id) : string_id_(string_id) {}

MenuItemBuilder::MenuItemBuilder(const MenuItemBuilder&) = default;

MenuItemBuilder& MenuItemBuilder::operator=(const MenuItemBuilder&) = default;

MenuItemBuilder::~MenuItemBuilder() = default;

NSMenuItem* MenuItemBuilder::Build() const {
  if (is_removed_) {
    return nil;
  }

  if (is_separator_) {
    NSMenuItem* item = [NSMenuItem separatorItem];
    if (tag_) {
      item.tag = tag_;
    }
    item.hidden = is_hidden_;
    return item;
  }

  NSString* title;
  if (!string_arg1_.empty()) {
    title = l10n_util::GetNSStringFWithFixup(string_id_, string_arg1_);
  } else {
    title = l10n_util::GetNSStringWithFixup(string_id_);
  }

  if (is_section_header_) {
    NSMenuItem* item;
    if (@available(macOS 14, *)) {
      item = [NSMenuItem sectionHeaderWithTitle:title];
    } else {
      item = [[NSMenuItem alloc] initWithTitle:title
                                        action:nil
                                 keyEquivalent:@""];
      item.enabled = NO;
    }
    if (tag_) {
      item.tag = tag_;
    }
    item.hidden = is_hidden_;
    return item;
  }

  // If the item is command-dispatched, look up the relevant key equivalent
  // from the accelerator table. Otherwise, use the builder-specified key
  // equivalent.
  NSString* key_equivalent = key_equivalent_;
  NSEventModifierFlags key_equivalent_flags = key_equivalent_flags_;
  if (tag_ != 0) {
    if (const ui::Accelerator* accelerator =
            AcceleratorsCocoa::GetInstance()->GetAcceleratorForCommand(tag_)) {
      KeyEquivalentAndModifierMask* equivalent =
          GetKeyEquivalentAndModifierMaskFromAccelerator(*accelerator);
      key_equivalent = equivalent.keyEquivalent;
      key_equivalent_flags = equivalent.modifierMask;
    }
  }

  SEL action = !submenu_.has_value() ? action_ : nil;

  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                action:action
                                         keyEquivalent:key_equivalent];
  item.target = target_;
  item.tag = tag_;
  item.keyEquivalentModifierMask = key_equivalent_flags;
  item.alternate = is_alternate_;
  item.hidden = is_hidden_;

  if (submenu_.has_value()) {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:title];
    for (const auto& subitem : submenu_.value()) {
      NSMenuItem* ns_subitem = subitem.Build();
      if (ns_subitem) {
        [menu addItem:ns_subitem];
      }
    }
    item.submenu = menu;
  }

  return item;
}

}  // namespace internal
}  // namespace chrome
