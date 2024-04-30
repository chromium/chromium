// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_onboarding_sheet_view.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

constexpr int kUsernameMarginTop = 10;

AuthenticatorGpmOnboardingSheetView::AuthenticatorGpmOnboardingSheetView(
    std::unique_ptr<AuthenticatorGpmOnboardingSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorGpmOnboardingSheetView::~AuthenticatorGpmOnboardingSheetView() =
    default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorGpmOnboardingSheetView::AutoFocus>
AuthenticatorGpmOnboardingSheetView::BuildStepSpecificContent() {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));

  container->AddChildView(CreatePasskeyIconWithLabelRow(
      kDevicesIcon,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_AVAILABLE_ACROSS_DEVICES)));
  container->AddChildView(CreatePasskeyIconWithLabelRow(
      kFingerprintIcon,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_FASTER_SIGN_IN)));
  container->AddChildView(CreatePasskeyIconWithLabelRow(
      // TODO(rgod): Add correct icon.
      kKeyIcon, l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_SAFER)));

  auto* sheet_model = static_cast<AuthenticatorSheetModelBase*>(model());
  std::u16string username = base::UTF8ToUTF16(
      sheet_model->dialog_model()->user_entity.name.value_or(""));
  std::unique_ptr<views::View> username_view =
      CreatePasskeyWithUsernameLabel(username);
  username_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(kUsernameMarginTop, 0, 0, 0)));
  container->AddChildView(std::move(username_view));

  return std::make_pair(std::move(container), AutoFocus::kNo);
}
