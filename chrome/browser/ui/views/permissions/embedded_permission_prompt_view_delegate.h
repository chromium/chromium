// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_VIEW_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"

// Interface class for `EmbeddedPermissionPrompt`, which is the presenter of
// embedded permission prompt views. This interface is used by the various
// relevant views to communicate with the presenter.
class EmbeddedPermissionPromptViewDelegate {
 public:
  virtual void Allow() = 0;
  virtual void AllowThisTime() = 0;
  virtual void Dismiss() = 0;
  virtual void Acknowledge() = 0;
  virtual void StopAllowing() = 0;
  virtual void ShowSystemSettings() = 0;

  // Return a weak pointer of `PermissionPrompt::Delegate` which is implemnted
  // in `components` layer
  virtual base::WeakPtr<permissions::PermissionPrompt::Delegate>
  GetPermissionPromptDelegate() const = 0;

  // Requests list the current prompt view is representing for.
  virtual const std::vector<
      raw_ptr<permissions::PermissionRequest, VectorExperimental>>&
  Requests() const = 0;

 protected:
  virtual ~EmbeddedPermissionPromptViewDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_VIEW_DELEGATE_H_
