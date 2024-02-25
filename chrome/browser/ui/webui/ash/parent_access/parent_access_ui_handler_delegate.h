// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_HANDLER_DELEGATE_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"

namespace ash {

// Interface for the ParentAccessUiHandlerDelegate.  Declares behaviors that
// the handler requires of components that communicate with it.
class ParentAccessUiHandlerDelegate {
 public:
  // Clones the incoming ParentAccessParams and returns ownership to the caller.
  virtual parent_access_ui::mojom::ParentAccessParamsPtr
  CloneParentAccessParams() = 0;

  // The following methods, when called, indicate that the UI has reached a
  // terminal state.

  // Indicates to the delegate that the request was approved, with the specified
  // parent access token and expire timestamp.
  virtual void SetApproved(const std::string& parent_access_token,
                           const base::Time& expire_timestamp) = 0;
  // Indicates to the delegate that the request was declined.
  virtual void SetDeclined() = 0;
  // Indicates to the delegate that the request was canceled.
  virtual void SetCanceled() = 0;
  // Indicates to the delegate that making a request is disabled.
  virtual void SetDisabled() = 0;
  // Indicates to the delegate that an error occurred.
  virtual void SetError() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_HANDLER_DELEGATE_H_
