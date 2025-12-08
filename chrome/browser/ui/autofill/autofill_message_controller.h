// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_H_

#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillMessageController {
 public:
  virtual ~AutofillMessageController() = default;

  // Show a new message. If an existing message is already showing, dismiss that
  // message and show the new one.
  virtual void Show(std::unique_ptr<AutofillMessageModel> message_model) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_MESSAGE_CONTROLLER_H_
