// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
#define CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace chrome {

enum MessageBoxResult {
  // User chose NO or CANCEL. If there's a checkbox, then the checkbox was
  // unchecked.
  MESSAGE_BOX_RESULT_NO = 0,

  // User chose YES or OK. If there's a checkbox, then the checkbox was checked.
  MESSAGE_BOX_RESULT_YES = 1,

  // Message box was displayed asynchronously and is pending a real result,
  // which will be delivered via callback.
  MESSAGE_BOX_RESULT_DEFERRED = 2,
};

enum MessageBoxType {
  MESSAGE_BOX_TYPE_WARNING,   // Shows an OK button.
  MESSAGE_BOX_TYPE_QUESTION,  // Shows YES and NO buttons.
};

// Shows a dialog box with the given |title| and |message|. If |parent| is
// non-NULL, the box will be made modal to the |parent|, except on Mac, where it
// is always app-modal.
//
// If |can_close| is false, then this dialog will not show the close button and
// the dialog will only be dismissed when the user presses the OK button.
//
// NOTE: In general, you should avoid this since it's usually poor UI.
// We have a variety of other surfaces such as app menu notifications and
// infobars; consult the UI leads for a recommendation.
MessageBoxResult ShowWarningMessageBox(gfx::NativeWindow parent,
                                       const std::u16string& title,
                                       const std::u16string& message);

// As above, but shows the dialog box asynchronously with a checkbox.
// |callback| will be invoked after the dialog is dismissed. It is invoked with
// true if the checkbox is checked and false otherwise.
void ShowWarningMessageBoxWithCheckbox(
    gfx::NativeWindow parent,
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& checkbox_text,
    base::OnceCallback<void(bool checked)> callback);

// As above, but two buttons are displayed and the return value indicates which
// is chosen.
MessageBoxResult ShowQuestionMessageBoxSync(gfx::NativeWindow parent,
                                            const std::u16string& title,
                                            const std::u16string& message);

void ShowQuestionMessageBox(
    gfx::NativeWindow parent,
    const std::u16string& title,
    const std::u16string& message,
    base::OnceCallback<void(MessageBoxResult)> callback);

// Shows a dialog box with the given |title| and |message|, and with two buttons
// labeled with |yes_text| and |no_text|. If |parent| is non-NULL, the box will
// be made modal to the |parent|.  (Aura only.)
//
// NOTE: In general, you should avoid this since it's usually poor UI.
// We have a variety of other surfaces such as app menu notifications and
// infobars; consult the UI leads for a recommendation.
MessageBoxResult ShowMessageBoxWithButtonText(gfx::NativeWindow parent,
                                              const std::u16string& title,
                                              const std::u16string& message,
                                              const std::u16string& yes_text,
                                              const std::u16string& no_text);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SIMPLE_MESSAGE_BOX_H_
