// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"

namespace autofill {

// This interface provides data to an AutofillPopupView.
class AutofillPopupController : public AutofillPopupViewDelegate {
 public:
  // Recalculates the height and width of the popup and triggers a redraw when
  // suggestions change.
  virtual void OnSuggestionsChanged() = 0;

  // Selects the suggestion with `index`. For fillable items, this will trigger
  // preview. For other items, it does not do anything.
  virtual void SelectSuggestion(absl::optional<size_t> index) = 0;

  // Accepts the suggestion at `index`. The suggestion will only be accepted if
  // the popup has been shown for at least `show_threshold` to allow
  // ruling out accidental popup interactions (crbug.com/1279268).
  virtual void AcceptSuggestion(int index, base::TimeDelta show_threshold) = 0;

  // Removes the suggestion at the given index.
  virtual bool RemoveSuggestion(int index) = 0;

  // Returns the number of lines of data that there are.
  virtual int GetLineCount() const = 0;

  // Returns the full set of autofill suggestions, if applicable.
  virtual std::vector<Suggestion> GetSuggestions() const = 0;

  // Returns the suggestion at the given |row| index. The |Suggestion| is the
  // data model including information that is to be shown in the UI.
  virtual const Suggestion& GetSuggestionAt(int row) const = 0;

  // Returns the suggestion main text string at the given |row| index. The main
  // text is shown in primary or secondary text style, serving as the title of
  // the suggestion.
  virtual std::u16string GetSuggestionMainTextAt(int row) const = 0;

  // Returns the suggestion minor text string at the given |row| index. The
  // minor text is shown in secondary text style, serving as the sub-title of
  // the suggestion item.
  virtual std::u16string GetSuggestionMinorTextAt(int row) const = 0;

  // Returns all the labels of the suggestion at the given |row| index. The
  // labels are presented as a N*M matrix, and the position of the text in the
  // matrix decides where the text will be shown on the UI. (e.g. The text
  // labels[1][2] will be shown on the second line, third column in the grid
  // view of label).
  virtual std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const = 0;

  // Returns whether the item at |list_index| can be removed. If so, fills
  // out |title| and |body| (when non-null) with relevant user-facing text.
  virtual bool GetRemovalConfirmationText(int index,
                                          std::u16string* title,
                                          std::u16string* body) = 0;

  // Returns the popup type corresponding to the controller.
  virtual PopupType GetPopupType() const = 0;

 protected:
  ~AutofillPopupController() override = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
