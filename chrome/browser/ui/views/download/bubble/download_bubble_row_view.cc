// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<views::View> CreateLabelWrapper() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto label_wrapper = std::make_unique<views::View>();
  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(0, icon_label_spacing));
  label_wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));
  return label_wrapper;
}

// Whether we are warning about a dangerous/malicious download.
bool is_download_warning(download::DownloadItemMode mode) {
  return (mode == download::DownloadItemMode::kDangerous) ||
         (mode == download::DownloadItemMode::kMalicious);
}

ui::ImageModel GetDefaultIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kInsertDriveFileOutlineIcon, ui::kColorIcon,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE));
}

constexpr int kDownloadButtonHeight = 24;
constexpr int kDownloadSubpageIconMargin = 8;

}  // namespace

void DownloadBubbleRowView::UpdateBubbleUIInfo() {
  auto mode = download::GetDesiredDownloadItemMode(model_.get());
  auto state = model_->GetState();
  bool mode_unchanged = (mode_ == mode);
  if (mode_unchanged && (state_ == state))
    return;

  mode_ = mode;
  state_ = state;

  // If either of mode or state changes, we might need to change UI.
  ui_info_ = model_->GetBubbleUIInfo();

  // This should only be logged once per download, so only for change in modes
  // to warning.
  if (!mode_unchanged && is_download_warning(mode)) {
    const auto danger_type = model_->GetDangerType();
    const auto file_path = model_->GetTargetFilePath();
    bool is_https = model_->GetURL().SchemeIs(url::kHttpsScheme);
    bool has_user_gesture = model_->HasUserGesture();
    RecordDangerousDownloadWarningShown(danger_type, file_path, is_https,
                                        has_user_gesture);
  }
}

void DownloadBubbleRowView::AddedToWidget() {
  const display::Screen* const screen = display::Screen::GetScreen();
  current_scale_ = screen->GetDisplayNearestView(GetWidget()->GetNativeView())
                       .device_scale_factor();
  LoadIcon();
}

void DownloadBubbleRowView::OnThemeChanged() {
  HoverButton::OnThemeChanged();
  secondary_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui_info_.secondary_color));
}

void DownloadBubbleRowView::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  current_scale_ = new_device_scale_factor;
  LoadIcon();
}

void DownloadBubbleRowView::SetIconFromImageModel(ui::ImageModel icon) {
  if (icon.IsEmpty()) {
    icon_->SetImage(GetDefaultIcon());
  } else {
    icon_->SetImage(icon);
  }
}

void DownloadBubbleRowView::SetIconFromImage(gfx::Image icon) {
  SetIconFromImageModel(ui::ImageModel::FromImage(icon));
}

void DownloadBubbleRowView::LoadIcon() {
  // The correct scale_factor is set only in the AddedToWidget()
  if (!GetWidget())
    return;

  if (ui_info_.icon_model_override) {
    if (last_overriden_icon_ == ui_info_.icon_model_override)
      return;
    last_overriden_icon_ = ui_info_.icon_model_override;
    SetIconFromImageModel(ui::ImageModel::FromVectorIcon(
        *ui_info_.icon_model_override, ui_info_.secondary_color,
        GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    return;
  }

  base::FilePath file_path = model_->GetTargetFilePath();
  // Use a default icon (drive file outline icon) in case we have an empty
  // target path, which is empty for non download offline items, and newly
  // started in-progress downloads.
  if (file_path.empty()) {
    if (already_set_default_icon_)
      return;
    already_set_default_icon_ = true;
    SetIconFromImageModel(GetDefaultIcon());
    return;
  }

  IconManager* const im = g_browser_process->icon_manager();
  const gfx::Image* const file_icon_image =
      im->LookupIconFromFilepath(file_path, IconLoader::SMALL, current_scale_);

  if (file_icon_image) {
    SetIconFromImage(*file_icon_image);
  } else {
    im->LoadIcon(file_path, IconLoader::SMALL, current_scale_,
                 base::BindOnce(&DownloadBubbleRowView::SetIconFromImage,
                                weak_factory_.GetWeakPtr()),
                 &cancelable_task_tracker_);
  }
}

DownloadBubbleRowView::~DownloadBubbleRowView() {
  if (model_.get()) {
    model_->RemoveObserver(this);
  }
}

views::View* DownloadBubbleRowView::TargetForRect(views::View* root,
                                                  const gfx::Rect& rect) {
  CHECK_EQ(root, this);
  if (!views::UsePointBasedTargeting(rect))
    return views::ViewTargeterDelegate::TargetForRect(root, rect);

  views::View* v = views::ViewTargeterDelegate::TargetForRect(root, rect);
  // Return the button if that is the target.
  if (v && (v == cancel_button_ || v == discard_button_ || v == scan_button_ ||
            v == open_now_button_)) {
    return v;
  }
  // All events go to this otherwise.
  return this;
}

DownloadBubbleRowView::DownloadBubbleRowView(
    DownloadUIModel::DownloadUIModelPtr model,
    DownloadBubbleRowListView* row_list_view,
    DownloadBubbleUIController* bubble_controller,
    DownloadBubbleNavigationHandler* navigation_handler)
    : HoverButton(
          base::BindRepeating(&DownloadBubbleRowView::OnMainButtonPressed,
                              base::Unretained(this)),
          std::u16string()),
      model_(std::move(model)),
      context_menu_(
          std::make_unique<DownloadShelfContextMenuView>(model_->GetWeakPtr(),
                                                         bubble_controller)),
      row_list_view_(row_list_view),
      bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler) {
  model_->AddObserver(this);
  set_context_menu_controller(this);

  views::FlexLayout& layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kVertical)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout.SetChildViewIgnoredByLayout(image(), true);
  layout.SetChildViewIgnoredByLayout(label(), true);
  layout.SetChildViewIgnoredByLayout(ink_drop_container(), true);

  SetBorder(views::CreateEmptyBorder(gfx::Insets(0)));

  main_row_ = AddChildView(std::make_unique<views::View>());
  main_row_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  main_row_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  icon_ = main_row_->AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(views::kMarginsKey, GetLayoutInsets(DOWNLOAD_ICON));

  // Make sure the icon is above the inkdrops.
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);

  auto* label_wrapper = main_row_->AddChildView(CreateLabelWrapper());
  primary_label_ = label_wrapper->AddChildView(std::make_unique<views::Label>(
      model_->GetFileNameToReportUser().LossyDisplayName(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  primary_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));

  secondary_label_ = label_wrapper->AddChildView(std::make_unique<views::Label>(
      model_->GetStatusText(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  secondary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  secondary_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithWeight(1));
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  cancel_button_ =
      AddMainPageButton(DownloadCommands::CANCEL,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CANCEL));
  discard_button_ =
      AddMainPageButton(DownloadCommands::DISCARD,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE));
  scan_button_ =
      AddMainPageButton(DownloadCommands::DEEP_SCAN,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_SCAN));
  open_now_button_ = AddMainPageButton(
      DownloadCommands::BYPASS_DEEP_SCANNING,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_NOW));

  subpage_icon_ = main_row_->AddChildView(std::make_unique<views::ImageView>());
  subpage_icon_->SetProperty(views::kMarginsKey,
                             gfx::Insets(kDownloadSubpageIconMargin));
  subpage_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowIcon, ui::kColorIcon));

  // Set up initial state.
  mode_ = download::GetDesiredDownloadItemMode(model_.get());
  state_ = model_->GetState();
  ui_info_ = model_->GetBubbleUIInfo();
  OnDownloadUpdated();
}

gfx::Size DownloadBubbleRowView::CalculatePreferredSize() const {
  return Button::CalculatePreferredSize();
}

int DownloadBubbleRowView::GetHeightForWidth(int w) const {
  return Button::GetHeightForWidth(w);
}

void DownloadBubbleRowView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  return Button::OnBoundsChanged(previous_bounds);
}

views::View* DownloadBubbleRowView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return Button::GetTooltipHandlerForPoint(point);
}

void DownloadBubbleRowView::OnMainButtonPressed() {
  if (ui_info_.has_subpage) {
    SetEnabled(false);
    model_->RemoveObserver(this);
    navigation_handler_->OpenSecurityDialog(std::move(model_), ui_info_);
    // |this| is deleted now.
  } else {
    DownloadCommands(model_->GetWeakPtr())
        .ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE);
  }
}

void DownloadBubbleRowView::UpdateButtonsForItems() {
  cancel_button_->SetVisible(ui_info_.primary_button_command ==
                             DownloadCommands::CANCEL);
  discard_button_->SetVisible(ui_info_.primary_button_command ==
                              DownloadCommands::DISCARD);
  scan_button_->SetVisible(ui_info_.primary_button_command ==
                           DownloadCommands::DEEP_SCAN);
  open_now_button_->SetVisible(ui_info_.primary_button_command ==
                               DownloadCommands::BYPASS_DEEP_SCANNING);

  subpage_icon_->SetVisible(ui_info_.has_subpage);
}

void DownloadBubbleRowView::UpdateProgressBar() {
  // Cannot simply set progress_bar's visibility because the bubble needs to be
  // resized.
  if (ui_info_.has_progress_bar) {
    if (!progress_bar_) {
      progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
      progress_bar_->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kScaleToMaximum,
                                   /*adjust_height_for_width=*/true)
              .WithWeight(1));
      progress_bar_->SetProperty(
          views::kMarginsKey,
          gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    }
    if (ui_info_.is_progress_bar_looping) {
      progress_bar_->SetValue(-1);
    } else {
      progress_bar_->SetValue(static_cast<double>(model_->PercentComplete()) /
                              100);
    }
    return;
  }

  if (progress_bar_) {
    RemoveChildViewT(progress_bar_);
    progress_bar_ = nullptr;
    navigation_handler_->ResizeDialog();
  }
}

void DownloadBubbleRowView::UpdateLabels() {
  primary_label_->SetText(model_->GetFileNameToReportUser().LossyDisplayName());
  secondary_label_->SetText(model_->GetStatusText());

  views::Button::SetAccessibleName(base::JoinString(
      {primary_label_->GetText(), secondary_label_->GetText()}, u"\n"));

  if (GetWidget()) {
    secondary_label_->SetEnabledColor(
        GetColorProvider()->GetColor(ui_info_.secondary_color));
  }
}

void DownloadBubbleRowView::OnDownloadUpdated() {
  UpdateBubbleUIInfo();
  UpdateLabels();
  LoadIcon();
  UpdateButtonsForItems();
  UpdateProgressBar();
}

void DownloadBubbleRowView::OnDownloadOpened() {
  bubble_controller_->RemoveContentIdFromPartialView(model_->GetContentId());
}

void DownloadBubbleRowView::OnDownloadDestroyed() {
  // This will return ownership and destroy this object at the end of the
  // method.
  auto row_view_ptr = row_list_view_->RemoveChildViewT(this);
  if (row_list_view_->children().empty()) {
    navigation_handler_->CloseDialog(views::Widget::ClosedReason::kUnspecified);
  } else {
    navigation_handler_->ResizeDialog();
  }
}

raw_ptr<views::MdTextButton> DownloadBubbleRowView::AddMainPageButton(
    DownloadCommands::Command command,
    const std::u16string& button_string) {
  // base::Unretained is fine as DownloadBubbleRowView owns the discard button
  // and the model, and has an ownership ancestry in
  // DownloadToolbarButtonView, which also owns bubble_controller. So, if the
  // discard button is alive, so should be its parents and their owned fields.
  raw_ptr<views::MdTextButton> button =
      main_row_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &DownloadBubbleUIController::ProcessDownloadButtonPress,
              base::Unretained(bubble_controller_),
              base::Unretained(model_.get()), command),
          button_string));
  button->SetMaxSize(gfx::Size(0, kDownloadButtonHeight));
  return button;
}

void DownloadBubbleRowView::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Similar hack as in MenuButtonController and DownloadItemView.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to null.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())
      ->SetMouseAndGestureHandler(nullptr);

  context_menu_->Run(GetWidget()->GetTopLevelWidget(),
                     gfx::Rect(point, gfx::Size()), source_type,
                     base::RepeatingClosure());
}

BEGIN_METADATA(DownloadBubbleRowView, HoverButton)
END_METADATA
