// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_package_install_failure_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

namespace crostini {

// Implementation from crostini_util.h, necessary due to chrome's inclusion
// rules.
void ShowCrostiniPackageInstallFailureView(const std::string& error_message) {
  CrostiniPackageInstallFailureView::Show(error_message);
}

}  // namespace crostini

void CrostiniPackageInstallFailureView::Show(const std::string& error_message) {
  views::DialogDelegate::CreateDialogWidget(
      new CrostiniPackageInstallFailureView(error_message), nullptr, nullptr)
      ->Show();
}

CrostiniPackageInstallFailureView::CrostiniPackageInstallFailureView(
    const std::string& error_message) {
  SetShowCloseButton(false);
  SetTitle(IDS_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_TITLE);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  views::StyledLabel* message_label =
      AddChildView(std::make_unique<views::StyledLabel>());
  message_label->SetText(l10n_util::GetStringUTF16(
      IDS_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_MESSAGE));

  views::MessageBoxView* error_box =
      new views::MessageBoxView(base::UTF8ToUTF16(error_message));
  AddChildView(error_box);

  set_close_on_deactivate(true);
}

CrostiniPackageInstallFailureView::~CrostiniPackageInstallFailureView() =
    default;

BEGIN_METADATA(CrostiniPackageInstallFailureView)
END_METADATA
