// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_H_

#include <memory>

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
  virtual bool GetVisible() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_H_
