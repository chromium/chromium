// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_DELEGATE_H_

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Delegate for SendTabToSelfToolbarIconController that is told when to show
// by the controller.
class SendTabToSelfToolbarIconControllerDelegate {
 public:
  virtual void Show(const SendTabToSelfEntry& entry) = 0;

  virtual bool IsActive() = 0;

 protected:
  virtual ~SendTabToSelfToolbarIconControllerDelegate() = default;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_ICON_CONTROLLER_DELEGATE_H_
