// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_infobar.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/signin/signin_qrcode_infobar_delegate.h"
#include "chrome/browser/ui/signin/signin_qrcode_model.h"
#include "chrome/browser/ui/signin/signin_qrcode_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace {

// Visual constants.
constexpr int kHorizontalMargin = 24;
constexpr int kVerticalMargin = 8;
constexpr int kTargetHeight = 136;

}  // namespace

SigninQRCodeInfoBar::SigninQRCodeInfoBar(
    std::unique_ptr<SigninQRCodeInfoBarDelegate> delegate,
    SigninQRCodeModel* model)
    : InfoBarView(std::move(delegate)) {
  CHECK(model);
  // Override InfoBarView default layout manager.
  auto* layout =
      content_container()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(kVerticalMargin, kHorizontalMargin, kVerticalMargin,
                            kHorizontalMargin)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetTargetHeight(kTargetHeight);

  qr_code_view_ =
      content_container()->AddChildView(std::make_unique<SigninQRCodeView>());

  model_observation_.Observe(model);
  if (model->qr_code_string().has_value()) {
    OnQrCodeReady(*model->qr_code_string());
  }
}

SigninQRCodeInfoBar::~SigninQRCodeInfoBar() = default;

bool SigninQRCodeInfoBar::IsShowingQrCodeForTesting() const {
  return qr_code_view_ &&
         qr_code_view_->IsShowingQrCodeForTesting();  // IN-TEST
}

void SigninQRCodeInfoBar::PlatformSpecificShow(bool animate) {
  InfoBarView::PlatformSpecificShow(animate);
  if (parent()) {
    views::View* shadow = parent()->children().back().get();
    if (shadow) {
      shadow->SetVisible(false);
    }
  }
}

void SigninQRCodeInfoBar::OnQrCodeReady(std::string_view qr_string) {
  qr_code_view_->UpdateQrCode(qr_string);
}

void SigninQRCodeInfoBar::OnQrCodeChanged(std::string_view qr_code_string) {
  OnQrCodeReady(qr_code_string);
}

void SigninQRCodeInfoBar::OnQrCodeReset() {
  RemoveSelf();
}

void SigninQRCodeInfoBar::OnModelDestroyed(SigninQRCodeModel* model) {
  model_observation_.Reset();
}

BEGIN_METADATA(SigninQRCodeInfoBar)
END_METADATA
