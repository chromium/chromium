// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_disallowed_dialog.h"
#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
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

// Views uses tricks like this to ensure singleton-ness of dialogs.
static Widget* g_instance_ = nullptr;

static std::pair<std::u16string, absl::optional<GURL>> GetMessageForStatus(
    AllowStatus status) {
  // TODO(b/256699588): Replace the below direct URLs with p-links.
  switch (status) {
    case AllowStatus::kAllowed:
      DCHECK(false);
      // Unreachable in practice. Show "failed" message just in case.
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_FAILED),
              absl::nullopt};
    case AllowStatus::kFeatureDisabled:
    case AllowStatus::kUnsupportedModel:
    case AllowStatus::kHardwareChecksFailed:
    case AllowStatus::kIncorrectToken:
      // TODO(b/256699588): Replace this with an actual help-center link when
      // one is created.
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_DISABLED),
              GURL("https://www.chromium.org/chromium-os/steam-on-chromeos/"
                   "#supported-devices")};
    case AllowStatus::kFailedToDetermine:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_FAILED),
              absl::nullopt};
    case AllowStatus::kBlockedOnIrregularProfile:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_IRREGULAR),
              absl::nullopt};
    case AllowStatus::kBlockedOnNonPrimaryProfile:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_PRIMARY),
              absl::nullopt};
    case AllowStatus::kBlockedOnChildAccount:
      // TODO(b/256699588): Add a help-center link for child-accounts.
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_CHILD),
              absl::nullopt};
    case AllowStatus::kVmPolicyBlocked:
    case AllowStatus::kUserPrefBlocked:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_ADMIN),
              absl::nullopt};
    case AllowStatus::kBlockedOnStable:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_CHANNEL),
              GURL("https://support.google.com/chromebook/answer/1086915")};
    case AllowStatus::kBlockedByFlag:
      return {l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_FLAG),
              absl::nullopt};
  }
}

class BorealisInstallerDisallowedDialog : public DialogDelegate {
 public:
  explicit BorealisInstallerDisallowedDialog(AllowStatus status) {
    DCHECK(!g_instance_);

    set_internal_name("BorealisDisallowedDialog");
    SetButtons(ui::DIALOG_BUTTON_OK);
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_BUTTON));
    InitializeView(status);
    SetModalType(ui::MODAL_TYPE_NONE);
    SetOwnedByWidget(true);
    SetShowCloseButton(false);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  }

  ~BorealisInstallerDisallowedDialog() override {
    DCHECK(g_instance_);
    g_instance_ = nullptr;
  }

 private:
  void InitializeView(AllowStatus status) {
    auto view = std::make_unique<views::View>();

    views::LayoutProvider* provider = views::LayoutProvider::Get();
    view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    views::Label* title_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_BOREALIS_DISALLOWED_TITLE),
        CONTEXT_IPH_BUBBLE_TITLE, views::style::STYLE_EMPHASIZED);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetMultiLine(true);
    view->AddChildView(title_label);

    std::pair<std::u16string, absl::optional<GURL>> message_link =
        GetMessageForStatus(status);
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
};

}  // namespace

void ShowInstallerDisallowedDialog(AllowStatus status) {
  DCHECK(status != AllowStatus::kAllowed);

  // TODO(b/248938308): Closing and reopening the dialog this way is not
  // desirable. When we move to webui we should just re-show the current dialog.
  if (g_instance_) {
    g_instance_->CloseNow();
  }

  auto delegate = std::make_unique<BorealisInstallerDisallowedDialog>(status);
  g_instance_ = views::DialogDelegate::CreateDialogWidget(std::move(delegate),
                                                          nullptr, nullptr);
  g_instance_->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, ash::ShelfID(::borealis::kInstallerAppId).Serialize());
  g_instance_->Show();
}

}  // namespace views::borealis
