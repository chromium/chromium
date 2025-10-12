// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_selection.h"

#include <algorithm>

#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);

constexpr size_t OmniboxPopupSelection::kNoMatch = static_cast<size_t>(-1);

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
    const AutocompleteResult& result) const {
  if (line >= result.size()) {
    return false;
  }

  const auto& match = result.match_at(line);

  switch (state) {
    case NORMAL:
      // `NULL_RESULT_MESSAGE` cannot be focused.
      return match.type != AutocompleteMatchType::NULL_RESULT_MESSAGE;
    case KEYWORD_MODE:
      return !match.associated_keyword.empty();
    case FOCUSED_BUTTON_ACTION: {
      // Actions buttons should not be shown in keyword mode.
      return !match.from_keyword && action_index < match.actions.size();
    }
    case FOCUSED_BUTTON_THUMBS_UP:
    case FOCUSED_BUTTON_THUMBS_DOWN:
      return match.type == AutocompleteMatchType::HISTORY_EMBEDDINGS;
    case FOCUSED_BUTTON_REMOVE_SUGGESTION:
      return match.SupportsDeletion();
    case FOCUSED_IPH_LINK:
      return match.IsIphSuggestion() && !match.iph_link_url.is_empty();
    default:
      break;
  }
  NOTREACHED();
}

OmniboxPopupSelection OmniboxPopupSelection::GetNextSelection(
    const AutocompleteInput& input,
    const AutocompleteResult& result,
    TemplateURLService* template_url_service,
    bool aim_button_visible,
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
      GetAllAvailableSelectionsSorted(input, result, template_url_service,
                                      aim_button_visible, step);

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

  NOTREACHED();
}

// static
std::vector<OmniboxPopupSelection>
OmniboxPopupSelection::GetAllAvailableSelectionsSorted(
    const AutocompleteInput& input,
    const AutocompleteResult& result,
    TemplateURLService* template_url_service,
    bool aim_button_visible,
    Step step) {
  // First enumerate all the accessible states based on `direction` and `step`,
  // as well as enabled feature flags. This doesn't mean each match will have
  // all of these states - just that it's possible to get there, if available.
  std::vector<LineState> all_states;
  switch (step) {
    case kWholeLine:
    case kAllLines:
      all_states.push_back(NORMAL);
      // Whole line stepping can go straight into keyword mode.
      all_states.push_back(KEYWORD_MODE);
      break;
    case kStateOrLine:
      all_states.push_back(NORMAL);
      all_states.push_back(KEYWORD_MODE);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      all_states.push_back(FOCUSED_BUTTON_AIM);
      all_states.push_back(FOCUSED_BUTTON_ACTION);
#endif
      all_states.push_back(FOCUSED_BUTTON_THUMBS_UP);
      all_states.push_back(FOCUSED_BUTTON_THUMBS_DOWN);
      all_states.push_back(FOCUSED_BUTTON_REMOVE_SUGGESTION);
      all_states.push_back(FOCUSED_IPH_LINK);
      break;
  }
  DCHECK(std::is_sorted(all_states.begin(), all_states.end()))
      << "Line states must be added in sorted order for the algorithm to work.";

  std::vector<OmniboxPopupSelection> available_selections;
  // The AIM button is included as a special case selection on the `kNoMatch`
  // line if:
  // - The AIM button is visible,
  // - The user is moving focus with tab or shift-tab (`kStateOrLine`).
  // - The user is in zero suggest state. When they're not in zero suggest, the
  //   AIM button selection will instead be added to the default match, below.
  // Note that the ordering logic in `operator<=>` ensures that `kNoMatch` comes
  // before other selections.
  if (aim_button_visible && step == kStateOrLine && input.IsZeroSuggest()) {
    available_selections.emplace_back(kNoMatch, FOCUSED_BUTTON_AIM);
  }
  // Now, for each accessible line, add all the available line states to a list.
  for (size_t line_number = 0; line_number < result.size(); ++line_number) {
    for (LineState line_state : all_states) {
      if (line_state == FOCUSED_BUTTON_AIM) {
        // The AIM button is included in the focus order if:
        // - The AIM button is visible.
        // - This is the first match (`line_number == 0`).
        // - The match is not from a keyword; i.e. user didn't type
        //   'youtube<tab>query'. Checking `from_keyword` isn't strictly
        //   necessary because `aim_button_visible` should be false in this
        //   case, but it's good to check and not depend on button visibility
        //   logic staying constant.
        // - The omnibox is not tabbed into keyword mode; i.e. user didn't type
        //   'youtube<tab>'. Otherwise, tab traversal wouldn't be consistent
        //   when tabbing v shift+tabbing. E.g. 'youtube<tab><tab><shift+tab>'
        //   would skip the AI button tabbing forward but not backward. Checking
        //   `associated_keyword` suffices though it also catches the case where
        //   before the user has tabbed into keyword mode; but that's ok since
        //   `FOCUSED_BUTTON_AIM` is ordered after `KEYWORD_MODE` anyways.
        // - The match does not have other actions, e.g. switch to tab, to avoid
        //   disrupting muscle memory and for consistency with keyword chips.
        // - The 2nd match isn't an instant keyword to avoid disrupting muscle
        //   memory e.g. '@gemini<tab>'. Don't have to similarly consider
        //   non-instant keywords since same-line `KEYWORD_MODE` is ordered
        //   before `FOCUSED_BUTTON_AIM`.
        // - The input is not ZeroSuggest (i.e., the user has typed something).
        bool second_match_has_instant_keyword =
            result.size() >= 2 &&
            result.match_at(1).HasInstantKeyword(template_url_service);
        if (aim_button_visible && line_number == 0 &&
            !result.match_at(0).from_keyword &&
            result.match_at(0).associated_keyword.empty() &&
            result.match_at(0).actions.size() == 0 &&
            !second_match_has_instant_keyword && !input.IsZeroSuggest()) {
          available_selections.emplace_back(line_number, line_state);
        }
      } else if (line_state == FOCUSED_BUTTON_ACTION) {
        constexpr size_t kMaxActionCount = 8;
        for (size_t i = 0; i < kMaxActionCount; i++) {
          OmniboxPopupSelection selection(line_number, line_state, i);
          if (selection.IsControlPresentOnMatch(result)) {
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
        if (selection.IsControlPresentOnMatch(result)) {
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
        if (selection.IsControlPresentOnMatch(result)) {
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
