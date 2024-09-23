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
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

namespace web_app {
namespace {

constexpr int kIconSize = 32;
constexpr int kNestedDialogIconSize = 24;
constexpr int kInfoPaneCornerRadius = 10;
constexpr int kProgressViewHorizontalPadding = 45;

views::View* GetRootView(views::View* view) {
  while (view->parent()) {
    view = view->parent();
  }
  return view;
}

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
  METADATA_HEADER(AnnotatedProgressBar, views::BoxLayoutView)

 public:
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
BEGIN_METADATA(AnnotatedProgressBar)
END_METADATA

// A View that displays key/value entries in a pane with a different
// background color and a rounded border.
class InfoPane : public views::BoxLayoutView {
  METADATA_HEADER(InfoPane, views::BoxLayoutView)

 public:
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
BEGIN_METADATA(InfoPane)
END_METADATA

// The contents view used for all installer screens. This will handle rendering
// common UI elements like icon, title, subtitle, and an optional View for the
// body of the dialog.
class InstallerDialogView : public views::BoxLayoutView {
  METADATA_HEADER(InstallerDialogView, views::BoxLayoutView)

 public:
  InstallerDialogView(const ui::ImageModel& icon_model,
                      const ToU16String& title,
                      int subtitle_id,
                      std::optional<ToU16String> subtitle_param = std::nullopt,
                      std::optional<base::RepeatingClosure>
                          subtitle_link_callback = std::nullopt) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(views::LayoutProvider::Get()->GetInsetsMetric(
        views::InsetsMetric::INSETS_DIALOG));
    SetCollapseMarginsSpacing(true);
    GetViewAccessibility().SetRole(ax::mojom::Role::kMain);

    auto* header = AddChildView(std::make_unique<views::BoxLayoutView>());
    header->SetOrientation(views::BoxLayout::Orientation::kVertical);
    header->SetDefaultFlex(0);
    header->GetViewAccessibility().SetRole(
        ax::mojom::Role::kRegion,
        l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_BODY_SCREENREADER_NAME));

    icon_ = header->AddChildView(std::make_unique<NonAccessibleImageView>());
    icon_->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
    icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
    icon_->SetProperty(
        views::kMarginsKey,
        BottomPadding(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
    SetIcon(icon_model);

    title_label_ = header->AddChildView(CreateLabelWithContextAndStyle(
        views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
    title_label_->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    SetTitle(title);

    subtitle_label_ = header->AddChildView(CreateLabelWithContextAndStyle(
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
                   std::optional<ToU16String> subtitle_param = std::nullopt,
                   std::optional<base::RepeatingClosure>
                       subtitle_link_callback = std::nullopt) {
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
  T* SetContentsView(std::unique_ptr<T> contents_view,
                     std::optional<int> region_name_id) {
    CHECK(!contents_wrapper_);
    contents_wrapper_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    contents_wrapper_->SetOrientation(views::BoxLayout::Orientation::kVertical);
    contents_wrapper_->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    contents_wrapper_->SetInsideBorderInsets(
        views::LayoutProvider::Get()
            ->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG)
            .set_left_right(0, 0));

    contents_wrapper_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0));
    if (region_name_id.has_value()) {
      contents_wrapper_->GetViewAccessibility().SetRole(
          ax::mojom::Role::kRegion,
          l10n_util::GetStringUTF16(region_name_id.value()));
    } else {
      contents_wrapper_->GetViewAccessibility().SetRole(
          ax::mojom::Role::kRegion);
    }
    SetFlexForView(contents_wrapper_, 1);
    return contents_wrapper_->AddChildView(std::move(contents_view));
  }

 private:
  raw_ptr<views::ImageView> icon_;
  raw_ptr<views::StyledLabel> title_label_;
  raw_ptr<views::StyledLabel> subtitle_label_;
  raw_ptr<views::BoxLayoutView> contents_wrapper_;
};

BEGIN_METADATA(InstallerDialogView)
END_METADATA

}  // namespace

class DisabledView : public InstallerDialogView {
  METADATA_HEADER(DisabledView, InstallerDialogView)

 public:
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

BEGIN_METADATA(DisabledView)
END_METADATA

class GetMetadataView : public InstallerDialogView {
  METADATA_HEADER(GetMetadataView, InstallerDialogView)

 public:
  GetMetadataView()
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_VERIFICATION_SUBTITLE) {
    auto progress_bar =
        std::make_unique<AnnotatedProgressBar>(l10n_util::GetPluralStringFUTF16(
            IDS_IWA_INSTALLER_VERIFICATION_STATUS, 0));
    progress_bar_ = SetContentsView(
        std::move(progress_bar), IDS_IWA_INSTALLER_PROGRESS_SCREENREADER_NAME);
  }

  void UpdateProgress(double percent) {
    progress_bar_->UpdateProgress(percent);
  }

 private:
  raw_ptr<AnnotatedProgressBar> progress_bar_;
};

BEGIN_METADATA(GetMetadataView)
END_METADATA

class ShowMetadataView : public InstallerDialogView {
  METADATA_HEADER(ShowMetadataView, InstallerDialogView)

 public:
  explicit ShowMetadataView(IsolatedWebAppInstallerView::Delegate* delegate)
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            // The title will be updated to the app name when available.
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_SHOW_METADATA_SUBTITLE) {
    // Initialize the View with dummy data so the initial height calculation
    // will be roughly accurate.
    std::vector<std::pair<int, std::u16string>> info = {
        {IDS_IWA_INSTALLER_SHOW_METADATA_APP_NAME_LABEL, u""},
        {IDS_IWA_INSTALLER_SHOW_METADATA_APP_VERSION_LABEL, u""},
    };
    info_pane_ = SetContentsView(std::make_unique<InfoPane>(info),
                                 IDS_IWA_INSTALLER_DETAILS_SCREENREADER_NAME);
  }

  void UpdateInfoPaneContents(
      const std::vector<std::pair<int, std::u16string>> data) {
    info_pane_->SetData(data);
  }

 private:
  raw_ptr<InfoPane> info_pane_;
};

BEGIN_METADATA(ShowMetadataView)
END_METADATA

class InstallView : public InstallerDialogView {
  METADATA_HEADER(InstallView, InstallerDialogView)

 public:
  InstallView()
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            // The title will be updated to the app name when available.
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_INSTALL_SUBTITLE) {
    auto progress_bar = std::make_unique<AnnotatedProgressBar>(
        l10n_util::GetStringUTF16(IDS_IWA_INSTALLER_INSTALL_PROGRESS));
    progress_bar_ = SetContentsView(
        std::move(progress_bar), IDS_IWA_INSTALLER_PROGRESS_SCREENREADER_NAME);
  }

  void UpdateProgress(double percent) {
    progress_bar_->UpdateProgress(percent);
  }

 private:
  raw_ptr<AnnotatedProgressBar> progress_bar_;
};

BEGIN_METADATA(InstallView)
END_METADATA

class InstallSuccessView : public InstallerDialogView {
  METADATA_HEADER(InstallSuccessView, InstallerDialogView)

 public:
  InstallSuccessView()
      : InstallerDialogView(
            CreateImageModelFromVector(kFingerprintIcon, ui::kColorAccent),
            // The title will be updated to the app name when available.
            IDS_IWA_INSTALLER_VERIFICATION_TITLE,
            IDS_IWA_INSTALLER_SUCCESS_SUBTITLE) {
    auto image = std::make_unique<NonAccessibleImageView>();
    image->SetImage(ui::ImageModel::FromResourceId(IDR_IWA_INSTALL_SUCCESS));
    SetContentsView(std::move(image), /*region_name_id=*/std::nullopt);
  }
};

BEGIN_METADATA(InstallSuccessView)
END_METADATA

class DimOverlayView : public views::View {
  METADATA_HEADER(DimOverlayView, views::View)

 public:
  DimOverlayView() {
    SetBackground(views::CreateSolidBackground(SkColorSetARGB(125, 0, 0, 0)));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  std::string GetObjectName() const override { return "DimOverlayView"; }
};

BEGIN_METADATA(DimOverlayView)
END_METADATA

void IsolatedWebAppInstallerViewImpl::Dim(bool dim) {
  views::View* root = GetRootView(this);

  // Undim: remove all |DimOverlayView|
  if (!dim) {
    for (views::View* child : root->children()) {
      if (child->GetObjectName().compare("DimOverlayView") == 0) {
        // |RemoveChildViewT()| returns the ownership of the child, which gets
        // dropped, effectively deleting the child from memory.
        root->RemoveChildViewT(child);
      }
    }
    return;
  }

  // Dim: add a |DimOverlayView| as the last child.
  root->AddChildView(std::make_unique<DimOverlayView>());
}

// static
void IsolatedWebAppInstallerView::SetDialogButtons(
    views::DialogDelegate* dialog_delegate,
    int close_button_label_id,
    std::optional<int> accept_button_label_id) {
  if (!dialog_delegate) {
    return;
  }

  int buttons = static_cast<int>(ui::mojom::DialogButton::kCancel);
  dialog_delegate->SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(close_button_label_id));
  if (accept_button_label_id.has_value()) {
    buttons = static_cast<int>(ui::mojom::DialogButton::kOk) |
              static_cast<int>(ui::mojom::DialogButton::kCancel);
    dialog_delegate->SetButtonLabel(
        ui::mojom::DialogButton::kOk,
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
      install_success_view_(MakeAndAddChildView<InstallSuccessView>()),
      dialog_visible_(false) {
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

views::Widget* IsolatedWebAppInstallerViewImpl::ShowDialog(
    const IsolatedWebAppInstallerModel::Dialog& dialog) {
  Dim(true);
  return absl::visit(
      base::Overloaded{
          [this](const IsolatedWebAppInstallerModel::BundleInvalidDialog&) {
            return ShowChildDialog(
                IDS_IWA_INSTALLER_VERIFICATION_ERROR_TITLE,
                ui::DialogModelLabel(
                    IDS_IWA_INSTALLER_VERIFICATION_ERROR_SUBTITLE),
                CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                           ui::kColorAlertMediumSeverityIcon),
                /*ok_label=*/std::nullopt);
          },
          [this](
              const IsolatedWebAppInstallerModel::BundleAlreadyInstalledDialog&
                  already_installed_dialog) {
            std::u16string title = l10n_util::GetStringFUTF16(
                IDS_IWA_INSTALLER_ALREADY_INSTALLED_TITLE,
                already_installed_dialog.bundle_name);

            std::string installed_version =
                already_installed_dialog.installed_version.GetString();
            auto subtitle = ui::DialogModelLabel::CreateWithReplacements(
                IDS_IWA_INSTALLER_ALREADY_INSTALLED_SUBTITLE,
                {
                    ui::DialogModelLabel::CreatePlainText(
                        already_installed_dialog.bundle_name),
                    ui::DialogModelLabel::CreatePlainText(
                        base::UTF8ToUTF16(installed_version)),
                });
            return ShowChildDialog(
                title, subtitle,
                CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                           ui::kColorAlertMediumSeverityIcon),
                /*ok_label=*/std::nullopt);
          },
          [this](const IsolatedWebAppInstallerModel::ConfirmInstallationDialog&
                     confirm_installation_dialog) {
            auto subtitle = ui::DialogModelLabel::CreateWithReplacement(
                IDS_IWA_INSTALLER_CONFIRM_SUBTITLE,
                ui::DialogModelLabel::CreateLink(
                    IDS_IWA_INSTALLER_CONFIRM_LEARN_MORE,
                    confirm_installation_dialog.learn_more_callback));
            return ShowChildDialog(
                IDS_IWA_INSTALLER_CONFIRM_TITLE, subtitle,
                CreateImageModelFromVector(kPrivacyTipIcon, ui::kColorAccent),
                IDS_IWA_INSTALLER_CONFIRM_CONTINUE);
          },
          [this](
              const IsolatedWebAppInstallerModel::InstallationFailedDialog&) {
            return ShowChildDialog(
                IDS_IWA_INSTALLER_INSTALL_FAILED_TITLE,
                ui::DialogModelLabel(IDS_IWA_INSTALLER_INSTALL_FAILED_SUBTITLE),
                CreateImageModelFromVector(vector_icons::kErrorOutlineIcon,
                                           ui::kColorAlertMediumSeverityIcon),
                IDS_IWA_INSTALLER_INSTALL_FAILED_RETRY);
          }},
      dialog);
}

gfx::Size IsolatedWebAppInstallerViewImpl::GetMaximumSize() const {
  // `SetCanResize` only works in ash. ash will consider Lacros windows to be
  // non-resizable if their min and max height are the same. To achieve this,
  // we set the max size to the View's preferred size.
  int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

views::Widget* IsolatedWebAppInstallerViewImpl::ShowChildDialog(
    int title,
    const ui::DialogModelLabel& subtitle,
    const ui::ImageModel& icon_model,
    std::optional<int> ok_label) {
  return ShowChildDialog(l10n_util::GetStringUTF16(title), subtitle, icon_model,
                         ok_label);
}

views::Widget* IsolatedWebAppInstallerViewImpl::ShowChildDialog(
    const std::u16string& title,
    const ui::DialogModelLabel& subtitle,
    const ui::ImageModel& icon_model,
    std::optional<int> ok_label) {
  CHECK(!dialog_visible_);
  dialog_visible_ = true;

  ui::DialogModel::Builder dialog_model_builder;
  dialog_model_builder
      .SetInternalName(IsolatedWebAppInstallerView::kNestedDialogWidgetName)
      .SetTitle(title)
      .AddParagraph(ui::DialogModelLabel(subtitle).set_is_secondary())
      .DisableCloseOnDeactivate()
      .AddCancelButton(base::BindOnce(&Delegate::OnChildDialogCanceled,
                                      base::Unretained(delegate_)))
      .SetDialogDestroyingCallback(base::BindOnce(
          &IsolatedWebAppInstallerViewImpl::OnChildDialogDestroying,
          weak_ptr_factory_.GetWeakPtr()));

  if (ok_label.has_value()) {
    dialog_model_builder.AddOkButton(
        base::BindOnce(&Delegate::OnChildDialogAccepted,
                       base::Unretained(delegate_)),
        ui::DialogModel::Button::Params().SetLabel(
            l10n_util::GetStringUTF16(ok_label.value())));
  }

  // None of DialogModel's banner image, icon, or main image display the icon
  // the way we want, so we have to manually create a header View that
  // positions the icon correctly.
  auto header = std::make_unique<views::BoxLayoutView>();
  int inset = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  header->SetInsideBorderInsets(gfx::Insets::TLBR(inset, inset, 0, inset));
  auto* icon = header->AddChildView(std::make_unique<NonAccessibleImageView>());
  icon->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  icon->SetImageSize(gfx::Size(kNestedDialogIconSize, kNestedDialogIconSize));
  icon->SetImage(icon_model);

  std::unique_ptr<views::BubbleDialogModelHost> bubble =
      views::BubbleDialogModelHost::CreateModal(dialog_model_builder.Build(),
                                                ui::mojom::ModalType::kChild);
  bubble->SetAnchorView(GetWidget()->GetContentsView());
  bubble->SetArrow(views::BubbleBorder::FLOAT);
  bubble->set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble->RegisterWidgetInitializedCallback(base::BindOnce(
      [](views::BubbleDialogModelHost* bubble,
         std::unique_ptr<views::View> header) {
        bubble->GetBubbleFrameView()->SetHeaderView(std::move(header));
      },
      // base::Unretained is safe here because the callback is invoked when
      // `bubble` is initialized, at which point it must still be alive.
      base::Unretained(bubble.get()), std::move(header)));

  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  widget->Show();
  return widget;
}

void IsolatedWebAppInstallerViewImpl::ShowChildView(views::View* view) {
  for (views::View* child : children()) {
    child->SetVisible(child == view);
  }
}

void IsolatedWebAppInstallerViewImpl::OnChildDialogDestroying() {
  dialog_visible_ = false;
  Dim(false);
  delegate_->OnChildDialogDestroying();
}

BEGIN_METADATA(IsolatedWebAppInstallerViewImpl)
END_METADATA

}  // namespace web_app
