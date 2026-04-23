// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/ui/tabbed_pane_enums.h"

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace autofill {

// The interface implemented by Desktop implementations of the
// `AutofillSuggestionController` - that is, the methods found below are
// specific to Desktop.
class AutofillPopupController : public AutofillSuggestionController {
 public:
  using StringFilter =
      base::StrongAlias<struct StringFilterTag, std::u16string>;

  // The suggestion filter is implemented as a string directly reflecting user
  // input or the index of the tab that the suggestion will be shown.
  using SuggestionFilter = std::variant<StringFilter, SuggestionTabIndex>;

  // Origin of a filtration request.
  enum class FilterSource {
    // The filter changed due to typing into the search bar input.
    kInputChanged,
    // The user explicitly submitted the search (e.g. by hitting Enter).
    kSearchSubmitted,
    // The user switched the active tab in the tabbed pane.
    kTabSelected,
  };

  // Suggestions consist of multiple parts (e.g., main text, labels). The filter
  // match structure reveals how a suggestion was found, enabling
  // the highlighting of these parts.
  struct SuggestionFilterMatch {
    // Shows the filter match location within the 'Suggestion::main_text'.
    gfx::Range main_text_match;

    bool operator==(const SuggestionFilterMatch& other) const = default;
  };

  // Selects the suggestion with `index`. For fillable items, this will trigger
  // preview. For other items, it does not do anything.
  virtual void SelectSuggestion(int index) = 0;

  // Unselect currently selected suggestion, noop if nothing is selected.
  virtual void UnselectSuggestion() = 0;

  // Creates and shows a sub-popup adjacent to `anchor_bounds`. The sub-popup
  // represents another level of `suggestions` which must be semantically
  // connected to a parent level suggestion, e.g. an address suggestion
  // break down providing more granular fillings.
  // The popup created via this method and this popup instance are linked
  // as child-parent. The child's lifetime depends on its parent, i.e. when
  // the parent dies the child dies also.
  virtual base::WeakPtr<AutofillSuggestionController> OpenSubPopup(
      const gfx::RectF& anchor_bounds,
      std::vector<Suggestion> suggestions,
      AutoselectFirstSuggestion autoselect_first_suggestion) = 0;

  // Hides open by `OpenSubPopup()` popup, noop if there is no open sub-popup.
  virtual void HideSubPopup() = 0;

  // Returns whether the popup should ignore the check that the mouse was
  // observed out of bounds - see `PopupRowView` for more detail.
  virtual bool ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const = 0;

  // Executes the `button_action` associated with the button that is displayed
  // in the suggestion at `index`.
  virtual void PerformButtonActionForSuggestion(
      int index,
      const SuggestionButtonAction& button_action) = 0;

  // If the filter is set, returns the same number of items as returned by
  // `AutofillSuggestionController::GetSuggestions()`, indicating how each
  // suggestion meets the filter criteria. A `std::nullopt` element means the
  // corresponding suggestion matched without a text range to highlight.
  // If the filter itself is `std::nullopt` (its default value), returns an
  // empty vector.
  // `SetFilter()` calls invalidate previously returned data.
  virtual const std::vector<std::optional<SuggestionFilterMatch>>&
  GetSuggestionFilterMatches() const = 0;

  // Sets the suggestion filter or removes it with `std::nullopt`. The filter
  // determines which suggestions are returned by GetSuggestions() and other
  // related methods (like `GetLineCount()`). When the filter changes, previous
  // suggestion indices (used in many `AutofillSuggestionController` methods,
  // e.g. `RemoveSuggestion()`) become invalid.
  virtual void SetFilter(std::optional<SuggestionFilter> filter,
                         FilterSource source) = 0;

  // Returns whethere there is at least one suggestion filtered out. It implies
  // that the filter is not empty, and if it's set to `nullopt`,
  // `GetSuggestions()` will return more suggestions.
  virtual bool HasFilteredOutSuggestions() const = 0;

  // Returns `true` if the popup should show a "no suggestions found" message.
  virtual bool ShouldShowNoSuggestionsMessage() const = 0;

  // Handles a key press event and returns whether the event should be swallowed
  // (meaning that no other handler, in particular not the default handler, can
  // process it).
  // TODO(crbug.com/325246516): Change the event type to `ui::KeyEvent` as
  // events can come not only from blink, but from native UI too.
  virtual bool HandleKeyPressEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Starts the time measurement that prevents accepting suggestions too early.
  // If the time measurement is already ongoing or has been made, this method is
  // a no-op.
  virtual void OnPopupPainted() = 0;

  // Indicates if the view should prevent accepting suggestions that are not
  // easily seen or noticed. The specific criteria for determining what's poorly
  // visible is up to the view's implementation. To avoid timer specific issues,
  // this method should return `false` in the test environment.
  virtual bool IsViewVisibilityAcceptingThresholdEnabled() const = 0;

  // Returns true if the controller is currently performing a search.
  // TODO(crbug.com/504977286) Rename when launched.
  virtual bool IsSearching() const = 0;

  // Called when a tabbed pane's tab is selected in the autofill dropdown.
  // `tab_index` and `tabbed_pane_tab_type` represent the index and the type of
  // the tab selected, respectively.
  virtual void OnTabSelected(int tab_index,
                             TabbedPaneTabType tabbed_pane_tab_type) = 0;

  virtual base::WeakPtr<AutofillPopupController> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
