// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include <string>

#include "base/trace_event/trace_event.h"
#include "chrome/browser/search/search.h"
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
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

searchbox::mojom::InputKeywordModelPtr CreateInputKeywordModel(
    KeywordState keyword_state,
    const std::u16string& keyword,
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
  return keyword_model;
}

// Returns true if `contents` should automatically focus the location bar by
// default (e.g. New Tab Page), verifying both the visible and pending URLs to
// avoid false-positives from stale navigation entries during tab creation.
bool ShouldFocusLocationBarForTab(content::WebContents* contents) {
  if (!contents || !contents->FocusLocationBarByDefault()) {
    return false;
  }
  const GURL& visible_url = contents->GetVisibleURL();
  content::NavigationEntry* pending_entry =
      contents->GetController().GetPendingEntry();
  const GURL& pending_url = pending_entry ? pending_entry->GetURL() : GURL();

  auto is_ntp_url = [](const GURL& url) {
    return search::IsNTPURL(url) ||
           url.spec() == chrome::kChromeUISplitViewNewTabPageURL;
  };

  return is_ntp_url(visible_url) || is_ntp_url(pending_url);
}

}  // namespace

OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupFullPresenter>(location_bar,
                                                      presenter_delegate,
                                                      controller)) {}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() = default;

// This method acts solely as a visibility and focus guard gate. Avoid adding
// extra feature logic here, as content updates are typically handled via
// `SyncNativeStateToWebUI` from specific event hooks (such as focus or tab
// switches).
void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  // Show the suggestions popup and synchronize state if the browser window is
  // active and the omnibox view holds keyboard focus. This prevents background
  // updates or asynchronous tab-switch restorations from opening the popup
  // when the user is interacting with other parts of the UI, while allowing
  // typing into the WebUI omnibox after clicking the top container.
  if (controller()->popup_state_manager()->popup_state() !=
      OmniboxPopupState::kFull) {
    return;
  }
  views::Widget* widget = presenter()->delegate().GetLocationBarWidget();
  if (widget && widget->IsActive() && location_bar()->IsFocusWithin()) {
    if (!IsReverting()) {
      OnFocus(/*query_zps=*/false);
      SyncNativeStateToWebUI(/*query_zps=*/false);
    }
  }
}

// TODO(crbug.com/553005514): Instrument callsite traces
// (ex: OnNewTabFocus, OnFocus, OnTabChanged, SaveStateToTab) at their
// respective entry points to capture individual trigger contexts.
void OmniboxPopupViewFullWebUI::SyncNativeStateToWebUI(bool query_zps) {
  TRACE_EVENT1("omnibox", "OmniboxPopupViewFullWebUI::SyncNativeStateToWebUI",
               "query_zps", query_zps);
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
  // Default to select-all if focused so that taking focus selects all text by
  // default (whether permanent URL or draft). Otherwise default to empty
  // selection.
  gfx::Range selection =
      focus ? gfx::Range(0, text.length()) : gfx::Range(0, 0);
  // If the user is actively typing a draft or has an active highlight from
  // mouse dragging or double-clicking, use the native view's selection range.
  if (omnibox_view_) {
    if (user_input_in_progress || omnibox_view_->HasSelection()) {
      selection = omnibox_view_->GetSelectionBounds();
    }
  }
  const std::u16string full_url = controller()->client()->GetFormattedFullURL();

  // `last_sent_text_` is null after a state reset (e.g., tab switch).
  // Otherwise, check if `text`, `selection`, or `focus` has diverged.
  bool text_changed = !last_sent_text_ || text != *last_sent_text_;
  bool selection_changed = selection != popup_handler->latest_selection();
  bool focus_changed = !last_sent_focus_ || focus != *last_sent_focus_;

  if (text_changed || selection_changed || focus_changed || query_zps) {
    searchbox::mojom::InputKeywordModelPtr keyword_model =
        CreateInputKeywordModel(
            edit_model->keyword_state(), edit_model->keyword(),
            controller()->client()->GetTemplateURLService());
    // TODO(crbug.com/497883783): Consider adding a dedicated
    // `SetSelectionRange` IPC method so that when only the selection
    // changes (e.g. during double clicks or mouse dragging), we do not push
    // the input text and risk resetting DOM input state or scroll position.
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, user_input_in_progress,
        base::UTF16ToUTF8(full_url), edit_model->has_focus(),
        base::UTF16ToUTF8(permanent_display_text), /*show_full_url=*/false,
        query_zps, std::move(keyword_model));
    last_sent_text_ = text;
    last_sent_focus_ = focus;
  }
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

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
  last_sent_text_.reset();
  last_sent_focus_.reset();

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

  if (state) {
    // Restore the saved state for the tab.
    controller()->edit_model()->RestoreState(&state->model_state);

    // Prevent focus state leaks by explicitly syncing the `OmniboxEditModel`'s
    // focus state with the restored state of the newly active tab.
    if (state->model_state.focus_state != OMNIBOX_FOCUS_NONE) {
      controller()->edit_model()->OnSetFocus(/*control_down=*/false);
    } else {
      controller()->edit_model()->OnKillFocus();
    }

    // Only request native keyboard focus for the omnibox
    // popup if it was logically focused when the user switched tabs.
    should_focus_popup = (state->model_state.focus_state != OMNIBOX_FOCUS_NONE);

    // The popup must be visible (`OmniboxPopupState::kFull`) if there is an
    // active draft or if the omnibox should have visible focus.
    target_popup_state = non_empty_user_input_in_progress || should_focus_popup
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
  } else {
    // No saved state. Revert the edit model and check if the tab should focus
    // the location bar by default (e.g., New Tab Page).
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    should_focus_popup = ShouldFocusLocationBarForTab(contents);
    if (should_focus_popup) {
      controller()->edit_model()->OnSetFocus(/*control_down=*/false);
      target_popup_state = OmniboxPopupState::kFull;
    } else {
      controller()->edit_model()->OnKillFocus();
      target_popup_state = OmniboxPopupState::kNone;
    }
  }

  // TODO(b/504668582): Fix flicker that occurs when switching between two tabs
  //   that have an Omnibox with text.
  controller()->popup_state_manager()->SetPopupState(target_popup_state);

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
      }
    }
  } else {
    if (presenter()) {
      presenter()->Hide();
    }
    if (contents) {
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
    gfx::Range selection = state ? state->selection : gfx::Range(0, 0);
    searchbox::mojom::InputKeywordModelPtr keyword_model;
    if (state) {
      keyword_model = CreateInputKeywordModel(
          state->model_state.keyword_state, state->model_state.keyword,
          controller()->client()->GetTemplateURLService());
    }
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, non_empty_user_input_in_progress,
        base::UTF16ToUTF8(full_url), should_focus_popup,
        base::UTF16ToUTF8(permanent_display_text), show_full_url,
        /*query_zps=*/false, std::move(keyword_model));
    last_sent_text_ = text;
    last_sent_focus_ = should_focus_popup;
  }
}

void OmniboxPopupViewFullWebUI::OnFocus(bool query_zps) {
  focused_ = true;
  bool changed = controller()->popup_state_manager()->popup_state() !=
                 OmniboxPopupState::kFull;

  if (changed) {
    last_sent_text_.reset();
    last_sent_focus_.reset();
  }

  controller()->edit_model()->OnSetFocus(/*control_down=*/false);
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);

  if (presenter()) {
    presenter()->Show();
    // Explicitly request focus on the presenter and its WebContents to ensure
    // keyboard events route to the WebUI input field.
    presenter()->RequestFocus();
  }

  if (changed) {
    SyncNativeStateToWebUI(query_zps);
  } else if (auto* popup_handler = GetPopupHandler()) {
    // If the popup was already open (`!changed`), explicitly send
    // `SetFocus(true, query_zps)` via IPC to ensure WebUI DOM input element
    // focus and suggestions are restored.
    popup_handler->SetFocus(true, query_zps);
  }
}

void OmniboxPopupViewFullWebUI::OnBlur() {
  focused_ = false;
  if (auto* popup_handler = GetPopupHandler()) {
    popup_handler->SetFocus(false);
  }
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
