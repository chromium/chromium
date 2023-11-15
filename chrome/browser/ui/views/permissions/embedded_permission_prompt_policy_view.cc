// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_policy_view.h"

#include "chrome/browser/ui/url_identity.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

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
  return GetMessageText();
}

std::u16string EmbeddedPermissionPromptPolicyView::GetWindowTitle() const {
  return std::u16string();
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
  std::vector<RequestLineConfiguration> lines;
  lines.emplace_back(&vector_icons::kBusinessIcon, GetMessageText());

  return lines;
}

std::vector<EmbeddedPermissionPromptPolicyView::ButtonConfiguration>
EmbeddedPermissionPromptPolicyView::GetButtonsConfiguration() const {
  std::vector<ButtonConfiguration> buttons;
  buttons.emplace_back(l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_OK_LABEL),
                       ButtonType::kPolicyOK, ui::ButtonStyle::kTonal);
  return buttons;
}

std::u16string EmbeddedPermissionPromptPolicyView::GetMessageText() const {
  auto& requests = delegate()->Requests();
  std::u16string permission_name;
  if (requests.size() == 2) {
    permission_name = l10n_util::GetStringUTF16(
        IDS_CAMERA_AND_MICROPHONE_PERMISSION_NAME_FRAGMENT);
  } else {
    permission_name = requests[0]->GetPermissionNameTextFragment();
  }

  int template_id = is_permission_allowed_ ? IDS_EMBEDDED_PROMPT_ADMIN_ALLOWED
                                           : IDS_EMBEDDED_PROMPT_ADMIN_BLOCKED;

  return l10n_util::GetStringFUTF16(template_id, permission_name,
                                    GetUrlIdentityObject().name);
}
