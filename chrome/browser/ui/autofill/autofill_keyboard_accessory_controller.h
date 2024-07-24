// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"

namespace autofill {

// The interface implemented by Android implementations of the
// `AutofillSuggestionController` - that is, the methods found below are
// specific to Android and used to control the keyboard accessory.
class AutofillKeyboardAccessoryController
    : public AutofillSuggestionController {
 public:
  // Returns all the labels of the suggestion at the given `row` index. The
  // labels are presented as a N*M matrix, and the position of the text in the
  // matrix decides where the text will be shown on the UI. (e.g. The text
  // labels[1][2] will be shown on the second line, third column in the grid
  // view of label).
  virtual std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const = 0;

  // Returns whether the item at `list_index` can be removed. If so, fills
  // out `title` and `body` (when non-null) with relevant user-facing text.
  virtual bool GetRemovalConfirmationText(int index,
                                          std::u16string* title,
                                          std::u16string* body) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_H_
