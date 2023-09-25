// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_error_dialog.h"

#include <memory>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::borealis {

namespace {

using ::borealis::mojom::InstallResult;

// Optionally, the dialog has an extra button so the user can act on the error.
enum class CallToAction {
  kNone,
  kRetry,
  kStorageManagement,
};

class ErrorBehaviourProvider {
 public:
  virtual ~ErrorBehaviourProvider() = default;

  virtual CallToAction GetAction() const { return CallToAction::kNone; }

  virtual std::u16string ErrorMessage() const = 0;

  virtual std::vector<std::pair<std::u16string, GURL>> GetLinks() const {
    return {};
  }
};

class Duplicate : public ErrorBehaviourProvider {
 public:
  std::u16string ErrorMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_DUPLICATE);
  }
};

class Update : public ErrorBehaviourProvider {
 public:
  std::u16string ErrorMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_UPDATE);
  }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    return {
        {l10n_util::GetStringUTF16(IDS_LEARN_MORE),
         GURL("https://support.google.com/chromebook?p=Steam_InternalError")}};
  }
};

class Busy : public ErrorBehaviourProvider {
 public:
  std::u16string ErrorMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_BUSY);
  }
  CallToAction GetAction() const override { return CallToAction::kRetry; }
};

class Space : public ErrorBehaviourProvider {
 public:
  std::u16string ErrorMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_SPACE);
  }
  CallToAction GetAction() const override {
    return CallToAction::kStorageManagement;
  }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    return {{l10n_util::GetStringUTF16(IDS_LEARN_MORE),
             GURL("https://support.google.com/"
                  "chromebook?p=Steam_DlcNeedSpaceError")}};
  }
};

class Offline : public ErrorBehaviourProvider {
 public:
  std::u16string ErrorMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_OFFLINE);
  }
  CallToAction GetAction() const override { return CallToAction::kRetry; }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    return {{l10n_util::GetStringUTF16(IDS_LEARN_MORE),
             GURL("https://support.google.com/chromebook?p=Steam_Offline")}};
  }
};

class Startup : public ErrorBehaviourProvider {
 public:
  std::u16string ErrorMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_STARTUP);
  }
  CallToAction GetAction() const override { return CallToAction::kRetry; }
};

std::unique_ptr<ErrorBehaviourProvider> GetBehaviour(InstallResult result) {
  switch (result) {
    case InstallResult::kSuccess:
    case InstallResult::kCancelled:
      DCHECK(false);
      ABSL_FALLTHROUGH_INTENDED;
    case InstallResult::kBorealisNotAllowed:
      // This should not be reachable either but if some kind of dynamic
      // permission change would occur then we can get here, so don't DCHECK.
      return GetBehaviour(InstallResult::kDlcNeedRebootError);
    case InstallResult::kBorealisInstallInProgress:
      return std::make_unique<Duplicate>();
    case InstallResult::kDlcUnsupportedError:
    case InstallResult::kDlcInternalError:
    case InstallResult::kDlcUnknownError:
    case InstallResult::kDlcNeedRebootError:
    case InstallResult::kDlcNeedUpdateError:
      // We capture most dlc-related issues as "need update". This is not
      // strictly true (there may not be an update available, and it may not fix
      // it) but we don't have visibility into their cause enough to make a
      // better recommendation.
      return std::make_unique<Update>();
    case InstallResult::kDlcBusyError:
      return std::make_unique<Busy>();
    case InstallResult::kDlcNeedSpaceError:
      return std::make_unique<Space>();
    case InstallResult::kOffline:
      return std::make_unique<Offline>();
    case InstallResult::kStartupFailed:
    case InstallResult::kMainAppNotPresent:
      return std::make_unique<Startup>();
  }
}

class BorealisInstallerErrorDialog : public views::DialogDelegate {
 public:
  BorealisInstallerErrorDialog(
      std::unique_ptr<ErrorBehaviourProvider> behaviour,
      DialogCallback callback)
      : behaviour_(std::move(behaviour)), callback_(std::move(callback)) {
    SetTitle(IDS_BOREALIS_INSTALLER_APP_NAME);
    set_internal_name("BorealisInstallerErrorDialog");
    InitializeButtons();
    InitializeView(*behaviour_);
    SetModalType(ui::MODAL_TYPE_WINDOW);
    SetOwnedByWidget(true);
    SetShowCloseButton(false);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    set_corner_radius(12);

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

  void SetIconColor(views::View* main_view) {
    const ui::ColorProvider* color_provider = main_view->GetColorProvider();
    CHECK(color_provider);
    alert_icon_->SetEnabledColor(
        color_provider->GetColor(cros_tokens::kIconColorAlert));
  }

  bool ShouldShowWindowTitle() const override { return false; }

 private:
  void InitializeButtons() {
    switch (behaviour_->GetAction()) {
      case CallToAction::kNone:
        SetButtons(ui::DIALOG_BUTTON_OK);
        SetButtonLabel(ui::DIALOG_BUTTON_OK,
                       l10n_util::GetStringUTF16(IDS_APP_CANCEL));
        break;
      case CallToAction::kRetry:
        SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
        SetButtonLabel(ui::DIALOG_BUTTON_OK,
                       l10n_util::GetStringUTF16(
                           IDS_BOREALIS_INSTALLER_ERROR_BUTTON_RETRY));
        SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                       l10n_util::GetStringUTF16(IDS_APP_CANCEL));
        break;
      case CallToAction::kStorageManagement:
        SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
        SetButtonLabel(ui::DIALOG_BUTTON_OK,
                       l10n_util::GetStringUTF16(
                           IDS_BOREALIS_INSTALLER_ERROR_BUTTON_STORAGE));
        SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                       l10n_util::GetStringUTF16(IDS_APP_CANCEL));
        break;
    }
  }

  void InitializeView(const ErrorBehaviourProvider& behaviour) {
    auto view = std::make_unique<views::View>();

    views::LayoutProvider* provider = views::LayoutProvider::Get();
    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    // TODO(b/284389804): Use TypographyToken::kCrosDisplay*
    alert_icon_ = view->AddChildView(std::make_unique<views::Label>(
        u"\x24D8", views::Label::CustomFont{gfx::FontList(
                       {"Google Sans", "Roboto"}, gfx::Font::NORMAL, 20,
                       gfx::Font::Weight::BOLD)}));
    alert_icon_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    views::Label* title_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_TITLE),
        CONTEXT_IPH_BUBBLE_TITLE, views::style::STYLE_EMPHASIZED);
    title_label->SetMultiLine(true);
    title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    view->AddChildView(title_label);

    views::Label* message_label = new views::Label(behaviour_->ErrorMessage());
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    view->AddChildView(message_label);

    for (const std::pair<std::u16string, GURL>& link : behaviour.GetLinks()) {
      LOG(ERROR) << link.first << link.second;
      views::Link* link_label =
          view->AddChildView(std::make_unique<views::Link>(link.first));
      link_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      link_label->SetCallback(base::BindRepeating(
          [](GURL url) {
            ash::NewWindowDelegate::GetPrimary()->OpenUrl(
                url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                ash::NewWindowDelegate::Disposition::kNewForegroundTab);
          },
          link.second));
    }

    SetContentsView(std::move(view));
  }

  void OnDismissed(bool is_accept) {
    DCHECK(!callback_.is_null());
    std::move(callback_).Run(GetChoice(is_accept));
  }

  views::borealis::ErrorDialogChoice GetChoice(bool is_accept) {
    switch (behaviour_->GetAction()) {
      case CallToAction::kNone:
        return views::borealis::ErrorDialogChoice::kExit;
      case CallToAction::kRetry:
        return is_accept ? views::borealis::ErrorDialogChoice::kRetry
                         : views::borealis::ErrorDialogChoice::kExit;
      case CallToAction::kStorageManagement:
        if (is_accept) {
          chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              ProfileManager::GetPrimaryUserProfile(),
              chromeos::settings::mojom::kStorageSubpagePath);
        }
        return views::borealis::ErrorDialogChoice::kExit;
    }
  }

  std::unique_ptr<ErrorBehaviourProvider> behaviour_;
  DialogCallback callback_;
  raw_ptr<views::Label> alert_icon_;

  base::WeakPtrFactory<BorealisInstallerErrorDialog> weak_factory_{this};
};

}  // namespace

void ShowInstallerErrorDialog(gfx::NativeView parent,
                              InstallResult result,
                              DialogCallback callback) {
  DCHECK(result != InstallResult::kSuccess);
  DCHECK(result != InstallResult::kCancelled);
  auto delegate = std::make_unique<BorealisInstallerErrorDialog>(
      GetBehaviour(result), std::move(callback));
  BorealisInstallerErrorDialog* delegate_ptr = delegate.get();
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::move(delegate), nullptr, parent);
  delegate_ptr->SetIconColor(widget->GetRootView());
  widget->Show();
}

}  // namespace views::borealis
