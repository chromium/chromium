// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_INFOBAR_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_INFOBAR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"

namespace content {
class WebContents;
}

namespace permissions {

class PermissionPromptInfoBar : public PermissionPromptAndroid,
                                public infobars::InfoBarManager::Observer {
 public:
  PermissionPromptInfoBar(const PermissionPromptInfoBar&) = delete;
  PermissionPromptInfoBar& operator=(const PermissionPromptInfoBar&) = delete;
  ~PermissionPromptInfoBar() override;

  PermissionPromptDisposition GetPromptDisposition() const override;

  static std::unique_ptr<PermissionPromptInfoBar> Create(
      content::WebContents* web_contents,
      Delegate* delegate);

  // InfoBar::Manager:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

 private:
  PermissionPromptInfoBar(content::WebContents* web_contents,
                          Delegate* delegate);

  // The infobar used to display the permission request. Never assume that this
  // pointer is currently alive.
  raw_ptr<infobars::InfoBar> permission_prompt_infobar_;

  base::WeakPtrFactory<PermissionPromptInfoBar> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_PROMPT_INFOBAR_H_
