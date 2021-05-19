// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_CONTROLLER_DELEGATE_H_

namespace send_tab_to_self {

// Delegate for SendTabToSelfToolbarButtonController that is told when to show
// by the controller.
class SendTabToSelfToolbarButtonControllerDelegate {
 public:
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  virtual ~SendTabToSelfToolbarButtonControllerDelegate() = default;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUTTON_CONTROLLER_DELEGATE_H_
