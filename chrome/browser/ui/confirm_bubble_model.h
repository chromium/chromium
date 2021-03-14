// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONFIRM_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_CONFIRM_BUBBLE_MODEL_H_

#include <string>

#include "base/macros.h"
#include "ui/base/ui_base_types.h"
#include "url/gurl.h"

// An interface implemented by objects wishing to control an ConfirmBubbleView.
// To use this class to implement a bubble menu, we need two steps:
// 1. Implement a class derived from this class.
// 2. Call chrome::ShowConfirmBubble() with the class implemented in 1.
class ConfirmBubbleModel {
 public:
  ConfirmBubbleModel();
  virtual ~ConfirmBubbleModel();

  // Returns the title string and the message string to be displayed for this
  // bubble menu. These must not be empty strings.
  virtual std::u16string GetTitle() const = 0;
  virtual std::u16string GetMessageText() const = 0;

  // Return the label for the specified button. The default implementation
  // returns "OK" for the OK button and "Cancel" for the Cancel button.
  virtual std::u16string GetButtonLabel(ui::DialogButton button) const;

  // Called when the OK button is pressed.
  virtual void Accept();

  // Called when the Cancel button is pressed.
  virtual void Cancel();

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // an empty string.
  virtual std::u16string GetLinkText() const;

  // Returns the URL of the link to be displayed.
  virtual GURL GetHelpPageURL() const;

  // Called when the link is clicked.
  virtual void OpenHelpPage();

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleModel);
};

#endif  // CHROME_BROWSER_UI_CONFIRM_BUBBLE_MODEL_H_
