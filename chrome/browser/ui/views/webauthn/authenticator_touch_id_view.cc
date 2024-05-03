// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_touch_id_view.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/mac_authentication_view.h"
#include "chrome/browser/ui/views/webauthn/passkey_detail_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "crypto/scoped_lacontext.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace {
constexpr int kVerticalPadding = 20;

// The size if the lock icon that replaces the Touch ID prompt when locked
// (square).
constexpr int kLockIconSize = 64;
}  // namespace

AuthenticatorTouchIdView::AuthenticatorTouchIdView(
    std::unique_ptr<AuthenticatorTouchIdSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorTouchIdView::~AuthenticatorTouchIdView() = default;

std::pair<std::unique_ptr<views::View>, AuthenticatorTouchIdView::AutoFocus>
AuthenticatorTouchIdView::BuildStepSpecificContent() {
  auto container = std::make_unique<views::BoxLayoutView>();
  auto* dialog_model =
      static_cast<AuthenticatorTouchIdSheetModel*>(model())->dialog_model();
  bool is_get_assertion =
      dialog_model->request_type == device::FidoRequestType::kGetAssertion;
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(kVerticalPadding);
  container->AddChildView(CreatePasskeyWithUsernameLabel(base::UTF8ToUTF16(
      (is_get_assertion ? dialog_model->preselected_cred->user.name
                        : dialog_model->user_entity.name)
          .value_or(""))));
  if (device::fido::mac::DeviceHasBiometricsAvailable()) {
    container->AddChildView(std::make_unique<MacAuthenticationView>(
        base::BindOnce(&AuthenticatorTouchIdView::OnTouchIDComplete,
                       base::Unretained(this))));
    container->AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_TOUCH_ID_CONTINUE)));
  } else {
    container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            vector_icons::kLockIcon, ui::kColorMenuIcon, kLockIconSize)));
    container->AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_TOUCH_ID_LOCKED)));
  }
  return {std::move(container), AutoFocus::kNo};
}

void AuthenticatorTouchIdView::OnTouchIDComplete(
    std::optional<crypto::ScopedLAContext> lacontext) {
  static_cast<AuthenticatorTouchIdSheetModel*>(model())->OnTouchIDSensorTapped(
      std::move(lacontext));
}
