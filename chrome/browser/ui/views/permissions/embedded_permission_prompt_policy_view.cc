// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"

#include "chrome/browser/ui/url_identity.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"

EmbeddedPermissionPromptPolicyView::EmbeddedPermissionPromptPolicyView(
    Browser* browser,
    base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate,
    bool is_permission_allowed)
    : EmbeddedPermissionPromptBaseView(browser, delegate),
      is_permission_allowed_(is_permission_allowed) {}

EmbeddedPermissionPromptPolicyView::~EmbeddedPermissionPromptPolicyView() =
    default;

std::u16string EmbeddedPermissionPromptPolicyView::GetAccessibleWindowTitle()
    const {
  return GetWindowTitle();
}

std::u16string EmbeddedPermissionPromptPolicyView::GetWindowTitleAdminAllowed()
    const {
  auto& requests = delegate()->Requests();
  std::u16string permission_name;
  if (requests.size() == 2) {
    permission_name = l10n_util::GetStringUTF16(
        IDS_CAMERA_AND_MICROPHONE_PERMISSION_NAME_FRAGMENT);
  } else {
    permission_name = requests[0]->GetPermissionNameTextFragment();
  }

  return l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_ADMIN_ALLOWED,
                                    permission_name);
}

std::u16string EmbeddedPermissionPromptPolicyView::GetWindowTitleAdminBlocked()
    const {
  auto& requests = delegate()->Requests();
  std::u16string permission_name;
  if (requests.size() == 2) {
    permission_name = l10n_util::GetStringUTF16(
        IDS_CAMERA_AND_MICROPHONE_PERMISSION_NAME_FRAGMENT);
  } else {
    permission_name = requests[0]->GetPermissionNameTextFragment();
  }

  return l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_ADMIN_BLOCKED,
                                    permission_name);
}

std::u16string EmbeddedPermissionPromptPolicyView::GetWindowTitle() const {
  return is_permission_allowed_ ? GetWindowTitleAdminAllowed()
                                : GetWindowTitleAdminBlocked();
}

const gfx::VectorIcon& EmbeddedPermissionPromptPolicyView::GetIcon() const {
  return vector_icons::kBusinessIcon;
}

void EmbeddedPermissionPromptPolicyView::RunButtonCallback(int button_id) {
  ButtonType button = GetButtonType(button_id);
  DCHECK_EQ(button, ButtonType::kPolicyOK);

  if (delegate()) {
    delegate()->Acknowledge();
  }
}

std::vector<EmbeddedPermissionPromptPolicyView::RequestLineConfiguration>
EmbeddedPermissionPromptPolicyView::GetRequestLinesConfiguration() const {
  return {};
}

std::vector<EmbeddedPermissionPromptPolicyView::ButtonConfiguration>
EmbeddedPermissionPromptPolicyView::GetButtonsConfiguration() const {
  std::vector<ButtonConfiguration> buttons;
  buttons.emplace_back(l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_OK_LABEL),
                       ButtonType::kPolicyOK, ui::ButtonStyle::kTonal);
  return buttons;
}

BEGIN_METADATA(EmbeddedPermissionPromptPolicyView)
END_METADATA
