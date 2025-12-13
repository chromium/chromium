// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_dialog.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

PermissionDialog::PermissionDialog(content::WebContents* web_contents,
                                   Delegate* delegate)
    : PermissionPromptIOS(web_contents, delegate) {
  DCHECK(web_contents);
  DCHECK(delegate);
  CreatePermissionDialogDelegate();
}

// static
std::unique_ptr<PermissionDialog> PermissionDialog::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return std::make_unique<PermissionDialog>(web_contents, delegate);
}

PermissionPromptDisposition PermissionDialog::GetPromptDisposition() const {
  return PermissionPromptDisposition::MODAL_DIALOG;
}

NSString* PermissionDialog::GetPositiveButtonText(bool is_one_time) const {
  return is_one_time ? base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                           IDS_PERMISSION_ALLOW_WHILE_VISITING))
                     : base::SysUTF16ToNSString(
                           l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
}

NSString* PermissionDialog::GetNegativeButtonText(bool is_one_time) const {
  return is_one_time ? base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                           IDS_PERMISSION_NEVER_ALLOW))
                     : base::SysUTF16ToNSString(
                           l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
}

NSString* PermissionDialog::GetPositiveEphemeralButtonText(
    bool is_one_time) const {
  return is_one_time ? base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                           IDS_PERMISSION_ALLOW_THIS_TIME))
                     : @"";
}

}  // namespace permissions
