// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_PROMPT_IOS_H_
#define COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_PROMPT_IOS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/ios/content/permission_prompt/permission_dialog_delegate.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

@class NSString;

namespace content {
class WebContents;
}

namespace permissions {

class PermissionPromptIOS : public PermissionPrompt {
 public:
  PermissionPromptIOS(content::WebContents* web_contents, Delegate* delegate);

  PermissionPromptIOS(const PermissionPromptIOS&) = delete;
  PermissionPromptIOS& operator=(const PermissionPromptIOS&) = delete;

  ~PermissionPromptIOS() override;

  // PermissionPrompt:
  bool UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  bool IsAskPrompt() const override;
  std::optional<permissions::feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const override;

  virtual void Accept();
  virtual void AcceptThisTime();
  virtual void Deny();
  virtual NSString* GetPositiveButtonText(bool is_one_time) const;
  virtual NSString* GetNegativeButtonText(bool is_one_time) const;
  virtual NSString* GetPositiveEphemeralButtonText(bool is_one_time) const;

  // We show one permission at a time except for grouped mic+camera, for which
  // we still have a single icon and message text.
  size_t PermissionCount() const;
  ContentSettingsType GetContentSettingType(size_t position) const;
  virtual PermissionRequest::AnnotatedMessageText GetAnnotatedMessageText()
      const;
  virtual const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
  Requests() const;
  GURL GetRequestingOrigin() const;
  PermissionDialogDelegate* permission_dialog_delegate() const {
    return permission_dialog_delegate_;
  }

 protected:
  void CreatePermissionDialogDelegate() {
    permission_dialog_delegate_ =
        [[PermissionDialogDelegate alloc] initWithPrompt:this
                                             webContents:web_contents_];
  }

  // Check if grouped permission requests can only be Mic+Camera, Camera+Mic.
  void CheckValidRequestGroup(
      const std::vector<base::WeakPtr<PermissionRequest>>& requests) const;

 private:
  // PermissionPromptIOS is owned by PermissionRequestManager, so it should
  // be safe to hold a raw WebContents pointer here because this class is
  // destroyed before the WebContents.
  const raw_ptr<content::WebContents> web_contents_;

  // |delegate_| is the PermissionRequestManager, which owns this object.
  const raw_ptr<Delegate> delegate_;

  std::vector<base::WeakPtr<permissions::PermissionRequest>> requests_;

  // Owns a `PermissionDialogDelegate` object.
  __strong PermissionDialogDelegate* permission_dialog_delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_IOS_CONTENT_PERMISSION_PROMPT_PERMISSION_PROMPT_IOS_H_
