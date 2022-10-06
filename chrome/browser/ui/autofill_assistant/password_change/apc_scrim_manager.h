// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_H_

#include <memory>

#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

// Adds a scrim over the webcontent during a password change run.
// TODO(crbug.com/1341037): Implement feature to optionally stop a APC run
// after 3 or more clicks over the scrim.
class ApcScrimManager {
 public:
  static std::unique_ptr<ApcScrimManager> Create(
      content::WebContents* web_contents);

  virtual ~ApcScrimManager() = default;

  virtual void Show() = 0;
  virtual void Hide() = 0;
  // Shuts down a scrim manager. Called at the end an APC run, it hides the
  // currently shown scrim and marks it as disabled. After shutdown, the scrim
  // will no longer be shown, unless `SetIsDisabled` is called to enable it
  // again.
  virtual void ShutDown() = 0;
  virtual bool GetVisible() const = 0;
  // Sets a scrim manager as disabled.
  // When set the scrim will always be hidden regardless of whether `Show` is
  // called or not. A disabled scrim can only be hidden but not shown again.
  virtual void SetIsDisabled(bool is_disabled) = 0;
  virtual bool GetIsDisabled() const = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_H_
