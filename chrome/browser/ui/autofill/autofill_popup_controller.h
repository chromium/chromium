// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace autofill {

// The interface implemented by Desktop implementations of the
// `AutofillSuggestionController` - that is, the methods found below are
// specific to Desktop.
class AutofillPopupController : public AutofillSuggestionController {
 public:
  // The suggestion filter is implemented as a string directly reflecting user
  // input. It could be refactored into a more complex data structure to enable
  // advanced search functionality.
  using SuggestionFilter =
      base::StrongAlias<struct SuggestionFilterTag, std::u16string>;

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
  // suggestion meets the filter criteria. Otherwise, if the filter is
  // `std::nullopt` (its default value), returns an empty vector.
  // `SetFilter()` calls invalidate previously returned data.
  virtual const std::vector<SuggestionFilterMatch>& GetSuggestionFilterMatches()
      const = 0;

  // Sets the suggestion filter or removes it with `std::nullopt`. The filter
  // determines which suggestions are returned by GetSuggestions() and other
  // related methods (like `GetLineCount()`). When the filter changes, previous
  // suggestion indices (used in many `AutofillSuggestionController` methods,
  // e.g. `RemoveSuggestion()`) become invalid.
  virtual void SetFilter(std::optional<SuggestionFilter> filter) = 0;

  // Returns whethere there is at least one suggestion filtered out. It implies
  // that the filter is not empty, and if it's set to `nullopt`,
  // `GetSuggestions()` will return more suggestions.
  virtual bool HasFilteredOutSuggestions() const = 0;

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

  virtual base::WeakPtr<AutofillPopupController> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
