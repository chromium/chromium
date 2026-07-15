// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_system_settings_view.h"

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/grit/branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    EmbeddedPermissionPromptSystemSettingsView,
    kOpenSettingsId);

EmbeddedPermissionPromptSystemSettingsView::
    EmbeddedPermissionPromptSystemSettingsView(
        content::WebContents* web_contents,
        base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate)
    : EmbeddedPermissionPromptBaseView(web_contents, delegate) {
  if (auto* widget =
          views::Widget::GetWidgetForNativeWindow(GetNativeWindow())) {
    host_widget_observation_.Observe(widget);
  }
}

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

  return l10n_util::GetStringFUTF16(
      IDS_PERMISSION_OFF_FOR_CHROME, permission_name,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
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

void EmbeddedPermissionPromptSystemSettingsView::OnWidgetTreeActivated(
    views::Widget* root_widget,
    views::Widget* active_widget) {
  if (!host_widget_observation_.IsObserving() ||
      !host_widget_observation_.IsObservingSource(root_widget)) {
    return;
  }

  // Ignore host widget activation changes that occur after the permission
  // prompt has been closed.
  if (GetWidget() && GetWidget()->IsClosed()) {
    return;
  }

  const auto& requests = delegate()->Requests();
  auto barrier = base::BarrierCallback<bool>(
      requests.size(),
      base::BindOnce(
          &EmbeddedPermissionPromptSystemSettingsView::OnPermissionChecksDone,
          weak_factory_.GetWeakPtr()));

  for (const auto& request : requests) {
    system_permission_settings::IsDeniedFresh(request->GetContentSettingsType(),
                                              barrier);
  }
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
#elif BUILDFLAG(IS_CHROMEOS)
  operating_system_name = l10n_util::GetStringUTF16(IDS_CHROMEOS_NAME_FRAGMENT);
#endif

  // Do not show buttons if the OS is not supported.
  if (operating_system_name.empty()) {
    return std::vector<ButtonConfiguration>();
  }

  return {{l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_OPEN_SYSTEM_SETTINGS,
                                      operating_system_name),
           ButtonType::kSystemSettings, ui::ButtonStyle::kTonal,
           kOpenSettingsId}};
}

void EmbeddedPermissionPromptSystemSettingsView::OnPermissionChecksDone(
    const std::vector<bool>& results) {
  if (!delegate() || prompt_resolved_) {
    return;
  }

  for (bool is_denied : results) {
    if (is_denied) {
      return;
    }
  }

  prompt_resolved_ = true;

  // Asynchronously notify the delegate that the current prompt can be resolved.
  // This is done asyncronouly to avoid checks in the focus logic which prevent
  // a new widget from activating the current window again at this exact moment
  // in time.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedPermissionPromptSystemSettingsView::
                                    NotifyDelegatePermissionNoLongerDenied,
                                weak_factory_.GetWeakPtr()));
}

void EmbeddedPermissionPromptSystemSettingsView::
    NotifyDelegatePermissionNoLongerDenied() {
  if (delegate()) {
    delegate()->SystemPermissionsNoLongerDenied();
  }
}
