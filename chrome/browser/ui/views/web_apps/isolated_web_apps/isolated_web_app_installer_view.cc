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
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
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

}  // namespace

// Base class for all installer screens that handles common UI elements like
// icons, title, and subtitle.
class InstallerScreenView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(InstallerScreenView);

  explicit InstallerScreenView(const ui::ImageModel& icon_model) {
    ConfigureBoxLayoutView(this);

    auto* icon = AddChildView(std::make_unique<NonAccessibleImageView>());
    icon->SetImage(icon_model);
    icon->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);

    title_ = AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
    subtitle_ = AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  }

  InstallerScreenView(const ui::ImageModel& icon_model,
                      const std::u16string& title,
                      const std::u16string& subtitle)
      : InstallerScreenView(icon_model) {
    SetTitle(title);
    SetSubtitle(subtitle);
  }

  virtual void SetProgress(double percent, int minutes_remaining) {}

 protected:
  void SetTitle(const std::u16string& title) { title_->SetText(title); }

  void SetSubtitle(const std::u16string& subtitle) {
    subtitle_->SetText(subtitle);
  }

  void SetSubtitleWithLink(const std::u16string& subtitle,
                           const gfx::Range& link_range,
                           base::RepeatingClosure link_handler) {
    SetSubtitle(subtitle);
    subtitle_->AddStyleRange(
        link_range,
        views::StyledLabel::RangeStyleInfo::CreateForLink(link_handler));
  }

  void SetContentsView(std::unique_ptr<views::View> contents) {
    CHECK(!contents_);
    contents_ = AddChildView(std::move(contents));
    SetFlexForView(contents_, 1);
  }

 private:
  raw_ptr<views::StyledLabel> title_;
  raw_ptr<views::StyledLabel> subtitle_;
  raw_ptr<views::View> contents_;
};
BEGIN_METADATA(InstallerScreenView, views::BoxLayoutView)
END_METADATA

class GetMetadataScreen : public InstallerScreenView {
 public:
  METADATA_HEADER(GetMetadataScreen);

  GetMetadataScreen()
      : InstallerScreenView(
            ui::ImageModel::FromVectorIcon(kFingerprintIcon,
                                           ui::kColorAccent,
                                           kIconSize),
            l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_VERIFICATION_TITLE),
            l10n_util::GetStringUTF16(
                IDS_IWA_INSTALLER_VERIFICATION_SUBTITLE)) {
    SetContentsView(CreateContentsView());
  }

  void SetProgress(double percent, int minutes_remaining) override {
    progress_bar_->SetValue(percent / 100.0);
  }

 private:
  std::unique_ptr<views::View> CreateContentsView() {
    auto progress_view = std::make_unique<views::BoxLayoutView>();
    ConfigureBoxLayoutView(progress_view.get());
    progress_view->SetInsideBorderInsets(
        gfx::Insets::VH(0, kProgressViewHorizontalPadding));

    progress_bar_ =
        progress_view->AddChildView(std::make_unique<views::ProgressBar>());
    progress_view->AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY,
        l10n_util::GetPluralStringFUTF16(IDS_IWA_INSTALLER_VERIFICATION_STATUS,
                                         0)));
    return progress_view;
  }

  raw_ptr<views::ProgressBar> progress_bar_;
};
BEGIN_METADATA(GetMetadataScreen, InstallerScreenView)
END_METADATA

class DisabledScreen : public InstallerScreenView {
 public:
  METADATA_HEADER(DisabledScreen);

  explicit DisabledScreen(base::RepeatingClosure change_preference_handler)
      : InstallerScreenView(
            ui::ImageModel::FromVectorIcon(vector_icons::kErrorOutlineIcon,
                                           ui::kColorAlertMediumSeverityIcon,
                                           kIconSize)) {
    SetTitle(l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_DISABLED_TITLE));

    std::u16string change_preference =
        l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_DISABLED_CHANGE_PREFERENCE);
    size_t offset;
    std::u16string subtitle = l10n_util::GetStringFUTF16(
        IDS_IWA_INSTALLER_DISABLED_SUBTITLE, change_preference, &offset);
    SetSubtitleWithLink(subtitle,
                        gfx::Range(offset, offset + change_preference.length()),
                        change_preference_handler);
  }
};
BEGIN_METADATA(DisabledScreen, InstallerScreenView)
END_METADATA

IsolatedWebAppInstallerView::IsolatedWebAppInstallerView(Delegate* delegate)
    : delegate_(delegate), screen_(nullptr), initialized_(false) {}

IsolatedWebAppInstallerView::~IsolatedWebAppInstallerView() = default;

void IsolatedWebAppInstallerView::ShowDisabledScreen() {
  ShowScreen(std::make_unique<DisabledScreen>(base::BindRepeating(
      &Delegate::OnSettingsLinkClicked, base::Unretained(delegate_))));
}

void IsolatedWebAppInstallerView::ShowGetMetadataScreen() {
  ShowScreen(std::make_unique<GetMetadataScreen>());
}

void IsolatedWebAppInstallerView::UpdateGetMetadataProgress(
    double percent,
    int minutes_remaining) {
  CHECK(screen_);
  screen_->SetProgress(percent, minutes_remaining);
}

void IsolatedWebAppInstallerView::ShowMetadataScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  // TODO(crbug.com/1479140): Implement
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
    std::unique_ptr<InstallerScreenView> screen) {
  if (!initialized_) {
    initialized_ = true;
    views::LayoutProvider* provider = views::LayoutProvider::Get();
    SetInsideBorderInsets(
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG));
  }

  if (screen_) {
    RemoveChildView(screen_);
  }
  screen_ = AddChildView(std::move(screen));
}

BEGIN_METADATA(IsolatedWebAppInstallerView, views::BoxLayoutView)
END_METADATA

}  // namespace web_app
