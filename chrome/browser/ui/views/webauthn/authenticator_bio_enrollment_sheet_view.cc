// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_bio_enrollment_sheet_view.h"

#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/webauthn/ring_progress_bar.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace {
static constexpr int kFingerprintSize = 120;
static constexpr int kRingSize = 228;

double CalculateProgressFor(double samples_remaining, double max_samples) {
  return 1 - samples_remaining / (max_samples <= 0 ? 1 : max_samples);
}
}  // namespace

AuthenticatorBioEnrollmentSheetView::AuthenticatorBioEnrollmentSheetView(
    std::unique_ptr<AuthenticatorBioEnrollmentSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {
  // Override the DialogClientView (i.e. this view's parent) handling of the
  // escape key to avoid closing the dialog when we want to cancel instead,
  // since cancelling might do something different.
  // This is a workaround to fix crbug.com/1145724.
  // TODO(nsatragno): remove this workaround once crbug.com/1147927 is fixed.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

AuthenticatorBioEnrollmentSheetView::~AuthenticatorBioEnrollmentSheetView() =
    default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorBioEnrollmentSheetView::BuildStepSpecificContent() {
  auto* bio_model = static_cast<AuthenticatorBioEnrollmentSheetModel*>(model());
  double target = CalculateProgressFor(bio_model->bio_samples_remaining(),
                                       bio_model->max_bio_samples());
  double initial;
  if (target <= 0) {
    initial = target;
  } else {
    initial = CalculateProgressFor(bio_model->bio_samples_remaining() + 1,
                                   bio_model->max_bio_samples());
  }

  auto animation_container = std::make_unique<views::View>();
  animation_container->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view->SetImage(ui::ImageModel::FromVectorIcon(
      target >= 1 ? views::kMenuCheckIcon : kFingerprintIcon, ui::kColorAccent,
      kFingerprintSize));
  animation_container->AddChildView(std::move(image_view));

  auto ring_progress_bar = std::make_unique<RingProgressBar>();
  ring_progress_bar->SetPreferredSize(gfx::Size(kRingSize, kRingSize));
  ring_progress_bar->SetValue(initial, target);
  animation_container->AddChildView(std::move(ring_progress_bar));

  return std::make_pair(std::move(animation_container), AutoFocus::kNo);
}

bool AuthenticatorBioEnrollmentSheetView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  model()->OnCancel();
  return true;
}

BEGIN_METADATA(AuthenticatorBioEnrollmentSheetView)
END_METADATA
