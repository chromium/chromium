// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_context_menu_mixin.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/send_tab_to_self/features.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/touch_selection/touch_editing_controller.h"
#include "ui/views/controls/textfield/textfield.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/appkit_utils.h"
#endif

namespace {

bool IsClipboardDataMarkedAsConfidential() {
  return ui::Clipboard::GetForCurrentThread()
      ->IsMarkedByOriginatorAsConfidential();
}

}  // namespace

OmniboxContextMenuMixinBase::OmniboxContextMenuMixinBase(
    LocationBar* location_bar,
    OmniboxController* controller)
    : location_bar_(location_bar), controller_(controller) {}

OmniboxContextMenuMixinBase::~OmniboxContextMenuMixinBase() = default;

bool OmniboxContextMenuMixinBase::HandleIsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == IDC_PASTE_AND_GO;
}

std::u16string OmniboxContextMenuMixinBase::HandleGetLabelForCommandId(
    int command_id) const {
  DCHECK_EQ(IDC_PASTE_AND_GO, command_id);

  // If the originator marked the clipboard data as confidential, then
  // paste-and-go is unavailable, so use a menu label that doesn't contain
  // clipboard data. (The menu command is disabled in
  // `OmniboxViewViews::IsCommandIdEnabled()`.)
  //
  // On the Mac, if Pasteboard Privacy is enabled, then programmatic access to
  // the clipboard is either prohibited or will prompt the user, and we can't
  // inline the contents of the clipboard into the label.
  //
  // If we were to attempt to access the clipboard contents to inline it into
  // the label, the result would be a glitched out user window (see the
  // screenshot attached to https://crbug.com/417683820#comment3). That's super
  // bad.
  //
  // Therefore, take the less bad approach as done below, where if accessing the
  // clipboard could block, we just turn "paste and go" into a generic menu
  // item.
  //
  // The best approach would actually be to use -[NSPasteboard
  // detectPatternsForPatterns:completionHandler:] to select a specific menu
  // string that matches what's on the clipboard, in order to convey to the user
  // what will happen. The usage of `/components/open_from_clipboard` might be
  // useful. This behavior should be patterned after what Chrome iOS does, which
  // has to work under similar restrictions. TODO(https://crbug.com/419266152):
  // Switch to this better approach.
  if (IsClipboardDataMarkedAsConfidential()
#if BUILDFLAG(IS_MAC)
      || ui::PasteMightBlockWithPrivacyAlert()
#endif
  )
    return l10n_util::GetStringUTF16(IDS_PASTE_AND_GO_EMPTY);

  if (clipboard_text_for_menu_.empty()) {
    return l10n_util::GetStringUTF16(IDS_PASTE_AND_GO_EMPTY);
  }

  constexpr size_t kMaxSelectionTextLength = 50;
  std::u16string selection_text = gfx::TruncateString(
      clipboard_text_for_menu_, kMaxSelectionTextLength, gfx::WORD_BREAK);

  AutocompleteMatch match;
  controller_->edit_model()->ClassifyString(clipboard_text_for_menu_, &match,
                                            nullptr);
  if (AutocompleteMatch::IsSearchType(match.type)) {
    return l10n_util::GetStringFUTF16(IDS_PASTE_AND_SEARCH, selection_text);
  }

  // To ensure the search and url strings began to truncate at the exact same
  // number of characters, the pixel width at which the url begins to elide is
  // derived from the truncated selection text. However, ideally there would be
  // a better way to do this.
  const auto& font_list = FontListForContextMenu();
  const float kMaxSelectionPixelWidth =
      GetStringWidthF(selection_text, font_list);
  const std::u16string url = url_formatter::ElideUrl(
      match.destination_url, font_list, kMaxSelectionPixelWidth);

  return l10n_util::GetStringFUTF16(IDS_PASTE_AND_GO, url);
}

bool OmniboxContextMenuMixinBase::HandleIsCommandIdEnabled(
    int command_id) const {
  if (command_id ==
      std::to_underlying(ui::TouchEditable::MenuCommands::kPaste)) {
    return !IsContextMenuForReadOnlyOmnibox() &&
           !clipboard_text_for_menu_.empty();
  }
  if (command_id == IDC_PASTE_AND_GO) {
    if (IsContextMenuForReadOnlyOmnibox()) {
      return false;
    }

    // If the originator marked the clipboard data as confidential, then
    // paste-and-go is unavailable, so disable the menu command. (The menu label
    // is set to be generic in `GetLabelForCommandId()`.)
    if (IsClipboardDataMarkedAsConfidential()) {
      return false;
    }

#if BUILDFLAG(IS_MAC)
    // On the Mac, if Pasteboard Privacy is enabled, then programmatic access to
    // the clipboard is either prohibited or will prompt the user, and we can't
    // use the actual clipboard text to make decisions about enabling the menu
    // command.
    //
    // Therefore, for now, go with a general check for if there is a
    // probably-valid item on the clipboard to use for paste-and-go, with a
    // cheat of using a constant string to ensure that all the other
    // requirements for paste-and-go are fulfilled.
    //
    // TODO(https://crbug.com/419266152): Switch to a better approach of using
    // -[NSPasteboard detectPatternsForPatterns:completionHandler:] to actually
    // know if there are valid values on the clipboard to enable paste-and-go
    // with confidence.
    if (ui::PasteMightBlockWithPrivacyAlert()) {
      if (!clipboard_text_for_menu_.empty()) {
        constexpr char16_t kSomeValidText[] = u"validtext";
        return controller_->edit_model()->CanPasteAndGo(kSomeValidText);
      } else {
        return false;
      }
    }
#endif

    return controller_->edit_model()->CanPasteAndGo(clipboard_text_for_menu_);
  }

  // These menu items are only shown when they are valid.
  if (command_id == IDC_SHOW_FULL_URLS ||
      command_id == IDC_SHOW_GOOGLE_LENS_SHORTCUT ||
      command_id == IDC_SHOW_AI_MODE_OMNIBOX_BUTTON ||
      command_id == IDC_SHOW_SEARCH_TOOLS) {
    return true;
  }

  return IsContextMenuTextEditingCommandEnabled(command_id) ||
         (location_bar_ &&
          location_bar_->command_updater()->IsCommandEnabled(command_id));
}

bool OmniboxContextMenuMixinBase::HandleExecuteCommand(int command_id,
                                                       int event_flags) {
  switch (command_id) {
    // These commands DO NOT invoke the popup via
    // OnBefore/AfterPossibleChange().
    case IDC_PASTE_AND_GO:
      GetClipboardText(
          /*notify_if_restricted=*/true,
          base::BindOnce(
              [](base::WeakPtr<OmniboxContextMenuMixinBase> self,
                 std::u16string text) {
                if (self && self->controller_) {
                  self->controller_->edit_model()->PasteAndGo(text);
                }
              },
              weak_ptr_factory_.GetWeakPtr()));
      return true;

    case IDC_EDIT_SEARCH_ENGINES:
    case IDC_SHOW_FULL_URLS:
    case IDC_SHOW_GOOGLE_LENS_SHORTCUT:
    case IDC_SHOW_AI_MODE_OMNIBOX_BUTTON:
    case IDC_SHOW_SEARCH_TOOLS:
      if (location_bar_) {
        location_bar_->command_updater()->ExecuteCommand(command_id);
      }
      return true;

    case IDC_SEND_TAB_TO_SELF:
      if (location_bar_ && location_bar_->GetWebContents()) {
        send_tab_to_self::SendTabToSelfBubbleController::
            GetOrCreateForWebContents(location_bar_->GetWebContents())
                ->ShowBubble(send_tab_to_self::ShareEntryPoint::kOmniboxMenu);
      }
      return true;

    default:
      return false;
  }
}

void OmniboxContextMenuMixinBase::PrepareToShowContextMenu(
    base::OnceClosure closure) {
  GetClipboardText(
      /*notify_if_restricted=*/false,
      base::BindOnce(&OmniboxContextMenuMixinBase::OnGotClipboardText,
                     weak_ptr_factory_.GetWeakPtr(), std::move(closure)));
}

void OmniboxContextMenuMixinBase::OnGotClipboardText(base::OnceClosure closure,
                                                     std::u16string text) {
  clipboard_text_for_menu_ = std::move(text);
  std::move(closure).Run();
}

void OmniboxContextMenuMixinBase::AddOmniboxSpecificItems(
    ui::SimpleMenuModel* menu_contents) {
  MaybeAddSendTabToSelfItem(menu_contents);

  const std::optional<size_t> paste_position =
      menu_contents->GetIndexOfCommandId(
          std::to_underlying(ui::TouchEditable::MenuCommands::kPaste));
  DCHECK(paste_position.has_value());
  menu_contents->InsertItemWithStringIdAt(paste_position.value() + 1,
                                          IDC_PASTE_AND_GO, IDS_PASTE_AND_GO);

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  menu_contents->AddItemWithStringId(
      IDC_EDIT_SEARCH_ENGINES,
      base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate)
          ? IDS_MANAGE_SEARCH_ENGINES_AND_SHORTCUTS
          : IDS_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH);

  if (features::IsMenuSimplificationEnabled()) {
    menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  const PrefService::Preference* show_full_urls_pref =
      location_bar_->GetProfile()->GetPrefs()->FindPreference(
          omnibox::kPreventUrlElisionsInOmnibox);
  if (!show_full_urls_pref->IsManaged()) {
    menu_contents->AddCheckItemWithStringId(IDC_SHOW_FULL_URLS,
                                            IDS_CONTEXT_MENU_SHOW_FULL_URLS);
  }

  // Location bar is also used in non-browser UI in production environment.
  // The only known case so far is simple_web_view_dialog for ChromeOS to draw
  // captive portal during OOBE signin. Null check to avoid crash before these
  // UIs are migrated away. See crbug.com/379534750 for a production crash
  // example. There is an effort to move simple_web_view_dialog away from
  // location_bar_view and from this nullptr situation.
  if (lens::features::IsOmniboxEntryPointEnabled() &&
      location_bar_->GetBrowser()) {
    if (auto* controller = lens::LensOverlayEntryPointController::From(
            location_bar_->GetBrowser());
        controller && controller->IsEnabled()) {
      menu_contents->AddCheckItemWithStringId(
          IDC_SHOW_GOOGLE_LENS_SHORTCUT,
          IDS_CONTEXT_MENU_SHOW_GOOGLE_LENS_SHORTCUT);
    }
  }

  if (omnibox::ShouldShowAimContextMenuOption(location_bar_->GetProfile())) {
    auto* config = GetAiModeConfig();
    if (config) {
      menu_contents->AddCheckItem(IDC_SHOW_AI_MODE_OMNIBOX_BUTTON,
                                  config->context_menu_label);
    }
  }

  if (omnibox_feature_configs::Toolbelt::Get().enabled) {
    menu_contents->AddCheckItemWithStringId(IDC_SHOW_SEARCH_TOOLS,
                                            IDS_CONTEXT_MENU_SHOW_SEARCH_TOOLS);
  }
}

bool OmniboxContextMenuMixinBase::HandleIsCommandIdChecked(int id) const {
  if (id == IDC_SHOW_FULL_URLS) {
    return location_bar_->GetProfile()->GetPrefs()->GetBoolean(
        omnibox::kPreventUrlElisionsInOmnibox);
  }
  if (id == IDC_SHOW_GOOGLE_LENS_SHORTCUT) {
    return location_bar_->GetProfile()->GetPrefs()->GetBoolean(
        omnibox::kShowGoogleLensShortcut);
  }
  if (id == IDC_SHOW_SEARCH_TOOLS) {
    return location_bar_->GetProfile()->GetPrefs()->GetBoolean(
        omnibox::kShowSearchTools);
  }
  if (id == IDC_SHOW_AI_MODE_OMNIBOX_BUTTON) {
    return location_bar_->GetProfile()->GetPrefs()->GetBoolean(
        omnibox::kShowAiModeOmniboxButton);
  }
  return false;
}

const ai_mode_button_config::AiModeButtonConfig*
OmniboxContextMenuMixinBase::GetAiModeConfig() const {
  if (!location_bar_) {
    return nullptr;
  }
  auto* service =
      AiModeButtonServiceFactory::GetForProfile(location_bar_->GetProfile());
  return service ? service->GetCurrentConfig() : nullptr;
}

void OmniboxContextMenuMixinBase::BuildSendTabToSelfSubmenu(
    ui::SimpleMenuModel* menu_contents,
    size_t index) {
  CHECK(location_bar_);
  send_tab_to_self_submenu_delegate_ =
      std::make_unique<send_tab_to_self::SendTabToSelfContextMenuDelegate>(
          location_bar_->GetWebContents(),
          send_tab_to_self::ShareEntryPoint::kOmniboxMenu);
  send_tab_to_self_submenu_ = std::make_unique<ui::SimpleMenuModel>(
      send_tab_to_self_submenu_delegate_.get());
  send_tab_to_self_submenu_delegate_->PopulateSubmenu(
      send_tab_to_self_submenu_.get());

  menu_contents->InsertSubMenuWithStringIdAt(index, IDC_SEND_TAB_TO_SELF,
                                             IDS_MENU_SEND_TAB_TO_SELF,
                                             send_tab_to_self_submenu_.get());
#if !BUILDFLAG(IS_MAC)
  menu_contents->SetIcon(
      index, ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                                ? kDevicesIcon
                                                : kDevicesOldIcon));
#endif
  menu_contents->SetIsNewFeatureAt(
      index, UserEducationService::MaybeShowNewBadge(
                 location_bar_->GetProfile(),
                 send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2));
}

void OmniboxContextMenuMixinBase::BuildSendTabToSelfSimpleItem(
    ui::SimpleMenuModel* menu_contents,
    size_t index) {
  menu_contents->InsertItemAt(
      index, IDC_SEND_TAB_TO_SELF,
      l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF));
#if !BUILDFLAG(IS_MAC)
  menu_contents->SetIcon(
      index, ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                                ? kDevicesIcon
                                                : kDevicesOldIcon));
#endif
}

void OmniboxContextMenuMixinBase::MaybeAddSendTabToSelfItem(
    ui::SimpleMenuModel* menu_contents) {
  // WebContents is required to evaluate Send Tab to Self availability
  // (such as sync state and target devices) and to provide tab context
  // (URL and title) for the context menu delegate.
  if (!location_bar_ || !location_bar_->GetWebContents()) {
    return;
  }

  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      send_tab_to_self::GetEntryPointDisplayReason(
          location_bar_->GetWebContents());
  if (!reason) {
    return;
  }

  if (features::IsMenuSimplificationEnabled()) {
    return;
  }

  size_t index =
      menu_contents->GetIndexOfCommandId(views::Textfield::kUndo).value();
  // Add a separator if this is not the first item.
  if (index) {
    menu_contents->InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);
  }

  // Build an inline submenu showing target devices if the Omnibox context menu
  // feature is enabled and target devices are available. Otherwise, fallback
  // to a simple command item that triggers the bubble dialog.
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfEnhancedDesktopUIv2) &&
      *reason == send_tab_to_self::EntryPointDisplayReason::kOfferFeature) {
    BuildSendTabToSelfSubmenu(menu_contents, index);
  } else {
    BuildSendTabToSelfSimpleItem(menu_contents, index);
  }

  menu_contents->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
}
