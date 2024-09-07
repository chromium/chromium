// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_expired_container_warning_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

CrostiniExpiredContainerWarningView* g_crostini_expired_container_warning_view =
    nullptr;

}  // namespace

void CrostiniExpiredContainerWarningView::Show(Profile* profile,
                                               base::OnceClosure callback) {
  if (g_crostini_expired_container_warning_view) {
    g_crostini_expired_container_warning_view->callbacks_.push_back(
        std::move(callback));
  } else {
    g_crostini_expired_container_warning_view =
        new CrostiniExpiredContainerWarningView(profile, std::move(callback));
    CreateDialogWidget(g_crostini_expired_container_warning_view, nullptr,
                       nullptr);
  }

  // Always call Show to bring the dialog to the front of the screen.
  g_crostini_expired_container_warning_view->GetWidget()->Show();

  VLOG(1) << "Showed CrostiniExpiredContainerWarningView";
}

CrostiniExpiredContainerWarningView::CrostiniExpiredContainerWarningView(
    Profile* profile,
    base::OnceClosure callback)
    : profile_(profile), weak_ptr_factory_(this) {
  callbacks_.push_back(std::move(callback));

  // Make the dialog modal to force the user to make a decision.
  SetModalType(ui::mojom::ModalType::kSystem);

  SetTitle(IDS_CROSTINI_EXPIRED_CONTAINER_WARNING_TITLE);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_CROSTINI_EXPIRED_CONTAINER_WARNING_CONTINUE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_CROSTINI_EXPIRED_CONTAINER_WARNING_UPGRADE_BUTTON));
  SetShowCloseButton(false);

  // Show upgrade dialog on accept, and pass in the callback.
  SetAcceptCallback(base::BindOnce(
      [](base::WeakPtr<CrostiniExpiredContainerWarningView> weak_this) {
        ash::CrostiniUpgraderDialog::Show(
            weak_this->profile_,
            base::BindOnce(
                [](std::vector<base::OnceClosure> callbacks) {
                  for (auto&& callback : callbacks) {
                    std::move(callback).Run();
                  }
                },
                std::move(weak_this->callbacks_)));
      },
      weak_ptr_factory_.GetWeakPtr()));

  // On cancel, call the callback directly.
  SetCancelCallback(base::BindOnce(
      [](base::WeakPtr<CrostiniExpiredContainerWarningView> weak_this) {
        for (auto&& callback : weak_this->callbacks_) {
          std::move(callback).Run();
        }
      },
      weak_ptr_factory_.GetWeakPtr()));

  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_CROSTINI_EXPIRED_CONTAINER_WARNING_BODY);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label);
}

CrostiniExpiredContainerWarningView::~CrostiniExpiredContainerWarningView() {
  g_crostini_expired_container_warning_view = nullptr;
}

BEGIN_METADATA(CrostiniExpiredContainerWarningView)
END_METADATA
