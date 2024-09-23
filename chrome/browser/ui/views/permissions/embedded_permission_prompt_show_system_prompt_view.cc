// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_show_system_prompt_view.h"

#include "base/memory/weak_ptr.h"

#include "chrome/browser/ui/url_identity.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

EmbeddedPermissionPromptShowSystemPromptView::
    EmbeddedPermissionPromptShowSystemPromptView(
        Browser* browser,
        base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate)
    : EmbeddedPermissionPromptBaseView(browser, delegate) {}

EmbeddedPermissionPromptShowSystemPromptView::
    ~EmbeddedPermissionPromptShowSystemPromptView() = default;

std::u16string
EmbeddedPermissionPromptShowSystemPromptView::GetAccessibleWindowTitle() const {
  return GetWindowTitle();
}

std::u16string EmbeddedPermissionPromptShowSystemPromptView::GetWindowTitle()
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

  return l10n_util::GetStringFUTF16(IDS_PERMISSION_CHROME_NEEDS_PERMISSION,
                                    permission_name);
}

void EmbeddedPermissionPromptShowSystemPromptView::RunButtonCallback(
    int button_id) {
  // This view has no buttons.
  NOTREACHED_IN_MIGRATION();
  return;
}

std::vector<
    EmbeddedPermissionPromptShowSystemPromptView::RequestLineConfiguration>
EmbeddedPermissionPromptShowSystemPromptView::GetRequestLinesConfiguration()
    const {
  return {};
}

std::vector<EmbeddedPermissionPromptShowSystemPromptView::ButtonConfiguration>
EmbeddedPermissionPromptShowSystemPromptView::GetButtonsConfiguration() const {
  // This view has no buttons.
  return std::vector<ButtonConfiguration>();
}

bool EmbeddedPermissionPromptShowSystemPromptView::ShowLoadingIcon() const {
  return true;
}
