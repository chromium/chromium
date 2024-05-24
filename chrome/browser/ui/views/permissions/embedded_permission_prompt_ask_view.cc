// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_ask_view.h"

#include "chrome/browser/ui/url_identity.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptAskView,
                                      kAllowId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EmbeddedPermissionPromptAskView,
                                      kAllowThisTimeId);

EmbeddedPermissionPromptAskView::EmbeddedPermissionPromptAskView(
    Browser* browser,
    base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate)
    : EmbeddedPermissionPromptBaseView(browser, delegate) {}

EmbeddedPermissionPromptAskView::~EmbeddedPermissionPromptAskView() = default;

std::u16string EmbeddedPermissionPromptAskView::GetAccessibleWindowTitle()
    const {
  return GetMessageText();
}

std::u16string EmbeddedPermissionPromptAskView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    GetUrlIdentityObject().name);
}

void EmbeddedPermissionPromptAskView::RunButtonCallback(int button_id) {
  ButtonType button = GetButtonType(button_id);

  if (delegate()) {
    if (button == ButtonType::kAllowThisTime) {
      delegate()->AllowThisTime();
    } else if (button == ButtonType::kAllow) {
      delegate()->Allow();
    }
  }
}

std::vector<EmbeddedPermissionPromptAskView::RequestLineConfiguration>
EmbeddedPermissionPromptAskView::GetRequestLinesConfiguration() const {
  std::vector<RequestLineConfiguration> lines;

  for (permissions::PermissionRequest* request : delegate()->Requests()) {
    lines.emplace_back(&permissions::GetIconId(request->request_type()),
                       request->GetMessageTextFragment());
  }
  return lines;
}

std::vector<EmbeddedPermissionPromptAskView::ButtonConfiguration>
EmbeddedPermissionPromptAskView::GetButtonsConfiguration() const {
  std::vector<ButtonConfiguration> buttons;
  if (base::FeatureList::IsEnabled(permissions::features::kOneTimePermission)) {
    ButtonConfiguration allow_once = {
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME),
        ButtonType::kAllowThisTime, ui::ButtonStyle::kTonal, kAllowThisTimeId};

    ButtonConfiguration allow_always = {
        GetAllowAlwaysText(delegate()->Requests()), ButtonType::kAllow,
        ui::ButtonStyle::kTonal, kAllowId};

    if (permissions::feature_params::kShowAllowAlwaysAsFirstButton.Get()) {
      buttons.push_back(allow_always);
      buttons.push_back(allow_once);
    } else {
      buttons.push_back(allow_once);
      buttons.push_back(allow_always);
    }
  } else {
    buttons.emplace_back(l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW),
                         ButtonType::kAllow, ui::ButtonStyle::kTonal, kAllowId);
  }
  return buttons;
}

std::u16string EmbeddedPermissionPromptAskView::GetMessageText() const {
  auto& requests = delegate()->Requests();
  std::u16string display_name = GetUrlIdentityObject().name;

  if (requests.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM, display_name,
        requests[0]->GetMessageTextFragment());
  }

  int template_id =
      requests.size() == 2
          ? IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS
          : IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS_MORE;
  return l10n_util::GetStringFUTF16(template_id, display_name,
                                    requests[0]->GetMessageTextFragment(),
                                    requests[1]->GetMessageTextFragment());
}

BEGIN_METADATA(EmbeddedPermissionPromptAskView)
END_METADATA
