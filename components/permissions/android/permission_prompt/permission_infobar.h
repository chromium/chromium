// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_INFOBAR_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_INFOBAR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionInfobar : public PermissionPromptAndroid,
                          public infobars::InfoBarManager::Observer {
 public:
  PermissionInfobar(const PermissionInfobar&) = delete;
  PermissionInfobar& operator=(const PermissionInfobar&) = delete;
  ~PermissionInfobar() override;

  PermissionPromptDisposition GetPromptDisposition() const override;

  static std::unique_ptr<PermissionInfobar> Create(
      content::WebContents* web_contents,
      Delegate* delegate);

  // InfoBar::Manager:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

 private:
  PermissionInfobar(content::WebContents* web_contents, Delegate* delegate);

  // The infobar used to display the permission request. Never assume that this
  // pointer is currently alive.
  raw_ptr<infobars::InfoBar> permission_infobar_;

  base::WeakPtrFactory<PermissionInfobar> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_INFOBAR_H_
