// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"

namespace autofill {

struct Suggestion;

// This interface provides data to an AutofillPopupView.
class AutofillPopupController : public AutofillPopupViewDelegate {
 public:
  // Recalculates the height and width of the popup and triggers a redraw when
  // suggestions change.
  virtual void OnSuggestionsChanged() = 0;

  // Accepts the suggestion at |index|.
  virtual void AcceptSuggestion(int index) = 0;

  // Returns the number of lines of data that there are.
  virtual int GetLineCount() const = 0;

  // Returns the full set of autofill suggestions, if applicable.
  virtual std::vector<Suggestion> GetSuggestions() const = 0;

  // Returns the suggestion at the given |row| index.
  virtual const Suggestion& GetSuggestionAt(int row) const = 0;

  // Returns the suggestion value string at the given |row| index.
  virtual const std::u16string& GetSuggestionValueAt(int row) const = 0;

  // Returns the suggestion label string at the given |row| index.
  virtual const std::u16string& GetSuggestionLabelAt(int row) const = 0;

  // Returns whether the item at |list_index| can be removed. If so, fills
  // out |title| and |body| (when non-null) with relevant user-facing text.
  virtual bool GetRemovalConfirmationText(int index,
                                          std::u16string* title,
                                          std::u16string* body) = 0;

  // Removes the suggestion at the given index.
  virtual bool RemoveSuggestion(int index) = 0;

  // Change which line is currently selected by the user.
  virtual void SetSelectedLine(base::Optional<int> selected_line) = 0;

  // Returns the index of the selected line. A line is "selected" when it is
  // hovered or has keyboard focus.
  virtual base::Optional<int> selected_line() const = 0;

  // Returns the popup type corresponding to the controller.
  virtual PopupType GetPopupType() const = 0;

 protected:
  ~AutofillPopupController() override = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
