// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_context_menu_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/appkit_utils.h"
#endif

namespace omnibox_everywhere {

namespace {

bool IsFuseboxEligible(Profile* profile) {
  if (!profile) {
    return false;
  }
  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return aim_eligibility_service &&
         aim_eligibility_service->IsFuseboxEligible();
}

}  // namespace

OmniboxEverywhereContextMenuController::OmniboxEverywhereContextMenuController(
    Delegate& delegate)
    : delegate_(delegate) {}

OmniboxEverywhereContextMenuController::
    ~OmniboxEverywhereContextMenuController() = default;

void OmniboxEverywhereContextMenuController::HandleContextMenu(
    const content::ContextMenuParams& params) {
  bool can_read_clipboard =
      params.is_editable && !ui::Clipboard::GetForCurrentThread()
                                 ->IsMarkedByOriginatorAsConfidential();
#if BUILDFLAG(IS_MAC)
  if (ui::PasteMightBlockWithPrivacyAlert()) {
    can_read_clipboard = false;
  }
#endif

  if (can_read_clipboard) {
    GetClipboardText(
        /*notify_if_restricted=*/false,
        base::BindOnce(&OmniboxEverywhereContextMenuController::ShowContextMenu,
                       weak_factory_.GetWeakPtr(), params));
  } else {
    ShowContextMenu(params, /*clipboard_text=*/std::u16string());
  }
}

void OmniboxEverywhereContextMenuController::Cancel() {
  if (is_open_ && context_menu_runner_) {
    context_menu_runner_->Cancel();
    is_open_ = false;
  } else {
    weak_factory_.InvalidateWeakPtrs();
  }
}

void OmniboxEverywhereContextMenuController::Reset() {
  weak_factory_.InvalidateWeakPtrs();
  if (context_menu_runner_) {
    context_menu_runner_->Cancel();
    ReleaseRunner();
  }
  ResetModels();
  last_params_ = content::ContextMenuParams();
  is_open_ = false;
  RunOverflowMenuClosedCallback();
}

void OmniboxEverywhereContextMenuController::ShowOverflowMenu(
    const gfx::Rect& anchor_screen_rect,
    ui::mojom::MenuSourceType source_type,
    base::OnceClosure on_closed) {
  views::Widget* host_widget = widget();
  if (!host_widget || !host_widget->GetContentsView()) {
    if (on_closed) {
      std::move(on_closed).Run();
    }
    return;
  }

  // Cancel and clean up any existing menu before creating a new one.
  if (context_menu_runner_) {
    context_menu_runner_->Cancel();
    ReleaseRunner();
  }
  ResetModels();
  RunOverflowMenuClosedCallback();

  // The overflow menu only contains settings commands, which do not depend on
  // the right-click target, so clear any stale right-click params. This also
  // keeps the editable-only Writing Direction submenu out of this menu.
  last_params_ = content::ContextMenuParams();
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  AppendSettingsContextMenu();
#if BUILDFLAG(IS_WIN)
  AppendWindowControlsContextMenu();
#endif  // BUILDFLAG(IS_WIN)

  overflow_menu_closed_callback_ = std::move(on_closed);
  // In Views the menu is anchored below the button, aligned to its right edge
  // (mirrored automatically in RTL). The native macOS menu ignores the anchor
  // position: when opened by a mouse click it opens at the click location,
  // like a right-click menu, and otherwise (e.g. keyboard activation) at the
  // center of the anchor rect. Pass the button's bottom leading corner so the
  // keyboard case opens below the button.
  gfx::Rect anchor_rect = anchor_screen_rect;
#if BUILDFLAG(IS_MAC)
  anchor_rect =
      gfx::Rect(base::i18n::IsRTL() ? anchor_screen_rect.bottom_right()
                                    : anchor_screen_rect.bottom_left(),
                gfx::Size());
#endif  // BUILDFLAG(IS_MAC)
  RunMenu(host_widget, anchor_rect, views::MenuAnchorPosition::kTopRight,
          source_type);
}

void OmniboxEverywhereContextMenuController::ShowContextMenu(
    const content::ContextMenuParams& params,
    std::u16string clipboard_text) {
  views::Widget* host_widget = widget();
  if (!host_widget || !host_widget->GetContentsView()) {
    return;
  }

  // Cancel and clean up any existing context menu before creating a new one.
  if (context_menu_runner_) {
    context_menu_runner_->Cancel();
    ReleaseRunner();
  }
  ResetModels();
  RunOverflowMenuClosedCallback();

  last_params_ = params;
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  // 1. Query input box / editable text
  if (params.is_editable) {
    BuildInputContextMenu(params, clipboard_text);
    // 2. Highlighted text
  } else if (!params.selection_text.empty()) {
    BuildSelectionContextMenu(params);
    // 3. Non-editable background / anywhere else on the widget
  } else {
    BuildBackgroundContextMenu(params);
  }

#if BUILDFLAG(IS_WIN)
  AppendWindowControlsContextMenu();
#endif  // BUILDFLAG(IS_WIN)

  if (context_menu_model_->GetItemCount() == 0) {
    return;
  }

  gfx::Point screen_point(params.x, params.y);
  views::View::ConvertPointToScreen(host_widget->GetContentsView(),
                                    &screen_point);
  RunMenu(host_widget, gfx::Rect(screen_point, gfx::Size()),
          views::MenuAnchorPosition::kTopLeft, params.source_type);
}

void OmniboxEverywhereContextMenuController::RunMenu(
    views::Widget* host_widget,
    const gfx::Rect& anchor_screen_rect,
    views::MenuAnchorPosition anchor_position,
    ui::mojom::MenuSourceType source_type) {
  is_open_ = true;
  auto on_closed_callback =
      base::BindRepeating(&OmniboxEverywhereContextMenuController::OnMenuClosed,
                          weak_factory_.GetWeakPtr());
  if (menu_runner_factory_) {
    context_menu_runner_ =
        menu_runner_factory_.Run(context_menu_model_.get(), on_closed_callback);
  } else {
    context_menu_runner_ = std::make_unique<views::MenuRunner>(
        context_menu_model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
        on_closed_callback);
  }

  context_menu_runner_->RunMenuAt(host_widget, nullptr, anchor_screen_rect,
                                  anchor_position, source_type);
}

void OmniboxEverywhereContextMenuController::OnMenuClosed() {
  is_open_ = false;
  ReleaseRunner();
  ResetModels();
  RunOverflowMenuClosedCallback();
  delegate_->OnContextMenuClosed();
}

void OmniboxEverywhereContextMenuController::RunOverflowMenuClosedCallback() {
  if (overflow_menu_closed_callback_) {
    std::move(overflow_menu_closed_callback_).Run();
  }
}

// The runner and models are deleted asynchronously because this may be reached
// from within a callback that the runner or the menu model is still executing.
void OmniboxEverywhereContextMenuController::ReleaseRunner() {
  if (context_menu_runner_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_runner_));
  }
}

void OmniboxEverywhereContextMenuController::ResetModels() {
  if (context_menu_model_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(context_menu_model_));
  }
#if BUILDFLAG(IS_MAC)
  if (writing_direction_submenu_model_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(writing_direction_submenu_model_));
  }
#endif  // BUILDFLAG(IS_MAC)
}

void OmniboxEverywhereContextMenuController::BuildInputContextMenu(
    const content::ContextMenuParams& params,
    const std::u16string& clipboard_text) {
  if (ui::IsEmojiPanelSupported()) {
    context_menu_model_->AddItemWithStringId(kEmoji, IDS_CONTENT_CONTEXT_EMOJI);
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  context_menu_model_->AddItemWithStringId(kUndo, IDS_APP_UNDO);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  context_menu_model_->AddItemWithStringId(kCut, IDS_APP_CUT);
  context_menu_model_->AddItemWithStringId(kCopy, IDS_APP_COPY);
  context_menu_model_->AddItemWithStringId(kPaste, IDS_APP_PASTE);
  context_menu_model_->AddItem(kPasteAndSearch,
                               GetPasteAndGoMenuLabel(clipboard_text));
  context_menu_model_->AddItemWithStringId(kDelete, IDS_APP_DELETE);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  context_menu_model_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  AppendSettingsContextMenu();
}

void OmniboxEverywhereContextMenuController::BuildSelectionContextMenu(
    const content::ContextMenuParams& params) {
  if (params.is_editable) {
    context_menu_model_->AddItemWithStringId(kUndo, IDS_APP_UNDO);
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_->AddItemWithStringId(kCut, IDS_APP_CUT);
  }
  context_menu_model_->AddItemWithStringId(kCopy, IDS_APP_COPY);
  if (params.is_editable) {
    context_menu_model_->AddItemWithStringId(kPaste, IDS_APP_PASTE);
    context_menu_model_->AddItemWithStringId(kDelete, IDS_APP_DELETE);
  }
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  context_menu_model_->AddItemWithStringId(kSelectAll, IDS_APP_SELECT_ALL);
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  AppendSettingsContextMenu();
}

void OmniboxEverywhereContextMenuController::BuildBackgroundContextMenu(
    const content::ContextMenuParams& params) {
  AppendSettingsContextMenu();
}

void OmniboxEverywhereContextMenuController::AppendSettingsContextMenu() {
  context_menu_model_->AddItemWithStringId(
      kManageSearchEngines,
      base::FeatureList::IsEnabled(switches::kSearchSettingsUpdate)
          ? IDS_MANAGE_SEARCH_ENGINES_AND_SHORTCUTS
          : IDS_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH);

  if (IsFuseboxEligible(profile())) {
    if (auto* service = AiModeButtonServiceFactory::GetForProfile(profile())) {
      if (const AiModeButtonUiConfig* config = service->GetCurrentConfig()) {
        context_menu_model_->AddCheckItem(kAlwaysShowAiMode,
                                          config->context_menu_label);
      }
    }
  }

#if BUILDFLAG(IS_MAC)
  if (last_params_.is_editable) {
    writing_direction_submenu_model_ =
        std::make_unique<ui::SimpleMenuModel>(this);
    writing_direction_submenu_model_->AddCheckItemWithStringId(
        kWritingDirectionDefault,
        IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT);
    writing_direction_submenu_model_->AddCheckItemWithStringId(
        kWritingDirectionLtr, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR);
    writing_direction_submenu_model_->AddCheckItemWithStringId(
        kWritingDirectionRtl, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL);
    context_menu_model_->AddSubMenuWithStringId(
        kWritingDirectionMenu, IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU,
        writing_direction_submenu_model_.get());
  }
#endif  // BUILDFLAG(IS_MAC)

  // Loomnibox settings.
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  if (prefs::AreShortcutsAvailableForProfile(profile())) {
    context_menu_model_->AddCheckItemWithStringId(
        kShowShortcuts, IDS_SETTINGS_OMNIBOX_EVERYWHERE_SHOW_SHORTCUTS_TITLE);
  }
  context_menu_model_->AddItemWithStringId(
      kCustomizeKeyboardShortcut,
      IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT);
  context_menu_model_->AddItemWithStringId(
      kSettings, IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS);
}

std::u16string OmniboxEverywhereContextMenuController::GetPasteAndGoMenuLabel(
    const std::u16string& clipboard_text) const {
  Profile* current_profile = delegate_->GetContextMenuProfile();
  if (!current_profile ||
      ui::Clipboard::GetForCurrentThread()->IsMarkedByOriginatorAsConfidential()
#if BUILDFLAG(IS_MAC)
      || ui::PasteMightBlockWithPrivacyAlert()
#endif
      || clipboard_text.empty()) {
    return l10n_util::GetStringUTF16(IDS_PASTE_AND_GO_EMPTY);
  }

  constexpr size_t kMaxSelectionTextLength = 50;
  std::u16string selection_text = gfx::TruncateString(
      clipboard_text, kMaxSelectionTextLength, gfx::WORD_BREAK);

  auto* classifier =
      AutocompleteClassifierFactory::GetForProfile(current_profile);
  if (!classifier) {
    return l10n_util::GetStringFUTF16(IDS_PASTE_AND_SEARCH, selection_text);
  }

  AutocompleteMatch match;
  classifier->Classify(clipboard_text, /*in_keyword_mode=*/false,
                       /*allow_exact_keyword_match=*/true,
                       metrics::OmniboxEventProto::OMNIBOX_EVERYWHERE, &match,
                       nullptr);
  if (AutocompleteMatch::IsSearchType(match.type)) {
    return l10n_util::GetStringFUTF16(IDS_PASTE_AND_SEARCH, selection_text);
  }

  const gfx::FontList& font_list = views::MenuConfig::instance().font_list;
  const float max_selection_pixel_width =
      gfx::GetStringWidthF(selection_text, font_list);
  const std::u16string url = url_formatter::ElideUrl(
      match.destination_url, font_list, max_selection_pixel_width);
  return l10n_util::GetStringFUTF16(IDS_PASTE_AND_GO, url);
}

#if BUILDFLAG(IS_WIN)
// Window controls only apply to persistent mode. An ephemeral widget has no
// taskbar entry to minimize to and is already dismissed on deactivation.
void OmniboxEverywhereContextMenuController::AppendWindowControlsContextMenu() {
  if (prefs::IsEphemeralModelEnabled()) {
    return;
  }
  context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  context_menu_model_->AddItemWithStringId(
      kMinimize, IDS_OMNIBOX_EVERYWHERE_CONTEXT_MENU_MINIMIZE);
  context_menu_model_->AddItemWithStringId(
      kClose, IDS_OMNIBOX_EVERYWHERE_CONTEXT_MENU_CLOSE);
}
#endif  // BUILDFLAG(IS_WIN)

// ui::SimpleMenuModel::Delegate:
// Supplies the platform-native emoji picker keyboard shortcut hint displayed
// alongside the Emoji item in the right-click context menu (Win + . on Windows,
// Cmd + Ctrl + Space on macOS).
bool OmniboxEverywhereContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == kEmoji) {
#if BUILDFLAG(IS_WIN)
    *accelerator = ui::Accelerator(ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN);
    return true;
#elif BUILDFLAG(IS_MAC)
    *accelerator = ui::Accelerator(ui::VKEY_SPACE,
                                   ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
    return true;
#else
    return false;
#endif
  }
  return false;
}

// Dispatches standard text editing commands from the context menu to the
// underlying WebContents. Explicitly focuses the WebContents beforehand so
// that focus temporarily acquired by the context menu UI runner is returned
// to the WebContents and its focused frame input handler.
void OmniboxEverywhereContextMenuController::ExecuteCommand(int command_id,
                                                            int event_flags) {
#if BUILDFLAG(IS_WIN)
  // Window controls act on the widget, not the page. Safe to run synchronously:
  // `MenuRunnerImpl` clears `running_` before dispatching, so the re-entrant
  // `Cancel()` in `Close()` is a no-op.
  switch (command_id) {
    case kMinimize:
      delegate_->MinimizeFromContextMenu();
      return;
    case kClose:
      delegate_->CloseFromContextMenu();
      return;
    default:
      break;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (!web_contents()) {
    return;
  }
  web_contents()->Focus();
  switch (command_id) {
    case kEmoji:
      if (views::Widget* host_widget = widget()) {
        host_widget->ShowEmojiPanel();
      }
      break;
    case kUndo:
      web_contents()->Undo();
      break;
    case kCut:
      web_contents()->Cut();
      break;
    case kCopy:
      web_contents()->Copy();
      break;
    case kPaste:
      web_contents()->Paste();
      break;
    case kPasteAndSearch: {
      GetClipboardText(
          /*notify_if_restricted=*/true,
          base::BindOnce(
              [](base::WeakPtr<OmniboxEverywhereContextMenuController> self,
                 std::u16string clipboard_text) {
                if (!self || clipboard_text.empty()) {
                  return;
                }
                Profile* profile = self->profile();
                if (!profile) {
                  return;
                }
                auto* classifier =
                    AutocompleteClassifierFactory::GetForProfile(profile);
                if (!classifier) {
                  return;
                }
                AutocompleteMatch match;
                classifier->Classify(
                    clipboard_text, /*in_keyword_mode=*/false,
                    /*allow_exact_keyword_match=*/true,
                    metrics::OmniboxEventProto::OMNIBOX_EVERYWHERE, &match,
                    nullptr);
                if (match.destination_url.is_valid()) {
                  if (auto* service =
                          OmniboxEverywhereServiceFactory::GetForProfile(
                              profile)) {
                    service->OpenUrl(
                        match.destination_url,
                        ui::mojom::WindowOpenDisposition::kNewForegroundTab,
                        ui::PAGE_TRANSITION_GENERATED);
                  }
                }
              },
              weak_factory_.GetWeakPtr()));
      break;
    }
    case kDelete:
      web_contents()->Delete();
      break;
    case kSelectAll:
      web_contents()->SelectAll();
      break;
    case kManageSearchEngines: {
      if (Profile* current_profile = profile()) {
        if (auto* service = OmniboxEverywhereServiceFactory::GetForProfile(
                current_profile)) {
          service->OpenUrl(
              chrome::GetSettingsUrl(chrome::kSearchEnginesSubPage),
              ui::mojom::WindowOpenDisposition::kNewForegroundTab,
              ui::PAGE_TRANSITION_AUTO_BOOKMARK);
        }
      }
      break;
    }
    case kAlwaysShowAiMode: {
      if (profile() && profile()->GetPrefs()) {
        PrefService* prefs = profile()->GetPrefs();
        prefs->SetBoolean(
            prefs::kOmniboxEverywhereShowAiMode,
            !prefs->GetBoolean(prefs::kOmniboxEverywhereShowAiMode));
      }
      break;
    }
    case kShowShortcuts:
      if (profile() && profile()->GetPrefs()) {
        PrefService* prefs = profile()->GetPrefs();
        const bool is_currently_visible =
            prefs::IsOmniboxEverywhereShortcutsVisible(profile());
        prefs->SetInteger(
            prefs::kOmniboxEverywhereShowShortcuts,
            static_cast<int>(is_currently_visible
                                 ? prefs::ShowShortcutsPrefValue::kDisabled
                                 : prefs::ShowShortcutsPrefValue::kEnabled));
      }
      break;
    // TODO(b/543460015): Differentiate settings and shortcut
    // customize URLs once dedicated deep-link routing / subpage anchors land.
    case kCustomizeKeyboardShortcut:
    case kSettings: {
      if (Profile* current_profile = profile()) {
        if (auto* service = OmniboxEverywhereServiceFactory::GetForProfile(
                current_profile)) {
          service->OpenUrl(chrome::GetSettingsUrl(chrome::kSearchSubPage),
                           ui::mojom::WindowOpenDisposition::kNewForegroundTab,
                           ui::PAGE_TRANSITION_AUTO_BOOKMARK);
        }
      }
      break;
    }
#if BUILDFLAG(IS_MAC)
    case kWritingDirectionDefault:
    case kWritingDirectionLtr:
    case kWritingDirectionRtl: {
      base::i18n::TextDirection direction =
          base::i18n::TextDirection::UNKNOWN_DIRECTION;
      if (command_id == kWritingDirectionRtl) {
        direction = base::i18n::RIGHT_TO_LEFT;
      } else if (command_id == kWritingDirectionLtr) {
        direction = base::i18n::LEFT_TO_RIGHT;
      }
      content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
      if (!rfh) {
        rfh = web_contents()->GetPrimaryMainFrame();
      }
      if (rfh && rfh->IsRenderFrameLive()) {
        rfh->GetRenderWidgetHost()->UpdateTextDirection(direction);
        rfh->GetRenderWidgetHost()->NotifyTextDirection();
      }
      break;
    }
#endif  // BUILDFLAG(IS_MAC)
    default:
      NOTREACHED();
  }
}

// Evaluates whether a context menu command should be enabled.
// Note: When right-clicking an editable element without focusing it first,
// Blink populates `ContextMenuParams::is_editable = true` but may not set
// `ContextMenuDataEditFlags::kCanPaste` in `edit_flags` (as focus controller
// has not yet focused the element). Therefore, Paste is enabled when either
// `kCanPaste` edit flag is set or the element is editable. PasteAndSearch
// requires a valid `Profile` with an active `OmniboxEverywhereService`. Cut /
// Copy check for selected text in addition to Blink edit flags.
bool OmniboxEverywhereContextMenuController::IsCommandIdEnabled(
    int command_id) const {
#if BUILDFLAG(IS_WIN)
  if (command_id == kMinimize || command_id == kClose) {
    return widget() != nullptr;
  }
#endif  // BUILDFLAG(IS_WIN)
  if (!web_contents()) {
    return false;
  }
  Profile* current_profile = delegate_->GetContextMenuProfile();
  switch (command_id) {
    case kEmoji:
      return ui::IsEmojiPanelSupported() && widget() != nullptr &&
             last_params_.is_editable;
    case kUndo:
      return (last_params_.edit_flags &
              blink::ContextMenuDataEditFlags::kCanUndo) != 0;
    case kCut:
      return ((last_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanCut) != 0) ||
             (last_params_.is_editable && !last_params_.selection_text.empty());
    case kCopy:
      return ((last_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanCopy) != 0) ||
             !last_params_.selection_text.empty();
    case kPaste:
      return ((last_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanPaste) != 0) ||
             last_params_.is_editable;
    case kPasteAndSearch:
      return current_profile &&
             (OmniboxEverywhereServiceFactory::GetForProfile(current_profile) !=
              nullptr) &&
             !ui::Clipboard::GetForCurrentThread()
                  ->IsMarkedByOriginatorAsConfidential();
    case kDelete:
      return ((last_params_.edit_flags &
               blink::ContextMenuDataEditFlags::kCanDelete) != 0) ||
             !last_params_.selection_text.empty();
    case kSelectAll:
      return true;
    case kManageSearchEngines:
      return current_profile && (OmniboxEverywhereServiceFactory::GetForProfile(
                                     current_profile) != nullptr);
    case kAlwaysShowAiMode:
      return IsFuseboxEligible(current_profile);
    case kShowShortcuts:
      return prefs::AreShortcutsAvailableForProfile(current_profile);
    case kCustomizeKeyboardShortcut:
      return true;
    case kSettings:
      return true;
#if BUILDFLAG(IS_MAC)
    case kWritingDirectionMenu:
      return true;
    case kWritingDirectionDefault:
      return (last_params_.writing_direction_default &
              blink::ContextMenuData::kCheckableMenuItemEnabled) != 0;
    case kWritingDirectionLtr:
      return (last_params_.writing_direction_left_to_right &
              blink::ContextMenuData::kCheckableMenuItemEnabled) != 0;
    case kWritingDirectionRtl:
      return (last_params_.writing_direction_right_to_left &
              blink::ContextMenuData::kCheckableMenuItemEnabled) != 0;
#endif  // BUILDFLAG(IS_MAC)
    default:
      return false;
  }
}

bool OmniboxEverywhereContextMenuController::IsCommandIdChecked(
    int command_id) const {
#if BUILDFLAG(IS_MAC)
  switch (command_id) {
    case kWritingDirectionDefault:
      return (last_params_.writing_direction_default &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    case kWritingDirectionLtr:
      return (last_params_.writing_direction_left_to_right &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    case kWritingDirectionRtl:
      return (last_params_.writing_direction_right_to_left &
              blink::ContextMenuData::kCheckableMenuItemChecked) != 0;
    default:
      break;
  }
#endif  // BUILDFLAG(IS_MAC)
  Profile* current_profile = delegate_->GetContextMenuProfile();
  if (command_id == kAlwaysShowAiMode) {
    if (current_profile && current_profile->GetPrefs()) {
      return current_profile->GetPrefs()->GetBoolean(
          prefs::kOmniboxEverywhereShowAiMode);
    }
    return true;
  }
  if (command_id == kShowShortcuts) {
    return prefs::IsOmniboxEverywhereShortcutsVisible(current_profile);
  }
  return false;
}

// views::ContextMenuController:
// Handles context menu requests that originate from the hosting views (e.g.
// right-clicking the widget's non-client area) rather than from the renderer.
void OmniboxEverywhereContextMenuController::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  views::Widget* host_widget = widget();
  if (!host_widget || !host_widget->GetContentsView() || !web_contents()) {
    return;
  }
  gfx::Point point_in_contents = point;
  views::View::ConvertPointFromScreen(host_widget->GetContentsView(),
                                      &point_in_contents);
  content::ContextMenuParams params;
  params.x = point_in_contents.x();
  params.y = point_in_contents.y();
  params.source_type = source_type;
  params.is_editable = false;
  HandleContextMenu(params);
}

}  // namespace omnibox_everywhere
