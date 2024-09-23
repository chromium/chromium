// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_disallowed_dialog.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::borealis {

namespace {

using AllowStatus = ::borealis::BorealisFeatures::AllowStatus;

using MaybeAction = std::optional<std::pair<std::u16string, base::OnceClosure>>;

// Views uses tricks like this to ensure singleton-ness of dialogs.
static Widget* g_instance_ = nullptr;

class BehaviourProvider {
 public:
  virtual ~BehaviourProvider() = default;

  virtual std::u16string GetMessage() const = 0;

  // Get a label and callback for the "call to action" button, if present.
  virtual MaybeAction GetAction() const { return std::nullopt; }

  virtual std::vector<std::pair<std::u16string, GURL>> GetLinks() const {
    return {};
  }
};

class DisallowedHardware : public BehaviourProvider {
 public:
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_HARDWARE);
  }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    return {{l10n_util::GetStringUTF16(IDS_LEARN_MORE),
             GURL("https://support.google.com/"
                  "chromebook?p=steam_on_chromebook")}};
  }
};

class DisallowedFailure : public BehaviourProvider {
 public:
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_FAILED);
  }
};

class DisallowedIrregular : public BehaviourProvider {
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_IRREGULAR);
  }
};

class DisallowedPrimary : public BehaviourProvider {
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_PRIMARY);
  }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    // TODO(b/256699588): Replace this with a p-link.
    return {{l10n_util::GetStringUTF16(IDS_LEARN_MORE),
             GURL("https://support.google.com/chromebook/answer/6088201")}};
  }
};

class DisallowedChild : public BehaviourProvider {
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_CHILD);
  }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    return {{l10n_util::GetStringUTF16(IDS_LEARN_MORE),
             GURL("https://support.google.com/"
                  "chromebook?p=steam_on_chromebook")}};
  }
};

class DisallowedPolicy : public BehaviourProvider {
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_ADMIN);
  }
  std::vector<std::pair<std::u16string, GURL>> GetLinks() const override {
    // As of 2023q2 directly-linking to chromeenterprise is common, so we shall
    // do it too.
    return {
        {base::UTF8ToUTF16(policy::key::kUserBorealisAllowed),
         GURL("https://chromeenterprise.google/policies/#UserBorealisAllowed")},
        {base::UTF8ToUTF16(policy::key::kVirtualMachinesAllowed),
         GURL("https://chromeenterprise.google/policies/"
              "#VirtualMachinesAllowed")},
    };
  }
};

class DisallowedFlag : public BehaviourProvider {
  std::u16string GetMessage() const override {
    return l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_FLAG);
  }
  MaybeAction GetAction() const override {
    return {{l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_FLAG_BUTTON),
             base::BindOnce([]() {
               ash::SystemAppLaunchParams params;
               params.url = GURL{std::string(chrome::kChromeUIOsFlagsAppURL) +
                                 "#borealis-enabled"};
               ash::LaunchSystemWebAppAsync(
                   ProfileManager::GetPrimaryUserProfile(),
                   ash::SystemWebAppType::OS_FLAGS, params);
             })}};
  }
};

class BorealisDisallowedDialog : public DialogDelegate {
 public:
  BorealisDisallowedDialog(std::unique_ptr<BehaviourProvider> behaviour,
                           int title_id) {
    DCHECK(!g_instance_);

    SetTitle(IDS_BOREALIS_INSTALLER_APP_NAME);
    set_internal_name("BorealisDisallowedDialog");
    MaybeAction second_action = behaviour->GetAction();
    if (second_action.has_value()) {
      SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
                 static_cast<int>(ui::mojom::DialogButton::kCancel));
      SetButtonLabel(ui::mojom::DialogButton::kCancel,
                     l10n_util::GetStringUTF16(IDS_CLOSE));
      SetButtonLabel(ui::mojom::DialogButton::kOk,
                     std::move(second_action.value().first));
      SetAcceptCallback(std::move(second_action.value().second));
    } else {
      SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
      SetButtonLabel(ui::mojom::DialogButton::kOk,
                     l10n_util::GetStringUTF16(IDS_CLOSE));
    }
    InitializeView(*behaviour, title_id);
    SetModalType(ui::mojom::ModalType::kSystem);
    SetOwnedByWidget(true);
    SetShowCloseButton(false);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  }

  ~BorealisDisallowedDialog() override {
    DCHECK(g_instance_);
    g_instance_ = nullptr;
  }

  bool ShouldShowWindowTitle() const override { return false; }

 private:
  void InitializeView(const BehaviourProvider& behaviour, int title_id) {
    auto view = std::make_unique<views::View>();

    views::LayoutProvider* provider = views::LayoutProvider::Get();
    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    views::Label* title_label = new views::Label(
        l10n_util::GetStringUTF16(title_id), CONTEXT_IPH_BUBBLE_TITLE,
        views::style::STYLE_EMPHASIZED);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetMultiLine(true);
    view->AddChildView(title_label);

    views::Label* message_label = new views::Label(behaviour.GetMessage());
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(message_label);

    for (const std::pair<std::u16string, GURL>& link : behaviour.GetLinks()) {
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
};

std::unique_ptr<BehaviourProvider> StatusBehaviour(AllowStatus status) {
  switch (status) {
    case AllowStatus::kAllowed:
      DCHECK(false);
      // Unreachable in practice. Show "failed" message just in case.
      return std::make_unique<DisallowedFailure>();
    case AllowStatus::kFeatureDisabled:
    case AllowStatus::kInsufficientHardware:
      return std::make_unique<DisallowedHardware>();
    case AllowStatus::kFailedToDetermine:
      return std::make_unique<DisallowedFailure>();
    case AllowStatus::kBlockedOnIrregularProfile:
      return std::make_unique<DisallowedIrregular>();
    case AllowStatus::kBlockedOnNonPrimaryProfile:
      return std::make_unique<DisallowedPrimary>();
    case AllowStatus::kBlockedOnChildAccount:
      return std::make_unique<DisallowedChild>();
    case AllowStatus::kVmPolicyBlocked:
    case AllowStatus::kUserPrefBlocked:
      return std::make_unique<DisallowedPolicy>();
    case AllowStatus::kBlockedByFlag:
      return std::make_unique<DisallowedFlag>();
  }
}

void ShowDisallowedDialog(AllowStatus status, int title_id) {
  DCHECK(status != AllowStatus::kAllowed);

  // TODO(b/248938308): Closing and reopening the dialog this way is not
  // desirable. When we move to webui we should just re-show the current dialog.
  if (g_instance_) {
    g_instance_->CloseNow();
  }

  auto delegate = std::make_unique<BorealisDisallowedDialog>(
      StatusBehaviour(status), title_id);
  g_instance_ = views::DialogDelegate::CreateDialogWidget(std::move(delegate),
                                                          nullptr, nullptr);
  g_instance_->Show();
}

}  // namespace

void ShowInstallerDisallowedDialog(AllowStatus status) {
  ShowDisallowedDialog(status, IDS_BOREALIS_DISALLOWED_TITLE);
}

void ShowLauncherDisallowedDialog(AllowStatus status) {
  ShowDisallowedDialog(status, IDS_BOREALIS_LAUNCH_ERROR_TITLE);
}

}  // namespace views::borealis
