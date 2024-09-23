// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_selection.h"

#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/template_url_service.h"

#include <algorithm>

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);

const size_t OmniboxPopupSelection::kNoMatch = static_cast<size_t>(-1);

bool OmniboxPopupSelection::operator==(const OmniboxPopupSelection& b) const {
  return line == b.line && state == b.state && action_index == b.action_index;
}

bool OmniboxPopupSelection::operator!=(const OmniboxPopupSelection& b) const {
  return !operator==(b);
}

bool OmniboxPopupSelection::operator<(const OmniboxPopupSelection& b) const {
  if (line == b.line) {
    if (state == b.state) {
      return action_index < b.action_index;
    }
    return state < b.state;
  }
  return line < b.line;
}

bool OmniboxPopupSelection::IsChangeToKeyword(
    OmniboxPopupSelection from) const {
  return state == KEYWORD_MODE && from.state != KEYWORD_MODE;
}

bool OmniboxPopupSelection::IsButtonFocused() const {
  return state != NORMAL && state != KEYWORD_MODE;
}

bool OmniboxPopupSelection::IsAction() const {
  return state == FOCUSED_BUTTON_ACTION;
}

bool OmniboxPopupSelection::IsControlPresentOnMatch(
    const AutocompleteResult& result,
    const PrefService* pref_service) const {
  if (line >= result.size()) {
    return false;
  }
  const auto& match = result.match_at(line);
  // Skip rows that are hidden because their header is collapsed, unless the
  // user is trying to focus the header itself (which is still shown).
  if (state != FOCUSED_BUTTON_HEADER && match.suggestion_group_id.has_value() &&
      pref_service &&
      result.IsSuggestionGroupHidden(pref_service,
                                     match.suggestion_group_id.value())) {
    return false;
  }

  switch (state) {
    case FOCUSED_BUTTON_HEADER: {
      // Trivial case where there's no header at all.
      if (!match.suggestion_group_id.has_value()) {
        return false;
      }
      // Empty string headers are not rendered and should not be traversed.
      if (result.GetHeaderForSuggestionGroup(match.suggestion_group_id.value())
              .empty()) {
        return false;
      }

      // Now we know there's an existing header. First line header is always
      // distinct from the previous match (because there is no previous match).
      if (line == 0) {
        return true;
      }

      // Otherwise, we verify that this header is distinct from the previous
      // match's header.
      const auto& previous_match = result.match_at(line - 1);
      return match.suggestion_group_id != previous_match.suggestion_group_id;
    }
    case NORMAL:
      // `NULL_RESULT_MESSAGE` cannot be focused.
      return match.type != AutocompleteMatchType::NULL_RESULT_MESSAGE;
    case KEYWORD_MODE:
      return match.associated_keyword != nullptr;
    case FOCUSED_BUTTON_ACTION: {
      // Actions buttons should not be shown in keyword mode.
      return !match.from_keyword && action_index < match.actions.size();
    }
    case FOCUSED_BUTTON_THUMBS_UP:
    case FOCUSED_BUTTON_THUMBS_DOWN:
      return match.type == AutocompleteMatchType::HISTORY_EMBEDDINGS;
    case FOCUSED_BUTTON_REMOVE_SUGGESTION:
      return match.SupportsDeletion();
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

OmniboxPopupSelection OmniboxPopupSelection::GetNextSelection(
    const AutocompleteResult& result,
    const PrefService* pref_service,
    TemplateURLService* template_url_service,
    Direction direction,
    Step step) const {
  if (result.empty()) {
    return *this;
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
      GetAllAvailableSelectionsSorted(result, pref_service,
                                      template_url_service, direction, step);

  if (all_available_selections.empty()) {
    return *this;
  }

  // Handle the simple case of just getting the first or last element.
  if (step == kAllLines) {
    return direction == kForward ? all_available_selections.back()
                                 : all_available_selections.front();
  }

  if (direction == kForward) {
    // To go forward, we want to change to the first selection that's larger
    // than the current selection, and std::upper_bound() does just
    // that.
    const auto next = std::upper_bound(all_available_selections.begin(),
                                       all_available_selections.end(), *this);

    // If we can't find any selections larger than the current
    // selection, wrap.
    if (next == all_available_selections.end())
      return all_available_selections.front();

    // Normal case where we found the next selection.
    return *next;
  } else if (direction == kBackward) {
    // To go backwards, decrement one from std::lower_bound(), which finds the
    // current selection. I didn't use std::find() here, because
    // std::lower_bound() can gracefully handle the case where
    // selection is no longer within the list of available selections.
    const auto current =
        std::lower_bound(all_available_selections.begin(),
                         all_available_selections.end(), *this);

    // If the current selection is the first one, wrap.
    if (current == all_available_selections.begin()) {
      return all_available_selections.back();
    }

    // Decrement one from the current selection.
    return *(current - 1);
  }

  NOTREACHED_IN_MIGRATION();
  return *this;
}

// static
std::vector<OmniboxPopupSelection>
OmniboxPopupSelection::GetAllAvailableSelectionsSorted(
    const AutocompleteResult& result,
    const PrefService* pref_service,
    TemplateURLService* template_url_service,
    Direction direction,
    Step step) {
  // First enumerate all the accessible states based on `direction` and `step`,
  // as well as enabled feature flags. This doesn't mean each match will have
  // all of these states - just that it's possible to get there, if available.
  std::vector<LineState> all_states;
  if (step == kWholeLine || step == kAllLines) {
    all_states.push_back(NORMAL);
    // Whole line stepping can go straight into keyword mode.
    all_states.push_back(KEYWORD_MODE);
  } else {
    // Arrow keys should never reach the header controls.
    if (step == kStateOrLine) {
      all_states.push_back(FOCUSED_BUTTON_HEADER);
    }

    all_states.push_back(NORMAL);
    all_states.push_back(KEYWORD_MODE);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    all_states.push_back(FOCUSED_BUTTON_ACTION);
#endif
    all_states.push_back(FOCUSED_BUTTON_THUMBS_UP);
    all_states.push_back(FOCUSED_BUTTON_THUMBS_DOWN);
    all_states.push_back(FOCUSED_BUTTON_REMOVE_SUGGESTION);
  }
  DCHECK(std::is_sorted(all_states.begin(), all_states.end()))
      << "This algorithm depends on a sorted list of line states.";

  // Now, for each accessible line, add all the available line states to a list.
  std::vector<OmniboxPopupSelection> available_selections;
  for (size_t line_number = 0; line_number < result.size(); ++line_number) {
    for (LineState line_state : all_states) {
      if (line_state == FOCUSED_BUTTON_ACTION) {
        constexpr size_t kMaxActionCount = 8;
        for (size_t i = 0; i < kMaxActionCount; i++) {
          OmniboxPopupSelection selection(line_number, line_state, i);
          if (selection.IsControlPresentOnMatch(result, pref_service)) {
            available_selections.push_back(selection);
          } else {
            // Break early when there are no more actions. Note, this
            // implies that a match takeover action should be last
            // to allow other actions on the match to be included.
            break;
          }
        }
      } else if (line_state == KEYWORD_MODE && kIsDesktop) {
        OmniboxPopupSelection selection(line_number, line_state);
        if (selection.IsControlPresentOnMatch(result, pref_service)) {
          if (result.match_at(line_number)
                  .HasInstantKeyword(template_url_service)) {
            if (available_selections.size() > 0 &&
                available_selections.back().line == line_number &&
                available_selections.back().state == LineState::NORMAL) {
              // Remove the preceding normal state selection so that keyword
              // mode will be entered immediately when the user arrows down
              // to this keyword line.
              available_selections.pop_back();
            }
            available_selections.push_back(selection);
          } else if (step == kStateOrLine) {
            available_selections.push_back(selection);
          }
        }
      } else {
        OmniboxPopupSelection selection(line_number, line_state);
        if (selection.IsControlPresentOnMatch(result, pref_service)) {
          available_selections.push_back(selection);
        }
      }
    }
  }
  DCHECK(
      std::is_sorted(available_selections.begin(), available_selections.end()))
      << "This algorithm depends on a sorted list of available selections.";
  return available_selections;
}
