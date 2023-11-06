// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace web_app {

namespace {

constexpr int kIconSize = 32;
constexpr int kProgressViewHorizontalPadding = 45;

void ConfigureBoxLayoutView(views::BoxLayoutView* view) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
  view->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
}

std::unique_ptr<views::StyledLabel> CreateLabelWithContextAndStyle(
    views::style::TextContext text_context,
    views::style::TextStyle text_style,
    absl::optional<std::u16string> text = absl::nullopt) {
  auto label = std::make_unique<views::StyledLabel>();
  label->SetTextContext(text_context);
  label->SetDefaultTextStyle(text_style);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  if (text) {
    label->SetText(*text);
  }
  return label;
}

ui::ImageModel CreateImageModelFromVector(const gfx::VectorIcon& vector_icon,
                                          ui::ColorId color_id) {
  return ui::ImageModel::FromVectorIcon(vector_icon, color_id, kIconSize);
}

}  // namespace

// The contents view used for all installer screens. This will handle rendering
// common UI elements like icon, title, subtitle, and an optional View for the
// body of the dialog.
class InstallerDialogView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(InstallerDialogView);

  // The message_id of the link's body and a callback to run when it's clicked.
  using LinkInfo = std::pair<int, base::RepeatingClosure>;

  InstallerDialogView(const ui::ImageModel& icon_model,
                      int title_id,
                      int subtitle_id,
                      absl::optional<LinkInfo> subtitle_link = absl::nullopt,
                      std::unique_ptr<views::View> contents_view = nullptr) {
    ConfigureBoxLayoutView(this);

    auto* icon = AddChildView(std::make_unique<NonAccessibleImageView>());
    icon->SetImage(icon_model);
    icon->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);

    views::StyledLabel* title = AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
    title->SetText(l10n_util::GetStringUTF16(title_id));

    views::StyledLabel* subtitle = AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    if (subtitle_link.has_value()) {
      std::u16string link_text =
          l10n_util::GetStringUTF16(subtitle_link->first);
      size_t offset;
      subtitle->SetText(
          l10n_util::GetStringFUTF16(subtitle_id, link_text, &offset));
      subtitle->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                              views::StyledLabel::RangeStyleInfo::CreateForLink(
                                  subtitle_link->second));
    } else {
      subtitle->SetText(l10n_util::GetStringUTF16(subtitle_id));
    }

    if (contents_view) {
      views::View* contents = AddChildView(std::move(contents_view));
      SetFlexForView(contents, 1);
    }
  }
};
BEGIN_METADATA(InstallerDialogView, views::BoxLayoutView)
END_METADATA

// static
void IsolatedWebAppInstallerView::SetDialogButtons(
    views::DialogDelegate* dialog_delegate,
    int close_button_label_id,
    absl::optional<int> accept_button_label_id) {
  if (!dialog_delegate) {
    return;
  }

  int buttons = ui::DIALOG_BUTTON_CANCEL;
  dialog_delegate->SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(close_button_label_id));
  if (accept_button_label_id.has_value()) {
    buttons |= ui::DIALOG_BUTTON_OK;
    dialog_delegate->SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(accept_button_label_id.value()));
  }
  dialog_delegate->SetButtons(buttons);
}

IsolatedWebAppInstallerView::IsolatedWebAppInstallerView(Delegate* delegate)
    : delegate_(delegate), dialog_view_(nullptr), initialized_(false) {}

IsolatedWebAppInstallerView::~IsolatedWebAppInstallerView() = default;

void IsolatedWebAppInstallerView::ShowDisabledScreen() {
  InstallerDialogView::LinkInfo link(
      IDS_IWA_INSTALLER_DISABLED_CHANGE_PREFERENCE,
      base::BindRepeating(&Delegate::OnSettingsLinkClicked,
                          base::Unretained(delegate_)));
  ShowScreen(std::make_unique<InstallerDialogView>(
      CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                 ui::kColorAlertMediumSeverityIcon),
      IDS_IWA_INSTALLER_DISABLED_TITLE, IDS_IWA_INSTALLER_DISABLED_SUBTITLE,
      link));
}

void IsolatedWebAppInstallerView::ShowGetMetadataScreen() {
  auto progress_view = std::make_unique<views::BoxLayoutView>();
  ConfigureBoxLayoutView(progress_view.get());
  progress_view->SetInsideBorderInsets(
      gfx::Insets::VH(0, kProgressViewHorizontalPadding));

  views::ProgressBar* progress_bar =
      progress_view->AddChildView(std::make_unique<views::ProgressBar>());
  progress_view->AddChildView(CreateLabelWithContextAndStyle(
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY,
      l10n_util::GetPluralStringFUTF16(IDS_IWA_INSTALLER_VERIFICATION_STATUS,
                                       0)));

  ShowScreen(std::make_unique<InstallerDialogView>(
                 CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
                 IDS_IWA_INSTALLER_VERIFICATION_TITLE,
                 IDS_IWA_INSTALLER_VERIFICATION_SUBTITLE,
                 /*subtitle_link=*/absl::nullopt, std::move(progress_view)),
             progress_bar);
}

void IsolatedWebAppInstallerView::UpdateGetMetadataProgress(
    double percent,
    int minutes_remaining) {
  CHECK(progress_bar_);
  progress_bar_->SetValue(percent / 100.0);
}

void IsolatedWebAppInstallerView::ShowMetadataScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
  ShowDisabledScreen();
}

void IsolatedWebAppInstallerView::ShowInstallScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::UpdateInstallProgress(double percent,
                                                        int minutes_remaining) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::ShowInstallSuccessScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
}

void IsolatedWebAppInstallerView::ShowScreen(
    std::unique_ptr<InstallerDialogView> dialog_view,
    views::ProgressBar* progress_bar) {
  if (!initialized_) {
    initialized_ = true;
    views::LayoutProvider* provider = views::LayoutProvider::Get();
    SetInsideBorderInsets(
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG));
  }
  progress_bar_ = progress_bar;
  if (dialog_view_) {
    RemoveChildView(dialog_view_);
  }
  dialog_view_ = AddChildView(std::move(dialog_view));
  InvalidateLayout();
}

void IsolatedWebAppInstallerView::ShowDialog(
    const IsolatedWebAppInstallerModel::DialogContent& dialog_content) {
  CHECK(initialized_);
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      GetWidget()->GetContentsView(), views::BubbleBorder::FLOAT);
  bubble_delegate->SetModalType(ui::MODAL_TYPE_CHILD);
  bubble_delegate->set_close_on_deactivate(false);

  bubble_delegate->SetContentsView(std::make_unique<InstallerDialogView>(
      dialog_content.is_error
          ? CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                       ui::kColorAlertMediumSeverityIcon)
          : CreateImageModelFromVector(kSecurityIcon, ui::kColorAccent),
      dialog_content.message, dialog_content.details,
      dialog_content.details_link));

  SetDialogButtons(bubble_delegate.get(), IDS_APP_CLOSE,
                   /*accept_button_label_id=*/absl::nullopt);

  bubble_delegate->SetCancelCallback(base::BindOnce(
      &Delegate::OnChildDialogCanceled, base::Unretained(delegate_)));
  bubble_delegate->SetAcceptCallback(base::BindOnce(
      &Delegate::OnChildDialogAccepted, base::Unretained(delegate_)));

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate))->Show();
}

BEGIN_METADATA(IsolatedWebAppInstallerView, views::BoxLayoutView)
END_METADATA

}  // namespace web_app
