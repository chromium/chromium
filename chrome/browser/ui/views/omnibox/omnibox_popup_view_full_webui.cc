// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/url_constants.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

searchbox::mojom::InputKeywordModelPtr CreateInputKeywordModel(
    KeywordState keyword_state,
    const std::u16string& keyword,
    const std::u16string& placeholder,
    const TemplateURLService* turl_service) {
  if (keyword_state == KeywordState::kNone) {
    CHECK(keyword.empty());
    return nullptr;
  }
  CHECK(!keyword.empty());
  auto keyword_model = searchbox::mojom::InputKeywordModel::New();
  keyword_model->type = keyword_state == KeywordState::kKeyword
                            ? searchbox::mojom::KeywordType::kInKeyword
                            : searchbox::mojom::KeywordType::kChip;
  keyword_model->keyword = base::UTF16ToUTF8(keyword);
  const auto names =
      SelectedKeywordView::GetKeywordLabelNames(keyword, turl_service);
  keyword_model->display_text = base::UTF16ToUTF8(names.full_name);
  keyword_model->placeholder = base::UTF16ToUTF8(placeholder);
  return keyword_model;
}

}  // namespace

OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    base::OnceClosure on_ready_callback)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupFullPresenter>(location_bar,
                                                      presenter_delegate,
                                                      controller)),
      on_ready_callback_(std::move(on_ready_callback)) {}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() = default;

void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  // Intentional no-op in Full WebUI mode. Suggestions and content updates are
  // handled directly via WebUI Mojo handlers and specific event hooks
  // (focus, tab switch). Avoid calling `OnFocus()` or
  // `SyncNativeStateToWebUI()` here, as that would clobber selection and cause
  // focus fights during deactivation or autocomplete changes.
}

void OmniboxPopupViewFullWebUI::SyncNativeStateToWebUI(bool query_zps,
                                                       bool show_full_url) {
  SyncNativeStateToWebUIImpl(
      query_zps ? omnibox_popup::mojom::MatchesUpdate::kQueryZps
                : omnibox_popup::mojom::MatchesUpdate::kDefault,
      show_full_url);
}

void OmniboxPopupViewFullWebUI::SyncNativeStateToWebUIPreservingMatches() {
  SyncNativeStateToWebUIImpl(
      omnibox_popup::mojom::MatchesUpdate::kPreserveMatches,
      /*show_full_url=*/false);
}

void OmniboxPopupViewFullWebUI::SyncNativeStateToWebUIImpl(
    omnibox_popup::mojom::MatchesUpdate matches_update,
    bool show_full_url) {
  TRACE_EVENT("omnibox", "OmniboxPopupViewFullWebUI::SyncNativeStateToWebUI",
              "matches_update", matches_update);
  auto* edit_model = controller()->edit_model();

  edit_model->ResetDisplayTexts();
  auto* popup_handler = GetPopupHandler();
  if (!popup_handler) {
    return;
  }

  bool user_input_in_progress = edit_model->user_input_in_progress();
  std::u16string permanent_display_text = edit_model->GetPermanentDisplayText();
  std::u16string text =
      user_input_in_progress ? edit_model->user_text() : permanent_display_text;
  bool focus = edit_model->has_focus();
  const std::u16string full_url = controller()->client()->GetFormattedFullURL();
  const bool should_show_full_url =
      show_full_url && !user_input_in_progress && !full_url.empty();
  // Default to select-all on permanent URLs (so focusing or refocusing selects
  // all text, and empty text defaults to Range(0, 0)).
  gfx::Range selection(
      0, should_show_full_url ? full_url.length() : text.length());

  // `last_sent_text_` is null after a state reset (e.g., tab switch).
  // Otherwise, check if `text` has diverged from what WebUI currently has.
  const bool text_changed = !last_sent_text_ || text != *last_sent_text_;

  const gfx::Range handler_selection = popup_handler->latest_selection();
  // When `user_input_in_progress` is false, a highlight may have been created
  // against the unelided `full_url` rather than the elided
  // `permanent_display_text` (`text`).
  const size_t max_selection_length =
      user_input_in_progress ? text.length()
                             : std::max(text.length(), full_url.length());
  const bool is_handler_selection_valid =
      !text_changed && handler_selection.IsValid() &&
      handler_selection.GetMax() <= max_selection_length;

  // Don't test `OmniboxView::HasSelection()` here. `OmniboxViewViews` keeps
  // reporting a selection long after it was handed off (nothing clears it,
  // since the native view holds `FocusBehavior::NEVER` in Full WebUI mode and
  // so never sees `OnBlur()`), and `WebUIReadOnlyOmnibox` hardcodes it to true
  // because an `<input>` always has a selection. Inspect the range instead, and
  // require it to be in-bounds so a highlight left over from longer, stale text
  // can't be applied.
  const gfx::Range native_selection = omnibox_view_
                                          ? omnibox_view_->GetSelectionBounds()
                                          : gfx::Range::InvalidRange();
  const bool has_native_highlight =
      native_selection.IsValid() && !native_selection.is_empty() &&
      native_selection.GetMax() <= max_selection_length;

  // When `should_show_full_url` is true (e.g. Ctrl+L / Cmd+L), keep the full
  // URL selected rather than restoring a prior native or WebUI selection.
  if (!should_show_full_url && has_native_highlight &&
      native_selection != last_consumed_native_selection_) {
    // The native view has a highlight we have not handed off yet (e.g. from
    // mouse dragging or double-clicking in Views), so preserve the exact
    // selection range. Record it so that later syncs, which would still see
    // this same range on `omnibox_view_`, defer to whatever selection WebUI
    // reports instead of re-applying it.
    selection = native_selection;
    last_consumed_native_selection_ = native_selection;
  } else if (user_input_in_progress) {
    // Preserve existing WebUI selection if the text has not changed and WebUI
    // selection is valid; otherwise default to placing the caret at the end of
    // the user text.
    selection = is_handler_selection_valid
                    ? handler_selection
                    : gfx::Range(text.length(), text.length());
  } else if (!should_show_full_url && is_handler_selection_valid &&
             !handler_selection.is_empty()) {
    // For permanent URLs without native selection, if WebUI already has a valid
    // non-empty selection range (e.g. from user highlighting text in WebUI),
    // preserve it.
    selection = handler_selection;
  }

  bool selection_changed = selection != handler_selection;
  bool focus_changed = !last_sent_focus_ || focus != *last_sent_focus_;

  // Always send for a zero-prefix query, or when preserving matches, even if
  // nothing else changed, so that WebUI starts the query or stops any
  // in-flight one, respectively.
  if (text_changed || selection_changed || focus_changed ||
      matches_update != omnibox_popup::mojom::MatchesUpdate::kDefault) {
    searchbox::mojom::InputKeywordModelPtr keyword_model =
        CreateInputKeywordModel(
            edit_model->keyword_state(), edit_model->keyword(),
            edit_model->keyword_placeholder(),
            controller()->client()->GetTemplateURLService());
    // TODO(crbug.com/497883783): Consider adding a dedicated
    // `SetSelectionRange` IPC method so that when only the selection
    // changes (e.g. during double clicks or mouse dragging), we do not push
    // the input text and risk resetting DOM input state or scroll position.
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, user_input_in_progress,
        base::UTF16ToUTF8(full_url), edit_model->has_focus(),
        base::UTF16ToUTF8(permanent_display_text), should_show_full_url,
        matches_update, std::move(keyword_model), /*is_tab_switch=*/false);
    last_sent_text_ = text;
    last_sent_focus_ = focus;
  }
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  CHECK(tab, base::NotFatalUntil::M162);
  TRACE_EVENT("omnibox", "OmniboxPopupViewFullWebUI::SaveStateToTab");

  auto* edit_model = controller()->edit_model();
  bool logically_focused = edit_model->has_focus();

  // The text input lives in WebUI, so the native view hierarchy does not
  // track selection. Fetch it from the WebUI handler.
  gfx::Range selection;
  bool show_full_url = false;
  if (auto* popup_handler = GetPopupHandler()) {
    selection = popup_handler->latest_selection();
    show_full_url = popup_handler->show_full_url();
  }

  // If the user cleared the Omnibox, we should not preserve the empty draft
  // across tab switches. Explicitly revert the edit model so that switching
  // back to this tab restores the page's permanent URL (or empty for NTP).
  // Don't try to revert the model if the user is in keyword mode, even if there
  // is no further query; otherwise, it'd restore the default text as a keyword
  // query.
  const bool was_cleared_by_user = edit_model->user_input_in_progress() &&
                                   edit_model->user_text().empty() &&
                                   !edit_model->is_keyword_selected();

  const OmniboxEditModel::State default_state =
      edit_model->GetStateForTabSwitch();
  std::unique_ptr<OmniboxEditModel::State> state;

  const OmniboxFocusState target_focus_state =
      logically_focused ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_NONE;

  state = std::make_unique<OmniboxEditModel::State>(default_state);
  state->focus_state = target_focus_state;

  if (was_cleared_by_user) {
    std::u16string permanent_text = edit_model->GetPermanentDisplayText();
    // If the WebUI input field was physically empty before switching tabs,
    // we must select-all on restoration (since the permanent URL will be
    // restored).
    selection = gfx::Range(0, permanent_text.length());
    // Force `user_input_in_progress` to false to trigger a revert
    // to the permanent URL on restoration.
    state->user_input_in_progress = false;
  }

  // WebUI retains selection across focus changes, so we only need to sync
  // the active selection. `saved_selection_for_focus_change` is unused.
  tab->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(
          *state, selection,
          /*saved_selection_for_focus_change=*/gfx::Range::InvalidRange(),
          show_full_url));
}

void OmniboxPopupViewFullWebUI::OnTabChanged(content::WebContents* contents) {
  TRACE_EVENT("omnibox", "OmniboxPopupViewFullWebUI::OnTabChanged");
  Observe(contents);
  last_sent_text_.reset();
  last_sent_focus_.reset();
  last_consumed_native_selection_.reset();
  controller()->edit_model()->ResetDisplayTexts();

  // Cancel in-flight queries from the previous tab immediately rather than
  // waiting for the WebUI's `stopAutocomplete` IPC.
  controller()->StopAutocomplete(/*clear_result=*/true);

  // TODO(b:544433912) Consider removing or fixing `target_popup_state` logic as
  //   it doesn't seem to be opening the popup as it intends, nor is it clear if
  //   it even should.
  OmniboxPopupState target_popup_state;
  auto* state = static_cast<OmniboxState*>(
      contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  bool should_focus_popup = false;
  // `user_input_in_progress` is true only if there's non-empty input in
  // progress.
  bool non_empty_user_input_in_progress =
      state ? state->model_state.user_input_in_progress : false;

  const bool is_first_tab_changed = !has_completed_first_tab_changed_;
  has_completed_first_tab_changed_ = true;

  // Don't open or focus the popup while the location bar isn't drawn (e.g. a
  // tab switch in fullscreen). `ReshowPopup()` reopens it for a draft once it
  // is drawn again.
  const bool can_show_popup = !location_bar() || location_bar()->IsDrawn();

  if (state) {
    // Restore the saved state for the tab.
    controller()->edit_model()->RestoreState(&state->model_state);

    // Only request native keyboard focus for the omnibox
    // popup if it was logically focused when the user switched tabs.
    should_focus_popup =
        can_show_popup && state->model_state.focus_state != OMNIBOX_FOCUS_NONE;

    // The popup must be visible (`OmniboxPopupState::kFull`) if there is an
    // active draft or if the omnibox should have visible focus, and the popup
    // is initialized and ready.
    target_popup_state =
        IsPopupHandlerReady() && can_show_popup &&
                (non_empty_user_input_in_progress || should_focus_popup)
            ? OmniboxPopupState::kFull
            : OmniboxPopupState::kNone;

    // Set popup state before setting focus state to avoid focus ring flicker.
    controller()->popup_state_manager()->SetPopupState(target_popup_state);

    // Prevent focus state leaks by explicitly syncing the `OmniboxEditModel`'s
    // focus state with the restored state of the newly active tab.
    if (should_focus_popup) {
      if (!is_first_tab_changed) {
        TRACE_EVENT_INSTANT0(
            "omnibox", "OmniboxPopupViewFullWebUI::OnTabChanged:OnSetFocus",
            TRACE_EVENT_SCOPE_THREAD);
      }
      controller()->edit_model()->OnSetFocus(/*control_down=*/false);
    } else {
      controller()->edit_model()->OnKillFocus();
    }
  } else {
    // No saved state. Revert the edit model and check if the tab should focus
    // the location bar by default (e.g., New Tab Page).
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    should_focus_popup =
        can_show_popup && contents->FocusLocationBarByDefault();
    // Only transition to `kFull` if the popup is initialized and ready.
    // Otherwise, maintain `kNone` until `OnPopupHandlerReady()` is called.
    target_popup_state = IsPopupHandlerReady() && should_focus_popup
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    // Set popup state before setting focus state to avoid focus ring flicker.
    controller()->popup_state_manager()->SetPopupState(target_popup_state);

    if (should_focus_popup) {
      if (!is_first_tab_changed) {
        TRACE_EVENT_INSTANT0(
            "omnibox", "OmniboxPopupViewFullWebUI::OnTabChanged:OnSetFocus",
            TRACE_EVENT_SCOPE_THREAD);
      }
      controller()->edit_model()->OnSetFocus(/*control_down=*/false);
    } else {
      controller()->edit_model()->OnKillFocus();
    }
  }

  // Keep `focused_` in sync with the tab's target focus state. When switching
  // to an unfocused tab, `edit_model()->OnKillFocus()` is called directly above
  // rather than `OmniboxPopupViewFullWebUI::OnBlur()`, which would otherwise
  // leave `focused_` stale at `true`.
  focused_ = should_focus_popup;

  // Request focus before pushing content state so our `SetInputState` IPC
  // overrides any OS-default focus selection (such as macOS Select-All).
  if (target_popup_state == OmniboxPopupState::kFull) {
    if (presenter()) {
      if (presenter()->ShouldApplyHeightWorkarounds()) {
        // Reset cached height to 1 on tab switch. This forces
        // `OmniboxPopupFullPresenter::SynchronizePopupBounds` to fall back to
        // `default_height` (the single location bar height) and drop
        // elevation to 0, preventing a transient blank white dropdown box from
        // painting while WebUI updates matches for the new tab.
        presenter()->OnContentHeightChanged(1);
      }
      presenter()->Show();
      if (should_focus_popup) {
        // Reset stored focus for the newly active WebContents (e.g. NTP) so
        // subsequent calls to `WebContents::RestoreFocus()` in BrowserView
        // do not overwrite native FocusManager focus back to the page
        // container.
        if (contents) {
          if (auto* focus_helper =
                  ChromeWebContentsViewFocusHelper::FromWebContents(contents)) {
            focus_helper->ResetStoredFocus();
          }
        }
        presenter()->RequestFocus();
      } else if (contents) {
        if (auto* focus_helper =
                ChromeWebContentsViewFocusHelper::FromWebContents(contents)) {
          focus_helper->RestoreFocus();
        }
      }
    }
  } else {
    if (presenter()) {
      presenter()->Hide();
    }
    if (!should_focus_popup && contents) {
      if (auto* focus_helper =
              ChromeWebContentsViewFocusHelper::FromWebContents(contents)) {
        focus_helper->RestoreFocus();
      }
    }
  }

  // Push the restored state to the WebUI handler so it can render the
  // correct text and selection range for the newly selected tab.
  if (auto* popup_handler = GetPopupHandler()) {
    std::u16string user_text = state ? state->model_state.user_text : u"";
    std::u16string permanent_display_text =
        controller()->edit_model()->GetPermanentDisplayText();
    std::u16string text =
        non_empty_user_input_in_progress ? user_text : permanent_display_text;
    bool show_full_url = state ? state->show_full_url : false;
    const std::u16string full_url =
        controller()->client()->GetFormattedFullURL();
    gfx::Range selection =
        state ? state->selection : gfx::Range(0, text.length());
    searchbox::mojom::InputKeywordModelPtr keyword_model;
    if (state) {
      keyword_model = CreateInputKeywordModel(
          state->model_state.keyword_state, state->model_state.keyword,
          state->model_state.keyword_placeholder,
          controller()->client()->GetTemplateURLService());
    }
    const bool is_tab_switch =
        !is_first_tab_changed && (state != nullptr || !should_focus_popup);
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, non_empty_user_input_in_progress,
        base::UTF16ToUTF8(full_url), should_focus_popup,
        base::UTF16ToUTF8(permanent_display_text), show_full_url,
        omnibox_popup::mojom::MatchesUpdate::kDefault, std::move(keyword_model),
        is_tab_switch);
    last_sent_text_ = text;
    last_sent_focus_ = should_focus_popup;
  }
}

void OmniboxPopupViewFullWebUI::ReshowPopup() {
  // Only a draft reopens the popup, as only a draft keeps it open when the
  // omnibox is blurred (see `OmniboxPopupFullPresenter`).
  if (controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kNone ||
      !IsPopupHandlerReady() || !controller()->edit_model()->HasDraft()) {
    return;
  }
  // Closing the popup cleared the WebUI, so resend the whole input state.
  last_sent_text_.reset();
  last_sent_focus_.reset();
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);
  SyncNativeStateToWebUI(/*query_zps=*/false, /*show_full_url=*/false);
}

void OmniboxPopupViewFullWebUI::OnFocus(bool query_zps,
                                        bool select_all,
                                        bool show_full_url) {
  TRACE_EVENT("omnibox", "OmniboxPopupViewFullWebUI::OnFocus", "query_zps",
              query_zps, "select_all", select_all);
  // Don't show the popup while the location bar isn't drawn, e.g. on Ctrl+L in
  // fullscreen, which `BrowserView::SetFocusToLocationBar()` still allows.
  if (location_bar() && !location_bar()->IsDrawn()) {
    return;
  }
  if (presenter() && presenter()->GetWebUIContent()) {
    static_cast<OmniboxFullPopupWebUIContent*>(presenter()->GetWebUIContent())
        ->CancelPendingContextMenuRequest();
  }
  focused_ = true;
  bool changed = controller()->popup_state_manager()->popup_state() !=
                 OmniboxPopupState::kFull;

  if (changed) {
    last_sent_text_.reset();
    last_sent_focus_.reset();
  }
  // Reset `last_consumed_native_selection_` when the popup is opening
  // (`changed`) or when a user-initiated focus/select-all (`query_zps` or
  // `select_all`) is triggered while the popup is already
  // open (`!changed`), so a repeated `OmniboxView::SelectAll` with identical
  // bounds is still handed off to WebUI. Skip resetting during passive focus
  // restorations (`!changed && !query_zps && !select_all`) so
  // a stale native selection does not overwrite the user's WebUI selection.
  if (changed || query_zps || select_all) {
    last_consumed_native_selection_.reset();
  }

  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);
  controller()->edit_model()->OnSetFocus(/*control_down=*/false);

  if (presenter()) {
    presenter()->Show();
    // Explicitly request focus on the presenter and its WebContents to ensure
    // keyboard events route to the WebUI input field.
    presenter()->RequestFocus();
  }

  if (changed) {
    SyncNativeStateToWebUI(query_zps, show_full_url);
  } else if (auto* popup_handler = GetPopupHandler()) {
    // If the popup was already open (`!changed`), explicitly send
    // `SetFocus(true, query_zps, select_all, show_full_url)` via IPC to ensure
    // WebUI DOM input element focus and suggestions are restored.
    popup_handler->SetFocus(true, query_zps, select_all, show_full_url);
  }
}

void OmniboxPopupViewFullWebUI::
    FocusSearchWithDefaultSearchEngineKeywordMode() {
  if (auto* popup_handler = GetPopupHandler()) {
    popup_handler->FocusSearchWithDefaultSearchEngineKeywordMode();
  }
}

void OmniboxPopupViewFullWebUI::OnBlur() {
  focused_ = false;
  if (auto* popup_handler = GetPopupHandler()) {
    popup_handler->SetFocus(false);
  }
}

bool OmniboxPopupViewFullWebUI::RequestWebUIContextMenu(
    const gfx::Point& screen_point,
    ui::mojom::MenuSourceType source_type) {
  if (!presenter() || !presenter()->GetWebUIContent()) {
    return false;
  }
  return static_cast<OmniboxFullPopupWebUIContent*>(
             presenter()->GetWebUIContent())
      ->RequestContextMenu(screen_point, source_type);
}

void OmniboxPopupViewFullWebUI::OnPopupHandlerReady() {
  if (on_ready_callback_) {
    std::move(on_ready_callback_).Run();
  }
}

bool OmniboxPopupViewFullWebUI::IsPopupHandlerReady() const {
  return const_cast<OmniboxPopupViewFullWebUI*>(this)->GetPopupHandler() !=
         nullptr;
}

OmniboxPopupHandler* OmniboxPopupViewFullWebUI::GetPopupHandler() {
  if (!presenter() || !presenter()->GetWebUIContent()) {
    return nullptr;
  }
  auto* contents_wrapper = presenter()->GetWebUIContent()->contents_wrapper();
  auto* webui_controller =
      contents_wrapper ? contents_wrapper->GetWebUIController() : nullptr;
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  return popup_ui ? popup_ui->popup_handler() : nullptr;
}

bool OmniboxPopupViewFullWebUI::IsReverting() const {
  return is_reverting_;
}

void OmniboxPopupViewFullWebUI::SetIsReverting(bool reverting) {
  is_reverting_ = reverting;
}

void OmniboxPopupViewFullWebUI::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Clicking into the page with a draft in the omnibox leaves the popup open,
  // unfocused, to show the draft (see OmniboxPopupFullPresenter::OnEvent()).
  // A later cross-document navigation of the page, e.g. clicking a link, ends
  // that edit session.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  // An unfocused draft is shown in the open popup, or kept in the edit model
  // while the popup is closed because the location bar isn't drawn (see
  // `ReshowPopup()`). Leave the AI Mode popup alone.
  auto* edit_model = controller()->edit_model();
  if (edit_model->has_focus() || !edit_model->HasDraft() ||
      controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim) {
    return;
  }
  if (omnibox_view_) {
    omnibox_view_->RevertAll();
  }
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
}
