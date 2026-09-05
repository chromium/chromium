// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/menu_model.h"

namespace {

void LogHistogramMediumTimes(const std::string& histogram_name,
                             base::TimeDelta elapsed) {
  base::UmaHistogramCustomTimes(histogram_name, elapsed, base::Milliseconds(10),
                                base::Minutes(3), 50);
}

}  // namespace

OmniboxPopupHandler::OmniboxPopupHandler(
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_popup::mojom::Page> page,
    content::WebContents* web_contents,
    OmniboxController* controller)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents),
      controller_(controller) {}

OmniboxPopupHandler::~OmniboxPopupHandler() = default;

void OmniboxPopupHandler::ShowContextMenu(const gfx::Point& point) {
  if (embedder_) {
    embedder_->ShowContextMenu(point, nullptr);
  }
}

// TODO(crbug.com/553005514): Add traces for popup dismissal (CloseUI/Hide) and
// in-flight query cancellation when the Omnibox is closed rapidly or queries
// arrive from inactive tabs.
void OmniboxPopupHandler::CloseUI() {
  if (embedder_) {
    // NOTE: `embedder_->CloseUI()` transitions the popup state to `kNone`,
    // which prompts `LocationBarView::OnPopupStateChanged()` to centrally clear
    // Views focus and notify the edit model via `OnKillFocus()`.
    embedder_->CloseUI();
  }
  // Return keyboard focus to the active webpage.
  if (controller_ && controller_->client()) {
    controller_->client()->FocusWebContents();
  }
}

void OmniboxPopupHandler::OnSelectionChanged(const gfx::Range& selection,
                                             uint32_t sequence_number,
                                             bool show_full_url) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  latest_selection_ = selection;
  show_full_url_ = show_full_url;

  // Update the selection range of the native view so that when keyboard focus
  // is transferred to the native view upon click on top container, that typed
  // text is entered at the correct place.
  if (controller_) {
    if (auto* view = controller_->edit_model()->view()) {
      view->SetSelectionBounds(selection);
    }
  }
}

void OmniboxPopupHandler::Revert(uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  if (controller_) {
    if (auto* popup_view = controller_->edit_model()->popup_view()) {
      popup_view->SetIsReverting(true);
    }
    controller_->edit_model()->Revert();
    if (auto* popup_view = controller_->edit_model()->popup_view()) {
      popup_view->SetIsReverting(false);
    }
  }
  latest_selection_ = gfx::Range(0, 0);
}

void OmniboxPopupHandler::OnInputCleared(uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  latest_selection_ = gfx::Range(0, 0);
  show_full_url_ = false;
  if (controller_) {
    controller_->edit_model()->SetUserText(std::u16string());
    // TODO(b/504668292): Vet if this setting of `SetWindowTextAndCaretPos` can
    // be removed. Right now `FullWebUIOmniboxInteractiveTest.ClearAndSwitchTab`
    // relies on this, since it checks if the Views Omnibox has the same string
    // as the WebUI Omnibox, but it might not be needed in production.
    if (controller_->edit_model()->view()) {
      controller_->edit_model()->view()->SetWindowTextAndCaretPos(
          /*text=*/std::u16string(), /*caret_pos=*/0, /*update_popup=*/false,
          /*notify_text_changed=*/false);
    }
  }
}

void OmniboxPopupHandler::OnPaste(const std::string& text,
                                  const gfx::Range& selection,
                                  uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  gfx::Range prev_selection = latest_selection_;
  latest_selection_ = selection;
  show_full_url_ = false;

  if (controller_ && controller_->edit_model()) {
    OmniboxEditModel* model = controller_->edit_model();
    model->OnPaste();

    // The old text is deliberately set to the "empty string" in order to align
    // with the logic in `OmniboxViewViews::OnOmniboxPasteComplete()`.
    std::u16string u16_old_text;
    std::u16string u16_new_text = base::UTF8ToUTF16(text);

    OmniboxView::StateChanges state_changes;
    state_changes.old_text = &u16_old_text;
    state_changes.new_text = &u16_new_text;
    state_changes.new_selection = selection;
    state_changes.selection_differs =
        (!prev_selection.is_empty() || !selection.is_empty()) &&
        !prev_selection.EqualsIgnoringDirection(selection);
    state_changes.text_differs = u16_old_text != u16_new_text;
    // By definition, a PASTE operation cannot enter/exit keyword mode, so
    // `keyword_differs` is always set to `false`.
    state_changes.keyword_differs = false;
    // Since "old text" is an empty string and "new text" is a (potentially)
    // non-empty string, a PASTE operation is never going to be a deletion, so
    // `just_deleted_text` is always set to `false`.
    state_changes.just_deleted_text = false;

    bool something_changed = model->OnAfterPossibleChange(
        state_changes, /*allow_keyword_ui_change=*/true);

    if (something_changed &&
        (state_changes.text_differs || state_changes.keyword_differs)) {
      // TODO(b/514811525): Trigger URL component emphasis in WebUI.
      model->OnChanged();
    }
  }
}

void OmniboxPopupHandler::RequestInputState() {
  auto* edit_model = controller_ ? controller_->edit_model() : nullptr;
  auto* popup_view = edit_model ? edit_model->popup_view() : nullptr;
  if (popup_view) {
    popup_view->SyncNativeStateToWebUI(/*query_zps=*/false);
  }
}

void OmniboxPopupHandler::OnShow() {
  page_->OnShow();
}

void OmniboxPopupHandler::OnContextMenuClosed() {
  page_->OnContextMenuClosed();
}

void OmniboxPopupHandler::SetInputState(
    const std::string& text,
    const gfx::Range& selection,
    bool user_input_in_progress,
    const std::string& full_url,
    bool is_focused,
    const std::string& permanent_display_text,
    bool show_full_url,
    bool query_zps,
    searchbox::mojom::InputKeywordModelPtr keyword_model) {
  latest_selection_ = selection;
  show_full_url_ = show_full_url;
  current_sequence_number_++;

  TRACE_EVENT2("omnibox", "OmniboxPopupHandler::SetInputState",
               "sequence_number", current_sequence_number_, "is_focused",
               is_focused);

  auto state = omnibox_popup::mojom::OmniboxInputState::New();
  state->sequence_number = current_sequence_number_;
  state->text = text;
  state->selection = selection;
  state->user_input_in_progress = user_input_in_progress;
  state->full_url = full_url;
  state->is_focused = is_focused;
  state->permanent_display_text = permanent_display_text;
  state->show_full_url = show_full_url;
  state->query_zps = query_zps;
  state->keyword_model = std::move(keyword_model);
  // Extract active tab ID if in a Chrome browser window context.
  if (controller_ && controller_->client()->IsChromeOmniboxClient()) {
    auto* chrome_client =
        static_cast<ChromeOmniboxClient*>(controller_->client());
    auto* browser = chrome_client->browser();
    auto* tab_strip = browser ? browser->GetTabStripModel() : nullptr;
    auto* tab = tab_strip ? tab_strip->GetActiveTab() : nullptr;
    if (tab) {
      state->tab_id = tab->GetHandle().raw_value();
    }
  }
  page_->SetInputState(std::move(state));
}

void OmniboxPopupHandler::SetFocus(bool is_focused, bool query_zps) {
  page_->SetFocus(is_focused, query_zps);
}

void OmniboxPopupHandler::ClearAutocompleteMatches() {
  page_->ClearAutocompleteMatches();
}

void OmniboxPopupHandler::ClearPopup(base::OnceClosure callback) {
  page_->ClearPopup(std::move(callback));
}

void OmniboxPopupHandler::LogEscapeAction(
    omnibox_popup::mojom::OmniboxEscapeAction action) {
  base::UmaHistogramEnumeration("Omnibox.Escape", action);
}

void OmniboxPopupHandler::OpenAimPopup(bool via_keyboard) {
  if (controller_) {
    controller_->edit_model()->OpenSelection(
        OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                              OmniboxPopupSelection::FOCUSED_BUTTON_AIM),
        via_keyboard);
  }
}

void OmniboxPopupHandler::OnCutOrCopy(uint32_t sequence_number,
                                      bool is_cut,
                                      const std::string& full_text,
                                      const gfx::Range& selection) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  gfx::Range prev_selection = latest_selection_;
  latest_selection_ =
      is_cut ? gfx::Range(selection.GetMin(), selection.GetMin()) : selection;

  if (!controller_ || !controller_->edit_model()) {
    return;
  }

  std::u16string u16_old_text = base::UTF8ToUTF16(full_text);
  size_t sel_min = selection.GetMin();
  size_t sel_max = std::min(selection.GetMax(), u16_old_text.length());
  if (sel_min > sel_max) {
    sel_min = sel_max;
  }
  std::u16string u16_selected_text =
      u16_old_text.substr(sel_min, sel_max - sel_min);
  bool is_select_all = (sel_min == 0 && sel_max == u16_old_text.length() &&
                        !u16_old_text.empty());

  GURL url;
  bool write_url = false;
  controller_->edit_model()->AdjustTextForCopy(sel_min, &u16_selected_text,
                                               &url, &write_url);

  if (is_select_all) {
    base::UmaHistogramCounts1M(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

    const auto last_omnibox_focus =
        controller_->edit_model()->last_omnibox_focus();
    if (!last_omnibox_focus.is_null()) {
      const base::TimeDelta elapsed =
          base::TimeTicks::Now() - last_omnibox_focus;
      bool is_zero_suggest =
          controller_->autocomplete_controller() &&
          controller_->autocomplete_controller()->input().IsZeroSuggest();
      auto page_classification =
          controller_->edit_model()->GetPageClassification();

      LogHistogramMediumTimes("Omnibox.FocusToCutOrCopyAllTextTime", elapsed);

      const std::string page_context =
          metrics::OmniboxEventProto::PageClassification_Name(
              page_classification);
      LogHistogramMediumTimes(
          base::StrCat({"Omnibox.FocusToCutOrCopyAllTextTime.ByPageContext.",
                        page_context}),
          elapsed);

      if (is_zero_suggest) {
        LogHistogramMediumTimes(
            "Omnibox.FocusToCutOrCopyAllTextTime.ZeroSuggest", elapsed);
        LogHistogramMediumTimes(
            base::StrCat({"Omnibox.FocusToCutOrCopyAllTextTime.ZeroSuggest."
                          "ByPageContext.",
                          page_context}),
            elapsed);
      } else {
        LogHistogramMediumTimes(
            "Omnibox.FocusToCutOrCopyAllTextTime.TypedSuggest", elapsed);
        LogHistogramMediumTimes(
            base::StrCat({"Omnibox.FocusToCutOrCopyAllTextTime.TypedSuggest."
                          "ByPageContext.",
                          page_context}),
            elapsed);
      }
    }

    if (web_contents_) {
      if (auto* clusters_helper =
              HistoryClustersTabHelper::FromWebContents(web_contents_)) {
        clusters_helper->OnOmniboxUrlCopied();
      }
    }
  }

  // TODO(b/522957982): Update this variable to also reflect IME state (to
  // better align with `TextfieldModel::CutOrCopyAllowed()` logic).
  bool is_cut_or_copy_allowed = !selection.is_empty();
  if (is_cut_or_copy_allowed) {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    if (web_contents_ && web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      writer.MarkAsOffTheRecord();
    }
    writer.WriteText(u16_selected_text);
  }

  std::u16string u16_new_text =
      is_cut ? (u16_old_text.substr(0, sel_min) + u16_old_text.substr(sel_max))
             : u16_old_text;
  gfx::Range new_selection = is_cut ? gfx::Range(sel_min, sel_min) : selection;

  OmniboxView::StateChanges state_changes;
  state_changes.old_text = &u16_old_text;
  state_changes.new_text = &u16_new_text;
  state_changes.new_selection = new_selection;
  state_changes.selection_differs =
      (!prev_selection.is_empty() || !new_selection.is_empty()) &&
      !prev_selection.EqualsIgnoringDirection(new_selection);
  state_changes.text_differs = is_cut && (u16_old_text != u16_new_text);
  state_changes.keyword_differs = false;
  state_changes.just_deleted_text = is_cut && !u16_selected_text.empty();

  bool something_changed = controller_->edit_model()->OnAfterPossibleChange(
      state_changes, /*allow_keyword_ui_change=*/true);

  if (something_changed &&
      (state_changes.text_differs || state_changes.keyword_differs)) {
    controller_->edit_model()->OnChanged();
  }
}

void OmniboxPopupHandler::SetEditHistoryState(bool can_undo, bool can_redo) {
  can_undo_ = can_undo;
  can_redo_ = can_redo;
}

void OmniboxPopupHandler::OpenDevTools() {
  CHECK(base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug));
  if (web_contents_) {
    DevToolsWindow::OpenDevToolsWindow(web_contents_,
                                       DevToolsOpenedByAction::kUnknown);
  }
}
