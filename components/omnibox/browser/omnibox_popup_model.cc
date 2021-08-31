// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_model.h"

#include <algorithm>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// OmniboxPopupSelection

const size_t OmniboxPopupSelection::kNoMatch = static_cast<size_t>(-1);

bool OmniboxPopupSelection::operator==(const OmniboxPopupSelection& b) const {
  return line == b.line && state == b.state;
}

bool OmniboxPopupSelection::operator!=(const OmniboxPopupSelection& b) const {
  return !operator==(b);
}

bool OmniboxPopupSelection::operator<(const OmniboxPopupSelection& b) const {
  if (line == b.line)
    return state < b.state;

  return line < b.line;
}

bool OmniboxPopupSelection::IsChangeToKeyword(
    OmniboxPopupSelection from) const {
  return state == OmniboxPopupSelection::KEYWORD_MODE &&
         from.state != OmniboxPopupSelection::KEYWORD_MODE;
}

bool OmniboxPopupSelection::IsButtonFocused() const {
  return state != OmniboxPopupSelection::NORMAL &&
         state != OmniboxPopupSelection::KEYWORD_MODE;
}

///////////////////////////////////////////////////////////////////////////////
// OmniboxPopupModel

OmniboxPopupModel::OmniboxPopupModel(OmniboxPopupView* popup_view,
                                     OmniboxEditModel* edit_model,
                                     PrefService* pref_service)
    : view_(popup_view),
      edit_model_(edit_model),
      pref_service_(pref_service),
      selection_(OmniboxPopupSelection::kNoMatch,
                 OmniboxPopupSelection::NORMAL) {}

OmniboxPopupModel::~OmniboxPopupModel() = default;

bool OmniboxPopupModel::IsOpen() const {
  return view_->IsOpen();
}

void OmniboxPopupModel::SetSelection(OmniboxPopupSelection new_selection,
                                     bool reset_to_default,
                                     bool force_update_ui) {
  if (result().empty())
    return;

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

  if (new_selection == selection_ && !force_update_ui)
    return;  // Nothing else to do.

  // We need to update selection before notifying any views, as they will query
  // selection_ to update themselves.
  const OmniboxPopupSelection old_selection = selection_;
  selection_ = new_selection;
  view_->OnSelectionChanged(old_selection, selection_);

  if (selection_.line == OmniboxPopupSelection::kNoMatch)
    return;

  const AutocompleteMatch& match = result().match_at(selection_.line);
  DCHECK((selection_.state != OmniboxPopupSelection::KEYWORD_MODE) ||
         match.associated_keyword.get());
  if (selection_.IsButtonFocused()) {
    old_focused_url_ = match.destination_url;
    edit_model_->SetAccessibilityLabel(match);
    // TODO(tommycli): Fold the focus hint into view_->OnSelectionChanged().
    // Caveat: We must update the accessibility label before notifying the View.
    view_->ProvideButtonFocusHint(selected_line());
  }

  std::u16string keyword;
  bool is_keyword_hint;
  TemplateURLService* service = edit_model_->client()->GetTemplateURLService();
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);

  if (selection_.state == OmniboxPopupSelection::FOCUSED_BUTTON_HEADER) {
    // If the new selection is a Header, the temporary text is an empty string.
    edit_model_->OnPopupDataChanged(std::u16string(),
                                    /*is_temporary_text=*/true,
                                    std::u16string(), std::u16string(), {},
                                    keyword, is_keyword_hint, std::u16string());
  } else if (old_selection.line != selection_.line ||
             (old_selection.IsButtonFocused() &&
              !new_selection.IsButtonFocused() &&
              new_selection.state != OmniboxPopupSelection::KEYWORD_MODE)) {
    // Otherwise, only update the edit model for line number changes, or
    // when the old selection was a button and we're not entering keyword mode.
    // Updating the edit model for every state change breaks keyword mode.
    if (reset_to_default) {
      edit_model_->OnPopupDataChanged(
          std::u16string(),
          /*is_temporary_text=*/false, match.inline_autocompletion,
          match.prefix_autocompletion, match.split_autocompletion, keyword,
          is_keyword_hint, match.additional_text);
    } else {
      edit_model_->OnPopupDataChanged(
          match.fill_into_edit,
          /*is_temporary_text=*/true, std::u16string(), std::u16string(), {},
          keyword, is_keyword_hint, std::u16string());
    }
  }
}

void OmniboxPopupModel::ResetToInitialState() {
  size_t new_line =
      result().default_match() ? 0 : OmniboxPopupSelection::kNoMatch;
  SetSelection(OmniboxPopupSelection(new_line, OmniboxPopupSelection::NORMAL),
               /*reset_to_default=*/true);
  view_->OnDragCanceled();
}

void OmniboxPopupModel::TryDeletingLine(size_t line) {
  // When called with line == selected_line(), we could use
  // GetInfoForCurrentText() here, but it seems better to try and delete the
  // actual selection, rather than any "in progress, not yet visible" one.
  if (line == OmniboxPopupSelection::kNoMatch) {
    return;
  }

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

  const AutocompleteMatch& match = result().match_at(line);
  if (match.SupportsDeletion()) {
    // Try to preserve the selection even after match deletion.
    size_t old_selected_line = selected_line();

    // This will synchronously notify both the edit and us that the results
    // have changed, causing both to revert to the default match.
    autocomplete_controller()->DeleteMatch(match);

    // Clamp the old selection to the new size of result(), since there may be
    // fewer results now.
    if (old_selected_line != OmniboxPopupSelection::kNoMatch)
      old_selected_line = std::min(line, result().size() - 1);

    // Move the selection to the next choice after the deleted one.
    // SetSelectedLine() will clamp to take care of the case where we deleted
    // the last item.
    // TODO(pkasting): Eventually the controller should take care of this
    // before notifying us, reducing flicker.  At that point the check for
    // deletability can move there too.
    SetSelection(
        OmniboxPopupSelection(old_selected_line, OmniboxPopupSelection::NORMAL),
        false, true);
  }
}

bool OmniboxPopupModel::SelectionOnInitialLine() const {
  size_t initial_line =
      result().default_match() ? 0 : OmniboxPopupSelection::kNoMatch;
  return selected_line() == initial_line;
}

void OmniboxPopupModel::OnResultChanged() {
  rich_suggestion_bitmaps_.clear();
  const AutocompleteResult& result = this->result();
  size_t old_selected_line = selected_line();

  if (result.default_match()) {
    OmniboxPopupSelection selection(0, selected_line_state());

    // If selected line state was |BUTTON_FOCUSED_TAB_SWITCH| and nothing has
    // changed leave it.
    const bool has_focused_match =
        selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH &&
        result.match_at(selection.line).has_tab_match;
    const bool has_changed =
        selection.line != old_selected_line ||
        result.match_at(selection.line).destination_url != old_focused_url_;

    if (!has_focused_match || has_changed) {
      selection.state = OmniboxPopupSelection::NORMAL;
    }
    selection_ = selection;
  } else {
    selection_ = OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                       OmniboxPopupSelection::NORMAL);
  }

  bool popup_was_open = view_->IsOpen();
  view_->UpdatePopupAppearance();
  if (view_->IsOpen() != popup_was_open)
    edit_model_->controller()->OnPopupVisibilityChanged();
}

const SkBitmap* OmniboxPopupModel::RichSuggestionBitmapAt(
    int result_index) const {
  const auto iter = rich_suggestion_bitmaps_.find(result_index);
  if (iter == rich_suggestion_bitmaps_.end()) {
    return nullptr;
  }
  return &iter->second;
}

void OmniboxPopupModel::SetRichSuggestionBitmap(int result_index,
                                                const SkBitmap& bitmap) {
  rich_suggestion_bitmaps_[result_index] = bitmap;
  view_->UpdatePopupAppearance();
}

std::vector<OmniboxPopupSelection>
OmniboxPopupModel::GetAllAvailableSelectionsSorted(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) const {
  // First enumerate all the accessible states based on |direction| and |step|,
  // as well as enabled feature flags. This doesn't mean each match will have
  // all of these states - just that it's possible to get there, if available.
  std::vector<OmniboxPopupSelection::LineState> all_states;
  if (step == OmniboxPopupSelection::kWholeLine ||
      step == OmniboxPopupSelection::kAllLines) {
    // In the case of whole-line stepping, only the NORMAL state is accessible.
    all_states.push_back(OmniboxPopupSelection::NORMAL);
  } else {
    // Arrow keys should never reach the header controls.
    if (step == OmniboxPopupSelection::kStateOrLine)
      all_states.push_back(OmniboxPopupSelection::FOCUSED_BUTTON_HEADER);

    all_states.push_back(OmniboxPopupSelection::NORMAL);

    // Keyword mode is accessible if the keyword search button is enabled. If
    // not, then keyword mode is only accessible by tabbing forward.
    if (OmniboxFieldTrial::IsKeywordSearchButtonEnabled() ||
        (direction == OmniboxPopupSelection::kForward &&
         step == OmniboxPopupSelection::kStateOrLine)) {
      all_states.push_back(OmniboxPopupSelection::KEYWORD_MODE);
    }

    all_states.push_back(OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH);
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    all_states.push_back(OmniboxPopupSelection::FOCUSED_BUTTON_ACTION);
#endif
    all_states.push_back(
        OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION);
  }
  DCHECK(std::is_sorted(all_states.begin(), all_states.end()))
      << "This algorithm depends on a sorted list of line states.";

  // Now, for each accessible line, add all the available line states to a list.
  std::vector<OmniboxPopupSelection> available_selections;
  {
    auto add_available_line_states_for_line = [&](size_t line) {
      for (OmniboxPopupSelection::LineState state : all_states) {
        OmniboxPopupSelection selection(line, state);
        if (IsControlPresentOnMatch(selection))
          available_selections.push_back(selection);
      }
    };

    for (size_t line = 0; line < result().size(); ++line) {
      add_available_line_states_for_line(line);
    }
  }
  DCHECK(
      std::is_sorted(available_selections.begin(), available_selections.end()))
      << "This algorithm depends on a sorted list of available selections.";
  return available_selections;
}

OmniboxPopupSelection OmniboxPopupModel::GetNextSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) const {
  if (result().empty()) {
    return selection_;
  }

  // Implementing this was like a Google Interview Problem. It was always a
  // tough problem to handle all the cases, but has gotten much harder since
  // we can now hide whole rows from view by collapsing sections.
  //
  // The only sane thing to do is to first enumerate all available selections.
  // Other approaches I've tried all end up being a jungle of branching code.
  // It's not necessarily optimal to generate this list for each keypress, but
  // in practice it's only something like ~10 elements long, and makes the code
  // easy to reason about.
  std::vector<OmniboxPopupSelection> all_available_selections =
      GetAllAvailableSelectionsSorted(direction, step);

  if (all_available_selections.empty())
    return selection_;

  // Handle the simple case of just getting the first or last element.
  if (step == OmniboxPopupSelection::kAllLines) {
    return direction == OmniboxPopupSelection::kForward
               ? all_available_selections.back()
               : all_available_selections.front();
  }

  if (direction == OmniboxPopupSelection::kForward) {
    // To go forward, we want to change to the first selection that's larger
    // than the current |selection_|, and std::upper_bound() does just that.
    const auto next =
        std::upper_bound(all_available_selections.begin(),
                         all_available_selections.end(), selection_);

    // If we can't find any selections larger than the current |selection_|
    // wrap.
    if (next == all_available_selections.end())
      return all_available_selections.front();

    // Normal case where we found the next selection.
    return *next;
  } else if (direction == OmniboxPopupSelection::kBackward) {
    // To go backwards, decrement one from std::lower_bound(), which finds the
    // current selection. I didn't use std::find() here, because
    // std::lower_bound() can gracefully handle the case where |selection_| is
    // no longer within the list of available selections.
    const auto current =
        std::lower_bound(all_available_selections.begin(),
                         all_available_selections.end(), selection_);

    // If the current selection is the first one, wrap.
    if (current == all_available_selections.begin())
      return all_available_selections.back();

    // Decrement one from the current selection.
    return *(current - 1);
  }

  NOTREACHED();
  return selection_;
}

OmniboxPopupSelection OmniboxPopupModel::StepSelection(
    OmniboxPopupSelection::Direction direction,
    OmniboxPopupSelection::Step step) {
  // This block steps the popup model, with special consideration
  // for existing keyword logic in the edit model, where ClearKeyword must be
  // called before changing the selected line.
  // AcceptKeyword should be called after changing the selected line so we don't
  // accept keyword on the wrong suggestion when stepping backwards.
  const auto old_selection = selection();
  const auto new_selection = GetNextSelection(direction, step);
  if (old_selection.IsChangeToKeyword(new_selection)) {
    edit_model()->ClearKeyword();
  }
  SetSelection(new_selection);
  if (new_selection.IsChangeToKeyword(old_selection)) {
    edit_model()->AcceptKeyword(metrics::OmniboxEventProto::TAB);
  }
  return selection_;
}

bool OmniboxPopupModel::IsControlPresentOnMatch(
    OmniboxPopupSelection selection) const {
  if (selection.line >= result().size()) {
    return false;
  }
  const auto& match = result().match_at(selection.line);
  // Skip rows that are hidden because their header is collapsed, unless the
  // user is trying to focus the header itself (which is still shown).
  if (selection.state != OmniboxPopupSelection::FOCUSED_BUTTON_HEADER &&
      match.suggestion_group_id.has_value() && pref_service_ &&
      result().IsSuggestionGroupIdHidden(pref_service_,
                                         match.suggestion_group_id.value())) {
    return false;
  }

  switch (selection.state) {
    case OmniboxPopupSelection::FOCUSED_BUTTON_HEADER: {
      // For the first match, if it a suggestion_group_id, then it has a header.
      if (selection.line == 0)
        return match.suggestion_group_id.has_value();

      // Otherwise, we only show headers that are distinct from the previous
      // match's header.
      const auto& previous_match = result().match_at(selection.line - 1);
      return match.suggestion_group_id.has_value() &&
             match.suggestion_group_id != previous_match.suggestion_group_id;
    }
    case OmniboxPopupSelection::NORMAL:
      return true;
    case OmniboxPopupSelection::KEYWORD_MODE:
      return match.associated_keyword != nullptr;
    case OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH:
      // Buttons are suppressed for matches with an associated keyword, unless
      // dedicated button row is enabled.
      if (OmniboxFieldTrial::IsKeywordSearchButtonEnabled())
        return match.has_tab_match;
      else
        return match.has_tab_match && !match.associated_keyword;
    case OmniboxPopupSelection::FOCUSED_BUTTON_ACTION:
      return match.action != nullptr;
    case OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      // Remove suggestion buttons are suppressed for matches with an associated
      // keyword, unless the feature that moves it to the button row is enabled.
      if (OmniboxFieldTrial::IsKeywordSearchButtonEnabled()) {
        return match.SupportsDeletion();
      } else {
        return !match.associated_keyword && match.SupportsDeletion();
      }
    default:
      break;
  }
  NOTREACHED();
  return false;
}

bool OmniboxPopupModel::TriggerSelectionAction(OmniboxPopupSelection selection,
                                               base::TimeTicks timestamp) {
  // Early exit for the kNoMatch case. Also exits if the calling UI passes in
  // an invalid |selection|.
  if (selection.line >= result().size())
    return false;

  auto& match = result().match_at(selection.line);
  switch (selection.state) {
    case OmniboxPopupSelection::FOCUSED_BUTTON_HEADER: {
      DCHECK(match.suggestion_group_id.has_value());

      omnibox::SuggestionGroupVisibility new_value =
          result().IsSuggestionGroupIdHidden(pref_service_,
                                             match.suggestion_group_id.value())
              ? omnibox::SuggestionGroupVisibility::SHOWN
              : omnibox::SuggestionGroupVisibility::HIDDEN;
      omnibox::SetSuggestionGroupVisibility(
          pref_service_, match.suggestion_group_id.value(), new_value);
      break;
    }
    case OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH:
      DCHECK(timestamp != base::TimeTicks());
      edit_model()->OpenMatch(match, WindowOpenDisposition::SWITCH_TO_TAB,
                              GURL(), std::u16string(), selected_line(),
                              timestamp);
      break;

    case OmniboxPopupSelection::FOCUSED_BUTTON_ACTION:
      DCHECK(timestamp != base::TimeTicks());
      DCHECK(match.action);
      edit_model()->ExecuteAction(match, selection.line, timestamp);
      break;

    case OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      TryDeletingLine(selection.line);
      break;

    default:
      // Behavior is not yet supported, return false.
      return false;
  }

  return true;
}

std::u16string OmniboxPopupModel::GetAccessibilityLabelForCurrentSelection(
    const std::u16string& match_text,
    bool include_positional_info,
    int* label_prefix_length) {
  size_t line = selection_.line;
  DCHECK_NE(line, OmniboxPopupSelection::kNoMatch)
      << "GetAccessibilityLabelForCurrentSelection should never be called if "
         "the current selection is kNoMatch.";

  const AutocompleteMatch& match = result().match_at(line);

  int additional_message_id = 0;
  std::u16string additional_message;
  switch (selection_.state) {
    case OmniboxPopupSelection::FOCUSED_BUTTON_HEADER: {
      bool group_hidden = result().IsSuggestionGroupIdHidden(
          pref_service_, match.suggestion_group_id.value());
      int message_id = group_hidden ? IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON
                                    : IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON;
      return l10n_util::GetStringFUTF16(
          message_id,
          result().GetHeaderForGroupId(match.suggestion_group_id.value()));
    }
    case OmniboxPopupSelection::NORMAL: {
      int available_actions_count = 0;
      if (IsControlPresentOnMatch(OmniboxPopupSelection(
              line, OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH))) {
        additional_message_id = IDS_ACC_TAB_SWITCH_SUFFIX;
        available_actions_count++;
      }
      if (IsControlPresentOnMatch(OmniboxPopupSelection(
              line, OmniboxPopupSelection::KEYWORD_MODE))) {
        additional_message_id = IDS_ACC_KEYWORD_SUFFIX;
        available_actions_count++;
      }
      if (IsControlPresentOnMatch(OmniboxPopupSelection(
              line, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION))) {
        additional_message =
            match.action->GetLabelStrings().accessibility_suffix;
        available_actions_count++;
      }
      if (IsControlPresentOnMatch(OmniboxPopupSelection(
              line, OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION))) {
        additional_message_id = IDS_ACC_REMOVE_SUGGESTION_SUFFIX;
        available_actions_count++;
      }
      DCHECK_EQ(OmniboxPopupSelection::LINE_STATE_MAX_VALUE, 6);
      if (available_actions_count > 1)
        additional_message_id = IDS_ACC_MULTIPLE_ACTIONS_SUFFIX;

      break;
    }
    case OmniboxPopupSelection::KEYWORD_MODE:
      additional_message_id = IDS_ACC_KEYWORD_MODE;
      break;
    case OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH:
      additional_message_id = IDS_ACC_TAB_SWITCH_BUTTON_FOCUSED_PREFIX;
      break;
    case OmniboxPopupSelection::FOCUSED_BUTTON_ACTION:
      // When pedal button is focused, the autocomplete suggestion isn't
      // read because it's not relevant to the button's action.
      return match.action->GetLabelStrings().accessibility_hint;
    case OmniboxPopupSelection::FOCUSED_BUTTON_REMOVE_SUGGESTION:
      additional_message_id = IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX;
      break;
    default:
      break;
  }
  if (additional_message_id != 0 && additional_message.empty()) {
    additional_message = l10n_util::GetStringUTF16(additional_message_id);
  }

  if (selection_.IsButtonFocused())
    include_positional_info = false;

  size_t total_matches = include_positional_info ? result().size() : 0;

  // If there's a button focused, we don't want the "n of m" message announced.
  return AutocompleteMatchType::ToAccessibilityLabel(
      match, match_text, line, total_matches, additional_message,
      label_prefix_length);
}

void OmniboxPopupModel::OnFaviconFetched(const GURL& page_url,
                                         const gfx::Image& icon) {
  if (icon.IsEmpty() || !view_->IsOpen())
    return;

  // Notify all affected matches.
  for (size_t i = 0; i < result().size(); ++i) {
    auto& match = result().match_at(i);
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.destination_url == page_url) {
      view_->OnMatchIconUpdated(i);
    }
  }
}
