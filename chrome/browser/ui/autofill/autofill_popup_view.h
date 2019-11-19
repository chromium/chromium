// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_

#include <stddef.h>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"

namespace autofill {

class AutofillPopupController;

// The interface for creating and controlling a platform-dependent
// AutofillPopupView.
class AutofillPopupView {
 public:
  // Displays the Autofill popup and fills it in with data from the controller.
  virtual void Show() = 0;

  // Hides the popup from view. This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // If not null, invalidates the given rows and redraws them.
  virtual void OnSelectedRowChanged(
      base::Optional<int> previous_row_selection,
      base::Optional<int> current_row_selection) = 0;

  // Refreshes the position and redraws popup when suggestions change.
  virtual void OnSuggestionsChanged() = 0;

  // Return the autofill popup view's ax unique id.
  virtual base::Optional<int32_t> GetAxUniqueId() = 0;

  // Factory function for creating the view.
  static AutofillPopupView* Create(
      base::WeakPtr<AutofillPopupController> controller);

 protected:
  virtual ~AutofillPopupView() {}
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
