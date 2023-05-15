// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_error_dialog.h"
#include <memory>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::borealis {

namespace {

using InstallResult = ::borealis::BorealisInstallResult;

bool ShouldAllowRetry(InstallResult result) {
  switch (result) {
    case InstallResult::kBorealisInstallInProgress:
    case InstallResult::kDlcBusyError:
    case InstallResult::kDlcNeedSpaceError:
    case InstallResult::kOffline:
      return true;
    case InstallResult::kDlcInternalError:
    case InstallResult::kDlcUnknownError:
    case InstallResult::kDlcNeedRebootError:
    case InstallResult::kBorealisNotAllowed:
    case InstallResult::kDlcUnsupportedError:
    case InstallResult::kDlcNeedUpdateError:
    case InstallResult::kStartupFailed:
    case InstallResult::kMainAppNotPresent:
      return false;
    case InstallResult::kSuccess:
    case InstallResult::kCancelled:
      NOTREACHED_NORETURN();
  }
}

std::pair<std::u16string, absl::optional<GURL>> GetErrorMessageAndHelpLink(
    InstallResult result) {
  // TODO(b/256699588): Replace the below direct URLs with p-links.
  switch (result) {
    case InstallResult::kBorealisNotAllowed:
    case InstallResult::kDlcUnsupportedError:
      return {
          l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_DISALLOWED),
          absl::nullopt};
    case InstallResult::kBorealisInstallInProgress:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_PROGRESS),
              absl::nullopt};
    case InstallResult::kDlcInternalError:
    case InstallResult::kDlcNeedUpdateError:
      return {
          l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_DLC_INTERNAL),
          GURL("https://support.google.com/chromebook/answer/177889")};
    case InstallResult::kDlcBusyError:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_DLC_BUSY),
              absl::nullopt};
    case InstallResult::kDlcNeedRebootError:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_REBOOT),
              absl::nullopt};
    case InstallResult::kDlcNeedSpaceError:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_SPACE),
              GURL("https://support.google.com/chromebook/answer/1061547")};
    case InstallResult::kDlcUnknownError:
      return {
          l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_DLC_UNKNOWN),
          absl::nullopt};
    case InstallResult::kOffline:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_OFFLINE),
              GURL("https://support.google.com/chromebook/answer/6318213")};
    case InstallResult::kStartupFailed:
    case InstallResult::kMainAppNotPresent:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_STARTUP),
              absl::nullopt};
    case InstallResult::kSuccess:
    case InstallResult::kCancelled:
      NOTREACHED_NORETURN();
  }
}

class BorealisInstallerErrorDialog : public views::DialogDelegate {
 public:
  BorealisInstallerErrorDialog(InstallResult result, DialogCallback callback)
      : result_(result), callback_(std::move(callback)) {
    set_internal_name("BorealisInstallerErrorDialog");
    if (ShouldAllowRetry(result_)) {
      SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
      SetButtonLabel(
          ui::DIALOG_BUTTON_OK,
          l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_RETRY));
      SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                     l10n_util::GetStringUTF16(IDS_APP_CANCEL));
    } else {
      SetButtons(ui::DIALOG_BUTTON_OK);
      SetButtonLabel(ui::DIALOG_BUTTON_OK,
                     l10n_util::GetStringUTF16(IDS_APP_CANCEL));
    }
    InitializeView();
    SetModalType(ui::MODAL_TYPE_WINDOW);
    SetOwnedByWidget(true);
    SetShowCloseButton(false);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

    // Bind the callbacks to our internal dispatcher.
    SetAcceptCallback(base::BindOnce(&BorealisInstallerErrorDialog::OnDismissed,
                                     weak_factory_.GetWeakPtr(),
                                     /*is_accept=*/true));
    SetCancelCallback(base::BindOnce(&BorealisInstallerErrorDialog::OnDismissed,
                                     weak_factory_.GetWeakPtr(),
                                     /*is_accept=*/false));
    SetCloseCallback(base::BindOnce(&BorealisInstallerErrorDialog::OnDismissed,
                                    weak_factory_.GetWeakPtr(),
                                    /*is_accept:=*/false));
  }

 private:
  void InitializeView() {
    auto view = std::make_unique<views::View>();

    views::LayoutProvider* provider = views::LayoutProvider::Get();
    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    views::ImageView* error_icon = new views::ImageView();
    error_icon->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_PLUGIN_VM_INSTALLER_ERROR));
    error_icon->SetImageSize({64, 64});
    view->AddChildView(error_icon);

    views::Label* title_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_TITLE),
        CONTEXT_IPH_BUBBLE_TITLE, views::style::STYLE_EMPHASIZED);
    title_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    title_label->SetMultiLine(true);
    view->AddChildView(title_label);

    std::pair<std::u16string, absl::optional<GURL>> message_link =
        GetErrorMessageAndHelpLink(result_);
    views::Label* message_label = new views::Label(message_link.first);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(message_label);

    if (message_link.second.has_value()) {
      views::Link* learn_more_link =
          new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
      learn_more_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      learn_more_link->SetCallback(base::BindRepeating(
          [](GURL url) {
            ash::NewWindowDelegate::GetPrimary()->OpenUrl(
                url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                ash::NewWindowDelegate::Disposition::kNewForegroundTab);
          },
          message_link.second.value()));
      view->AddChildView(learn_more_link);
    }

    SetContentsView(std::move(view));
  }

  void OnDismissed(bool is_accept) {
    DCHECK(!callback_.is_null());
    std::move(callback_).Run(is_accept && ShouldAllowRetry(result_)
                                 ? views::borealis::ErrorDialogChoice::kRetry
                                 : views::borealis::ErrorDialogChoice::kExit);
  }

  InstallResult result_;
  DialogCallback callback_;

  base::WeakPtrFactory<BorealisInstallerErrorDialog> weak_factory_{this};
};

}  // namespace

void ShowInstallerErrorDialog(gfx::NativeView parent,
                              InstallResult result,
                              DialogCallback callback) {
  DCHECK(result != InstallResult::kSuccess);
  DCHECK(result != InstallResult::kCancelled);
  views::DialogDelegate::CreateDialogWidget(
      std::make_unique<BorealisInstallerErrorDialog>(result,
                                                     std::move(callback)),
      nullptr, parent)
      ->Show();
}

}  // namespace views::borealis
