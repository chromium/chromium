// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"

#include "base/memory/weak_ptr.h"

#include "chrome/browser/ui/url_identity.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

EmbeddedPermissionPromptSystemSettingsView::
    EmbeddedPermissionPromptSystemSettingsView(Browser* browser,
                                               base::WeakPtr<Delegate> delegate)
    : EmbeddedPermissionPromptBaseView(browser, delegate) {}

EmbeddedPermissionPromptSystemSettingsView::
    ~EmbeddedPermissionPromptSystemSettingsView() = default;

std::u16string
EmbeddedPermissionPromptSystemSettingsView::GetAccessibleWindowTitle() const {
  return GetMessageText();
}

std::u16string EmbeddedPermissionPromptSystemSettingsView::GetWindowTitle()
    const {
  return std::u16string();
}

void EmbeddedPermissionPromptSystemSettingsView::RunButtonCallback(
    int button_id) {
  ButtonType button = GetButtonType(button_id);
  DCHECK_EQ(button, ButtonType::kSystemSettings);

  // TODO: Implement method callback into Embedded Permission Prompt.
}

std::vector<
    EmbeddedPermissionPromptSystemSettingsView::RequestLineConfiguration>
EmbeddedPermissionPromptSystemSettingsView::GetRequestLinesConfiguration()
    const {
  return {{/*icon=*/nullptr, GetMessageText()}};
}

std::vector<EmbeddedPermissionPromptSystemSettingsView::ButtonConfiguration>
EmbeddedPermissionPromptSystemSettingsView::GetButtonsConfiguration() const {
  return {{l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_OPEN_SYSTEM_SETTINGS),
           ButtonType::kSystemSettings, ui::ButtonStyle::kTonal}};
}

std::u16string EmbeddedPermissionPromptSystemSettingsView::GetMessageText()
    const {
  const auto& requests = delegate()->Requests();
  CHECK_GT(requests.size(), 0U);

  std::u16string permission_name;
  if (requests.size() == 2) {
    permission_name = l10n_util::GetStringUTF16(
        IDS_CAMERA_AND_MICROPHONE_PERMISSION_NAME_FRAGMENT);
  } else {
    permission_name = requests[0]->GetPermissionNameTextFragment();
  }

  return l10n_util::GetStringFUTF16(IDS_PERMISSION_OFF_FOR_CHROME,
                                    permission_name,
                                    GetUrlIdentityObject().name);
}
