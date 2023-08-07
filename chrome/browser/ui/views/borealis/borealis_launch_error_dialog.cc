// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_launch_error_dialog.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ui/views/borealis/borealis_disallowed_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::borealis {

using ::borealis::BorealisStartupResult;

namespace {

// Views uses tricks like this to ensure singleton-ness of dialogs.
static Widget* g_instance_ = nullptr;

class BorealisLaunchErrorDialog : public DialogDelegate {
 public:
  explicit BorealisLaunchErrorDialog(Profile* profile) {
    DCHECK(!g_instance_);

    SetTitle(IDS_BOREALIS_APP_NAME);
    set_internal_name("BorealisLaunchErrorDialog");

    SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
    SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_BUTTON_RETRY));
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_APP_CANCEL));

    SetAcceptCallback(base::BindOnce(
        [](Profile* profile) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](Profile* profile) {
                    // Technically "retry" should re-do whatever the user
                    // originally tried. For simplicity we just retry the
                    // client app.
                    ::borealis::BorealisService::GetForProfile(profile)
                        ->AppLauncher()
                        .Launch(::borealis::kClientAppId, base::DoNothing());
                  },
                  profile));
        },
        profile));

    InitializeView();
    SetModalType(ui::MODAL_TYPE_NONE);
    SetOwnedByWidget(true);
    SetShowCloseButton(false);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  }

  ~BorealisLaunchErrorDialog() override {
    DCHECK(g_instance_);
    g_instance_ = nullptr;
  }

  bool ShouldShowWindowTitle() const override { return false; }

 private:
  void InitializeView() {
    auto view = std::make_unique<views::View>();

    views::LayoutProvider* provider = views::LayoutProvider::Get();
    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    views::Label* title_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_BOREALIS_LAUNCH_ERROR_TITLE),
        CONTEXT_IPH_BUBBLE_TITLE, views::style::STYLE_EMPHASIZED);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetMultiLine(true);
    view->AddChildView(title_label);

    views::Label* message_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_BOREALIS_LAUNCH_ERROR_BODY));
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(message_label);

    SetContentsView(std::move(view));
  }
};
}  // namespace

void ShowBorealisLaunchErrorView(Profile* profile,
                                 BorealisStartupResult error) {
  if (error == BorealisStartupResult::kDisallowed) {
    ::borealis::BorealisService::GetForProfile(profile)->Features().IsAllowed(
        base::BindOnce(&ShowLauncherDisallowedDialog));
    return;
  }

  // TODO(b/248938308): Closing and reopening the dialog this way is not
  // desirable. When we move to webui we should just re-show the current dialog.
  if (g_instance_) {
    g_instance_->CloseNow();
  }

  auto delegate = std::make_unique<BorealisLaunchErrorDialog>(profile);
  g_instance_ = views::DialogDelegate::CreateDialogWidget(std::move(delegate),
                                                          nullptr, nullptr);
  g_instance_->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, ash::ShelfID(::borealis::kClientAppId).Serialize());
  g_instance_->Show();
}

}  // namespace views::borealis
