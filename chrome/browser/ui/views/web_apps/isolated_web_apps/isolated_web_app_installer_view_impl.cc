// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace web_app {
namespace {

constexpr int kIconSize = 32;
constexpr int kInfoPaneCornerRadius = 10;
constexpr int kProgressViewHorizontalPadding = 45;

gfx::Insets BottomPadding(views::DistanceMetric distance) {
  return gfx::Insets::TLBR(
      0, 0, ChromeLayoutProvider::Get()->GetDistanceMetric(distance), 0);
}

std::unique_ptr<views::StyledLabel> CreateLabelWithContextAndStyle(
    views::style::TextContext text_context,
    views::style::TextStyle text_style,
    std::optional<std::u16string> text = std::nullopt) {
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

ui::ImageModel CreateImageModelFromBundleMetadata(
    const SignedWebBundleMetadata& metadata) {
  // WebAppInfoImageSource only stores images at specific sizes. Request the
  // smallest size that's bigger than kIconSize.
  int app_icon_size = 32;
  gfx::ImageSkia icon_image(std::make_unique<WebAppInfoImageSource>(
                                app_icon_size, metadata.icons().any),
                            gfx::Size(app_icon_size, app_icon_size));
  return ui::ImageModel::FromImageSkia(icon_image);
}

// Implicitly converts an id or raw string to a string. Used as an argument to
// functions that need a string, but want to accept either ids or raw strings.
class ToU16String {
 public:
  // NOLINTNEXTLINE(runtime/explicit)
  ToU16String(int string_id) : string_(l10n_util::GetStringUTF16(string_id)) {}

  // NOLINTNEXTLINE(runtime/explicit)
  ToU16String(const std::u16string& string) : string_(string) {}

  const std::u16string& get() const { return string_; }

 private:
  std::u16string string_;
};

// A View containing a progress bar and a description string below it.
class AnnotatedProgressBar : public views::BoxLayoutView {
 public:
  METADATA_HEADER(AnnotatedProgressBar);

  explicit AnnotatedProgressBar(const std::u16string& description) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(gfx::Insets::VH(0, kProgressViewHorizontalPadding));

    progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
    progress_bar_->SetProperty(
        views::kMarginsKey,
        BottomPadding(views::DISTANCE_RELATED_CONTROL_VERTICAL));
    AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY,
        description));
  }

  void UpdateProgress(double percent) { progress_bar_->SetValue(percent); }

 private:
  raw_ptr<views::ProgressBar> progress_bar_;
};
BEGIN_METADATA(AnnotatedProgressBar, views::BoxLayoutView)
END_METADATA

// A View that displays key/value entries in a pane with a different
// background color and a rounded border.
class InfoPane : public views::BoxLayoutView {
 public:
  METADATA_HEADER(InfoPane);

  explicit InfoPane(
      const std::vector<std::pair<int, std::u16string>>& metadata) {
    views::LayoutProvider* provider = views::LayoutProvider::Get();
    SetInsideBorderInsets(
        provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG));
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorSubtleEmphasisBackground, kInfoPaneCornerRadius));
    SetData(metadata);
  }

  void SetData(const std::vector<std::pair<int, std::u16string>>& metadata) {
    RemoveAllChildViews();
    for (const auto& data : metadata) {
      size_t offset;
      views::StyledLabel* label = AddChildView(CreateLabelWithContextAndStyle(
          views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
      label->SetText(
          l10n_util::GetStringFUTF16(data.first, data.second, &offset));

      if (data.second.size() > 0) {
        views::StyledLabel::RangeStyleInfo style;
        style.custom_font = label->GetFontList().Derive(
            0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD);
        label->AddStyleRange(gfx::Range(0, offset), style);
      }
    }
    InvalidateLayout();
  }
};
BEGIN_METADATA(InfoPane, views::BoxLayoutView)
END_METADATA

// The contents view used for all installer screens. This will handle rendering
// common UI elements like icon, title, subtitle, and an optional View for the
// body of the dialog.
class InstallerDialogView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(InstallerDialogView);

  InstallerDialogView(const ui::ImageModel& icon_model,
                      const ToU16String& title,
                      int subtitle_id,
                      std::optional<ToU16String> subtitle_param = absl::nullopt,
                      std::optional<base::RepeatingClosure>
                          subtitle_link_callback = absl::nullopt) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(views::LayoutProvider::Get()->GetInsetsMetric(
        views::InsetsMetric::INSETS_DIALOG));
    SetCollapseMarginsSpacing(true);

    icon_ = AddChildView(std::make_unique<NonAccessibleImageView>());
    icon_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
    icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
    icon_->SetProperty(
        views::kMarginsKey,
        BottomPadding(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
    SetIcon(icon_model);

    title_label_ = AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
    SetTitle(title);

    subtitle_label_ = AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
    SetSubtitle(subtitle_id, subtitle_param, subtitle_link_callback);
  }

  void SetIcon(const ui::ImageModel& icon_model) {
    icon_->SetImage(icon_model);
  }

  void SetTitle(const ToU16String& title) {
    title_label_->SetText(title.get());
  }

  void SetSubtitle(int subtitle_id,
                   std::optional<ToU16String> subtitle_param = absl::nullopt,
                   std::optional<base::RepeatingClosure>
                       subtitle_link_callback = absl::nullopt) {
    if (subtitle_param.has_value()) {
      size_t offset;
      subtitle_label_->SetText(l10n_util::GetStringFUTF16(
          subtitle_id, subtitle_param->get(), &offset));
      if (subtitle_link_callback.has_value()) {
        subtitle_label_->AddStyleRange(
            gfx::Range(offset, offset + subtitle_param->get().length()),
            views::StyledLabel::RangeStyleInfo::CreateForLink(
                *subtitle_link_callback));
      }
    } else {
      subtitle_label_->SetText(l10n_util::GetStringUTF16(subtitle_id));
    }
  }

  template <typename T>
  T* SetContentsView(std::unique_ptr<T> contents_view) {
    CHECK(!contents_wrapper_);
    contents_wrapper_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    contents_wrapper_->SetOrientation(views::BoxLayout::Orientation::kVertical);
    contents_wrapper_->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    contents_wrapper_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0));
    SetFlexForView(contents_wrapper_, 1);
    return contents_wrapper_->AddChildView(std::move(contents_view));
  }

 private:
  raw_ptr<views::ImageView> icon_;
  raw_ptr<views::StyledLabel> title_label_;
  raw_ptr<views::StyledLabel> subtitle_label_;
  raw_ptr<views::BoxLayoutView> contents_wrapper_;
};
BEGIN_METADATA(InstallerDialogView, views::BoxLayoutView)
END_METADATA

}  // namespace

class DisabledView : public InstallerDialogView {
 public:
  METADATA_HEADER(DisabledView);
  explicit DisabledView(IsolatedWebAppInstallerView::Delegate* delegate)
      : InstallerDialogView(
            CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                       ui::kColorAlertMediumSeverityIcon),
            IDS_IWA_INSTALLER_DISABLED_TITLE,
            IDS_IWA_INSTALLER_DISABLED_SUBTITLE,
            IDS_IWA_INSTALLER_DISABLED_CHANGE_PREFERENCE,
            base::BindRepeating(
                &IsolatedWebAppInstallerView::Delegate::OnSettingsLinkClicked,
                base::Unretained(delegate))) {}
};
BEGIN_METADATA(DisabledView, InstallerDialogView)
END_METADATA

class GetMetadataView : public InstallerDialogView {
 public:
  METADATA_HEADER(GetMetadataView);
  GetMetadataView()
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_VERIFICATION_SUBTITLE) {
    auto progress_bar =
        std::make_unique<AnnotatedProgressBar>(l10n_util::GetPluralStringFUTF16(
            IDS_IWA_INSTALLER_VERIFICATION_STATUS, 0));
    progress_bar_ = SetContentsView(std::move(progress_bar));
  }

  void UpdateProgress(double percent) {
    progress_bar_->UpdateProgress(percent);
  }

 private:
  raw_ptr<AnnotatedProgressBar> progress_bar_;
};
BEGIN_METADATA(GetMetadataView, InstallerDialogView)
END_METADATA

class ShowMetadataView : public InstallerDialogView {
 public:
  METADATA_HEADER(ShowMetadataView);
  explicit ShowMetadataView(IsolatedWebAppInstallerView::Delegate* delegate)
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            // The title will be updated to the app name when available.
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_SHOW_METADATA_SUBTITLE,
            IDS_IWA_INSTALLER_SHOW_METADATA_MANAGE_PROFILES,
            base::BindRepeating(&IsolatedWebAppInstallerView::Delegate::
                                    OnManageProfilesLinkClicked,
                                base::Unretained(delegate))) {
    // Initialize the View with dummy data so the initial height calculation
    // will be roughly accurate.
    std::vector<std::pair<int, std::u16string>> info = {
        {IDS_IWA_INSTALLER_SHOW_METADATA_APP_NAME_LABEL, u""},
        {IDS_IWA_INSTALLER_SHOW_METADATA_APP_VERSION_LABEL, u""},
    };
    info_pane_ = SetContentsView(std::make_unique<InfoPane>(info));
  }

  void UpdateInfoPaneContents(
      const std::vector<std::pair<int, std::u16string>> data) {
    info_pane_->SetData(data);
  }

 private:
  raw_ptr<InfoPane> info_pane_;
};
BEGIN_METADATA(ShowMetadataView, InstallerDialogView)
END_METADATA

class InstallView : public InstallerDialogView {
 public:
  METADATA_HEADER(InstallView);
  InstallView()
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            // The title will be updated to the app name when available.
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_INSTALL_SUBTITLE) {
    auto progress_bar = std::make_unique<AnnotatedProgressBar>(
        l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_INSTALL_PROGRESS));
    progress_bar_ = SetContentsView(std::move(progress_bar));
  }

  void UpdateProgress(double percent) {
    progress_bar_->UpdateProgress(percent);
  }

 private:
  raw_ptr<AnnotatedProgressBar> progress_bar_;
};
BEGIN_METADATA(InstallView, InstallerDialogView)
END_METADATA

class InstallSuccessView : public InstallerDialogView {
 public:
  METADATA_HEADER(InstallSuccessView);
  InstallSuccessView()
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            // The title will be updated to the app name when available.
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_SUCCESS_SUBTITLE) {}
};
BEGIN_METADATA(InstallSuccessView, InstallerDialogView)
END_METADATA

// static
void IsolatedWebAppInstallerView::SetDialogButtons(
    views::DialogDelegate* dialog_delegate,
    int close_button_label_id,
    std::optional<int> accept_button_label_id) {
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

// static
std::unique_ptr<IsolatedWebAppInstallerView>
IsolatedWebAppInstallerView::Create(Delegate* delegate) {
  return std::make_unique<IsolatedWebAppInstallerViewImpl>(delegate);
}

IsolatedWebAppInstallerViewImpl::IsolatedWebAppInstallerViewImpl(
    Delegate* delegate)
    : delegate_(delegate),
      disabled_view_(MakeAndAddChildView<DisabledView>(delegate)),
      get_metadata_view_(MakeAndAddChildView<GetMetadataView>()),
      show_metadata_view_(MakeAndAddChildView<ShowMetadataView>(delegate)),
      install_view_(MakeAndAddChildView<InstallView>()),
      install_success_view_(MakeAndAddChildView<InstallSuccessView>()) {
  SetUseDefaultFillLayout(true);
  ShowChildView(nullptr);
}

IsolatedWebAppInstallerViewImpl::~IsolatedWebAppInstallerViewImpl() = default;

void IsolatedWebAppInstallerViewImpl::ShowDisabledScreen() {
  ShowChildView(disabled_view_);
}

void IsolatedWebAppInstallerViewImpl::ShowGetMetadataScreen() {
  ShowChildView(get_metadata_view_);
}

void IsolatedWebAppInstallerViewImpl::UpdateGetMetadataProgress(
    double percent) {
  get_metadata_view_->UpdateProgress(percent);
}

void IsolatedWebAppInstallerViewImpl::ShowMetadataScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  std::vector<std::pair<int, std::u16string>> data = {
      {IDS_IWA_INSTALLER_SHOW_METADATA_APP_NAME_LABEL,
       bundle_metadata.app_name()},
      {IDS_IWA_INSTALLER_SHOW_METADATA_APP_VERSION_LABEL,
       base::UTF8ToUTF16(bundle_metadata.version().GetString())},
  };
  show_metadata_view_->UpdateInfoPaneContents(data);
  show_metadata_view_->SetTitle(bundle_metadata.app_name());
  show_metadata_view_->SetIcon(
      CreateImageModelFromBundleMetadata(bundle_metadata));
  ShowChildView(show_metadata_view_);
}

void IsolatedWebAppInstallerViewImpl::ShowInstallScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  install_view_->SetTitle(bundle_metadata.app_name());
  install_view_->SetIcon(CreateImageModelFromBundleMetadata(bundle_metadata));
  ShowChildView(install_view_);
}

void IsolatedWebAppInstallerViewImpl::UpdateInstallProgress(double percent) {
  install_view_->UpdateProgress(percent);
}

void IsolatedWebAppInstallerViewImpl::ShowInstallSuccessScreen(
    const SignedWebBundleMetadata& bundle_metadata) {
  install_success_view_->SetTitle(bundle_metadata.app_name());
  install_success_view_->SetSubtitle(IDS_IWA_INSTALLER_SUCCESS_SUBTITLE,
                                     bundle_metadata.app_name());
  install_success_view_->SetIcon(
      CreateImageModelFromBundleMetadata(bundle_metadata));
  ShowChildView(install_success_view_);
}

void IsolatedWebAppInstallerViewImpl::ShowDialog(
    const IsolatedWebAppInstallerModel::DialogContent& dialog_content) {
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      GetWidget()->GetContentsView(), views::BubbleBorder::FLOAT);
  bubble_delegate->SetModalType(ui::MODAL_TYPE_CHILD);
  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble_delegate->set_close_on_deactivate(false);

  ui::ImageModel image =
      dialog_content.is_error
          ? CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                       ui::kColorAlertMediumSeverityIcon)
          : CreateImageModelFromVector(kSecurityIcon, ui::kColorAccent);
  if (dialog_content.details_link.has_value()) {
    bubble_delegate->SetContentsView(std::make_unique<InstallerDialogView>(
        image, dialog_content.message, dialog_content.details,
        dialog_content.details_link->first,
        dialog_content.details_link->second));
  } else {
    bubble_delegate->SetContentsView(std::make_unique<InstallerDialogView>(
        image, dialog_content.message, dialog_content.details));
  }

  SetDialogButtons(bubble_delegate.get(), IDS_APP_CLOSE,
                   dialog_content.accept_message);

  bubble_delegate->SetCancelCallback(base::BindOnce(
      &Delegate::OnChildDialogCanceled, base::Unretained(delegate_)));
  bubble_delegate->SetAcceptCallback(base::BindOnce(
      &Delegate::OnChildDialogAccepted, base::Unretained(delegate_)));

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate))->Show();
}

void IsolatedWebAppInstallerViewImpl::ShowChildView(views::View* view) {
  for (views::View* child : children()) {
    child->SetVisible(child == view);
  }
}

BEGIN_METADATA(IsolatedWebAppInstallerViewImpl, views::View)
END_METADATA

}  // namespace web_app
