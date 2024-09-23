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
    EmbeddedPermissionPromptSystemSettingsView(
        Browser* browser,
        base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate)
    : EmbeddedPermissionPromptBaseView(browser, delegate) {}

EmbeddedPermissionPromptSystemSettingsView::
    ~EmbeddedPermissionPromptSystemSettingsView() = default;

std::u16string
EmbeddedPermissionPromptSystemSettingsView::GetAccessibleWindowTitle() const {
  return GetWindowTitle();
}

std::u16string EmbeddedPermissionPromptSystemSettingsView::GetWindowTitle()
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
                                    permission_name);
}

void EmbeddedPermissionPromptSystemSettingsView::RunButtonCallback(
    int button_id) {
  if (!delegate()) {
    return;
  }

  ButtonType button = GetButtonType(button_id);
  DCHECK_EQ(button, ButtonType::kSystemSettings);

  delegate()->ShowSystemSettings();
}

std::vector<
    EmbeddedPermissionPromptSystemSettingsView::RequestLineConfiguration>
EmbeddedPermissionPromptSystemSettingsView::GetRequestLinesConfiguration()
    const {
  return {};
}

std::vector<EmbeddedPermissionPromptSystemSettingsView::ButtonConfiguration>
EmbeddedPermissionPromptSystemSettingsView::GetButtonsConfiguration() const {
  std::u16string operating_system_name;

#if BUILDFLAG(IS_MAC)
  operating_system_name = l10n_util::GetStringUTF16(IDS_MACOS_NAME_FRAGMENT);
#elif BUILDFLAG(IS_WIN)
  operating_system_name = l10n_util::GetStringUTF16(IDS_WINDOWS_NAME_FRAGMENT);
#endif

  // Do not show buttons if the OS is not supported.
  if (operating_system_name.empty()) {
    return std::vector<ButtonConfiguration>();
  }

  return {{l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_OPEN_SYSTEM_SETTINGS,
                                      operating_system_name),
           ButtonType::kSystemSettings, ui::ButtonStyle::kTonal}};
}
