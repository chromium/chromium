// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace {
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
// Num of columns in the table layout, the width of which progress bar will
// span. The 6 columns are Download Icon, Padding, Status text, Padding,
// Main Button, Subpage Icon.
constexpr int kNumColumns = 6;

// A stub subclass of HoverButton that has no visuals.
class TransparentButton : public HoverButton {
 public:
  METADATA_HEADER(TransparentButton);

  explicit TransparentButton(PressedCallback callback,
                             const std::u16string& text,
                             DownloadBubbleRowView* row_view)
      : HoverButton(callback, text), row_view_(row_view) {}
  ~TransparentButton() override = default;

  // Forward dragging and capture loss events, since this class doesn't have
  // enough context to handle them. Let the `DownloadBubbleRowView` manage
  // visual transitions.
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    HoverButton::OnMouseDragged(event);
    return parent()->OnMouseDragged(event);
  }

  void OnMouseCaptureLost() override {
    parent()->OnMouseCaptureLost();
    HoverButton::OnMouseCaptureLost();
  }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    if (reverse) {
      row_view_->UpdateQuickActionsVisibilityAndFocus(
          /*visible=*/true, /*request_focus_on_last=*/true);
    }
  }

 private:
  raw_ptr<DownloadBubbleRowView> row_view_;
};

BEGIN_METADATA(TransparentButton, HoverButton)
END_METADATA
}  // namespace

bool DownloadBubbleRowView::UpdateBubbleUIInfo(bool initial_setup) {
  auto mode = download::GetDesiredDownloadItemMode(model_.get());
  auto state = model_->GetState();
  bool is_paused = model_->IsPaused();
  if (!initial_setup && (mode_ == mode) && (state_ == state) &&
      (is_paused_ == is_paused)) {
    return false;
  }
  mode_ = mode;
  state_ = state;
  is_paused_ = is_paused;

  // If either of mode or state changes, or if it is the initial setup,
  // we might need to change UI.
  ui_info_ = model_->GetBubbleUIInfo(
      download::IsDownloadBubbleV2Enabled(browser_->profile()));
  return true;
}

void DownloadBubbleRowView::UpdateRow(bool initial_setup) {
  bool ui_info_changed = UpdateBubbleUIInfo(initial_setup);
  if (ui_info_changed) {
    RecordMetricsOnUpdate();
    LoadIcon();
    UpdateButtons();
  }
  RecordDownloadDisplayed();
  UpdateLabels();
  UpdateProgressBar();
}

void DownloadBubbleRowView::AddedToWidget() {
  const display::Screen* const screen = display::Screen::GetScreen();
  current_scale_ = screen->GetDisplayNearestView(GetWidget()->GetNativeView())
                       .device_scale_factor();
  LoadIcon();
  auto* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->AddFocusChangeListener(this);
  }
}

void DownloadBubbleRowView::RemovedFromWidget() {
  auto* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->RemoveFocusChangeListener(this);
}

void DownloadBubbleRowView::OnThemeChanged() {
  views::View::OnThemeChanged();
  secondary_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui_info_.secondary_color));
}

void DownloadBubbleRowView::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  current_scale_ = new_device_scale_factor;
  LoadIcon();
}

void DownloadBubbleRowView::SetIconFromImageModel(bool use_over_last_override,
                                                  ui::ImageModel icon) {
  if (last_overriden_icon_ && !use_over_last_override)
    return;
  if (icon.IsEmpty()) {
    icon_->SetImage(GetDefaultIcon());
  } else {
    icon_->SetImage(icon);
  }
}

void DownloadBubbleRowView::SetIconFromImage(bool use_over_last_override,
                                             gfx::Image icon) {
  SetIconFromImageModel(use_over_last_override,
                        ui::ImageModel::FromImage(icon));
}

void DownloadBubbleRowView::LoadIcon() {
  // The correct scale_factor is set only in the AddedToWidget()
  if (!GetWidget())
    return;

  if (ui_info_.icon_model_override) {
    if (last_overriden_icon_ == ui_info_.icon_model_override)
      return;
    last_overriden_icon_ = ui_info_.icon_model_override;
    SetIconFromImageModel(
        /*use_over_last_override=*/true,
        ui::ImageModel::FromVectorIcon(*ui_info_.icon_model_override,
                                       ui_info_.secondary_color,
                                       GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    return;
  }

  if (bubble_controller_->ShouldShowIncognitoIcon(model_.get())) {
    if (last_overriden_icon_ == &kIncognitoIcon)
      return;
    last_overriden_icon_ = &kIncognitoIcon;
    SetIconFromImageModel(
        /*use_over_last_override=*/true,
        ui::ImageModel::FromVectorIcon(kIncognitoIcon, ui::kColorIcon,
                                       GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    return;
  }

  last_overriden_icon_ = nullptr;

  base::FilePath file_path = model_->GetTargetFilePath();
  // Use a default icon (drive file outline icon) in case we have an empty
  // target path, which is empty for non download offline items, and newly
  // started in-progress downloads.
  if (file_path.empty()) {
    if (already_set_default_icon_)
      return;
    already_set_default_icon_ = true;
    SetIconFromImageModel(/*use_over_last_override=*/true, GetDefaultIcon());
    return;
  }

  IconManager* const im = g_browser_process->icon_manager();
  const gfx::Image* const file_icon_image =
      im->LookupIconFromFilepath(file_path, IconLoader::SMALL, current_scale_);

  if (file_icon_image) {
    SetIconFromImage(/*use_over_last_override=*/true, *file_icon_image);
  } else {
    im->LoadIcon(file_path, IconLoader::SMALL, current_scale_,
                 base::BindOnce(&DownloadBubbleRowView::SetIconFromImage,
                                weak_factory_.GetWeakPtr(),
                                /*use_over_last_override=*/false),
                 &cancelable_task_tracker_);
  }
}

DownloadBubbleRowView::~DownloadBubbleRowView() = default;

DownloadBubbleRowView::DownloadBubbleRowView(
    DownloadUIModel::DownloadUIModelPtr model,
    DownloadBubbleRowListView* row_list_view,
    DownloadBubbleUIController* bubble_controller,
    DownloadBubbleNavigationHandler* navigation_handler,
    Browser* browser)
    : model_(std::move(model)),
      context_menu_(
          std::make_unique<DownloadShelfContextMenuView>(model_->GetWeakPtr(),
                                                         bubble_controller)),
      row_list_view_(row_list_view),
      bubble_controller_(bubble_controller),
      navigation_handler_(navigation_handler),
      browser_(browser) {
  model_->SetDelegate(this);
  SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ROW)));

  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  // Download Icon
  layout->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kStart,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  // Download name/status labels
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 1.0f, views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing);
  // Download Buttons: Cancel, Discard, Scan, Open Now, only one may be active
  layout->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kStart,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  // Subpage icon
  layout->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kStart,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  // Three rows, one for name, one for status, and one for the progress bar.
  layout->AddRows(3, 1.0f);

  hover_button_ = AddChildView(std::make_unique<TransparentButton>(
      base::BindRepeating(&DownloadBubbleRowView::OnMainButtonPressed,
                          base::Unretained(this)),
      std::u16string(), this));
  hover_button_->set_context_menu_controller(this);
  hover_button_->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON);
  layout->SetChildViewIgnoredByLayout(hover_button_, true);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetCanProcessEventsWithinSubtree(false);
  icon_->SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ICON)));
  // Make sure the icon is above the inkdrops.
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);

  primary_label_ = AddChildView(std::make_unique<views::Label>(
      model_->GetFileNameToReportUser().LossyDisplayName(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  primary_label_->SetCanProcessEventsWithinSubtree(false);

  main_button_holder_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  cancel_button_ =
      AddMainPageButton(DownloadCommands::CANCEL,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CANCEL));
  discard_button_ =
      AddMainPageButton(DownloadCommands::DISCARD,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE));
  keep_button_ = AddMainPageButton(
      DownloadCommands::KEEP, l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD));
  scan_button_ =
      AddMainPageButton(DownloadCommands::DEEP_SCAN,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_SCAN));
  open_now_button_ = AddMainPageButton(
      DownloadCommands::BYPASS_DEEP_SCANNING,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_NOW));
  resume_button_ =
      AddMainPageButton(DownloadCommands::RESUME,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RESUME));
  review_button_ =
      AddMainPageButton(DownloadCommands::REVIEW,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_REVIEW));
  retry_button_ =
      AddMainPageButton(DownloadCommands::RETRY,
                        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RETRY));

  // Note that the addition order of these quick actions matches the visible
  // order, i.e. buttons added first will appear first (left in LTR)
  quick_action_holder_ = main_button_holder_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  resume_action_ = AddQuickAction(DownloadCommands::RESUME);
  pause_action_ = AddQuickAction(DownloadCommands::PAUSE);
  open_when_complete_action_ =
      AddQuickAction(DownloadCommands::OPEN_WHEN_COMPLETE);
  cancel_action_ = AddQuickAction(DownloadCommands::CANCEL);
  show_in_folder_action_ = AddQuickAction(DownloadCommands::SHOW_IN_FOLDER);
  quick_action_holder_->SetVisible(false);

  subpage_icon_holder_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  subpage_icon_holder_->SetCanProcessEventsWithinSubtree(false);
  subpage_icon_ =
      subpage_icon_holder_->AddChildView(std::make_unique<views::ImageView>());
  subpage_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowIcon, ui::kColorIcon));
  subpage_icon_->SetVisible(false);

  // Empty cell under icon_
  AddChildView(std::make_unique<views::FlexLayoutView>());

  secondary_label_ = AddChildView(std::make_unique<views::Label>(
      model_->GetStatusText(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  secondary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // The 4 columns are filename text, Padding, Main Button, Subpage Icon.
  secondary_label_->SetProperty(views::kTableColAndRowSpanKey, gfx::Size(4, 1));
  secondary_label_->SetCanProcessEventsWithinSubtree(false);

  // TODO(bhatiarohit): Remove the progress bar holder view here.
  // Currently the animation does not show up on deep scanning without
  // the holder.
  progress_bar_holder_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  progress_bar_holder_->SetCanProcessEventsWithinSubtree(false);
  progress_bar_holder_->SetProperty(views::kTableColAndRowSpanKey,
                                    gfx::Size(kNumColumns, 1));
  progress_bar_holder_->SetProperty(views::kTableHorizAlignKey,
                                    views::LayoutAlignment::kStretch);
  progress_bar_ = progress_bar_holder_->AddChildView(
      std::make_unique<views::ProgressBar>());
  progress_bar_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL),
                        0, 0, 0)));
  progress_bar_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false));
  // Expect to start not visible, will be updated later.
  progress_bar_->SetVisible(false);

  // Set up initial state.
  UpdateRow(/*initial_setup=*/true);
}

views::View::Views DownloadBubbleRowView::GetChildrenInZOrder() {
  auto children = views::View::GetChildrenInZOrder();
  const auto move_child_to_top = [&](View* child) {
    auto it = base::ranges::find(children, child);
    DCHECK(it != children.end());
    std::rotate(it, it + 1, children.end());
  };
  move_child_to_top(hover_button_);
  move_child_to_top(main_button_holder_);
  return children;
}

bool DownloadBubbleRowView::OnMouseDragged(const ui::MouseEvent& event) {
  // Handle drag (file copy) operations.
  // Drag and drop should only be activated in normal mode.
  if (mode_ != download::DownloadItemMode::kNormal)
    return true;

  if (!drag_start_point_)
    drag_start_point_ = event.location();
  if (!dragging_) {
    dragging_ = ExceededDragThreshold(event.location() - *drag_start_point_);
  } else if ((model_->GetState() == download::DownloadItem::COMPLETE) &&
             model_->GetDownloadItem()) {
    const gfx::Image* const file_icon =
        g_browser_process->icon_manager()->LookupIconFromFilepath(
            model_->GetTargetFilePath(), IconLoader::SMALL, current_scale_);
    const views::Widget* const widget = GetWidget();
    DragDownloadItem(model_->GetDownloadItem(), file_icon,
                     widget ? widget->GetNativeView() : nullptr);
    RecordDownloadBubbleDragInfo(DownloadDragInfo::DRAG_STARTED);
  }
  return true;
}

void DownloadBubbleRowView::OnMouseCaptureLost() {
  // Drag and drop should only be activated in normal mode.
  if (mode_ != download::DownloadItemMode::kNormal)
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    drag_start_point_.reset();
  }
}

void DownloadBubbleRowView::OnWillChangeFocus(views::View* before,
                                              views::View* now) {
  if (now) {
    UpdateQuickActionsVisibilityAndFocus(/*visible=*/Contains(now),
                                         /*request_focus_on_last=*/false);
  }
}

void DownloadBubbleRowView::UpdateQuickActionsVisibilityAndFocus(
    bool visible,
    bool request_focus_on_last) {
  quick_action_holder_->SetVisible(visible);
  // Update focus only if focus received from a different row.
  bool should_set_focus = request_focus_on_last && GetFocusManager() &&
                          !Contains(GetFocusManager()->GetFocusedView());
  if (should_set_focus && ui_info_.quick_actions.size() != 0) {
    GetActionButtonForCommand(ui_info_.quick_actions.back().command)
        ->RequestFocus();
  }
}

void DownloadBubbleRowView::Layout() {
  views::View::Layout();
  hover_button_->SetBoundsRect(GetLocalBounds());
}

void DownloadBubbleRowView::OnMainButtonPressed() {
  if (ui_info_.has_subpage) {
    navigation_handler_->OpenSecurityDialog(this);
  } else {
    DownloadCommands(model_->GetWeakPtr())
        .ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE);
  }
}

void DownloadBubbleRowView::UpdateButtons() {
  resume_action_->SetVisible(false);
  pause_action_->SetVisible(false);
  open_when_complete_action_->SetVisible(false);
  cancel_action_->SetVisible(false);
  show_in_folder_action_->SetVisible(false);
  open_when_complete_action_->SetVisible(false);
  for (const auto& action : ui_info_.quick_actions) {
    views::ImageButton* action_button =
        GetActionButtonForCommand(action.command);
    action_button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*(action.icon), ui::kColorIcon,
                                       GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    action_button->SetAccessibleName(action.hover_text);
    action_button->SetTooltipText(action.hover_text);
    action_button->SetVisible(true);
  }

  cancel_button_->SetVisible(ui_info_.primary_button_command ==
                             DownloadCommands::CANCEL);
  discard_button_->SetVisible(ui_info_.primary_button_command ==
                              DownloadCommands::DISCARD);
  keep_button_->SetVisible(ui_info_.primary_button_command ==
                           DownloadCommands::KEEP);
  scan_button_->SetVisible(ui_info_.primary_button_command ==
                           DownloadCommands::DEEP_SCAN);
  open_now_button_->SetVisible(ui_info_.primary_button_command ==
                               DownloadCommands::BYPASS_DEEP_SCANNING);
  resume_button_->SetVisible(ui_info_.primary_button_command ==
                             DownloadCommands::RESUME);
  review_button_->SetVisible(ui_info_.primary_button_command ==
                             DownloadCommands::REVIEW);
  retry_button_->SetVisible(ui_info_.primary_button_command ==
                            DownloadCommands::RETRY);

  subpage_icon_->SetVisible(ui_info_.has_subpage);
  subpage_icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(ui_info_.has_subpage ? kDownloadSubpageIconMargin : 0)));
}

void DownloadBubbleRowView::UpdateProgressBar() {
  if (ui_info_.has_progress_bar) {
    if (!progress_bar_->GetVisible()) {
      progress_bar_->SetVisible(true);
      // Need for a few cases, for example if the view is the only one in a
      // partial view.
      navigation_handler_->ResizeDialog();
    }
    progress_bar_->SetValue(
        ui_info_.is_progress_bar_looping
            ? -1
            : static_cast<double>(model_->PercentComplete()) / 100);
    progress_bar_->SetPaused(model_->IsPaused());
  } else if (progress_bar_->GetVisible()) {
    // Hide the progress bar.
    progress_bar_->SetVisible(false);
    navigation_handler_->ResizeDialog();
  }
}

void DownloadBubbleRowView::UpdateLabels() {
  primary_label_->SetText(model_->GetFileNameToReportUser().LossyDisplayName());
  secondary_label_->SetText(model_->GetStatusText());

  hover_button_->SetAccessibleName(base::JoinString(
      {primary_label_->GetText(), secondary_label_->GetText()}, u" "));
  // TODO(crbug.com/1326181): Below is a workaround for single line labels.
  // Remove the tooltip text once `primary_label_` and `secondary_label_` can
  // display multiline text.
  hover_button_->SetTooltipText(base::JoinString(
      {primary_label_->GetText(), secondary_label_->GetText()}, u"\n"));

  if (GetWidget()) {
    secondary_label_->SetEnabledColor(
        GetColorProvider()->GetColor(ui_info_.secondary_color));
  }
}

void DownloadBubbleRowView::RecordMetricsOnUpdate() {
  // This should only be logged once per download.
  if (is_download_warning(download::GetDesiredDownloadItemMode(model_.get())) &&
      !model_->WasUIWarningShown()) {
    model_->SetWasUIWarningShown(true);
    RecordDangerousDownloadWarningShown(
        model_->GetDangerType(), model_->GetTargetFilePath(),
        model_->GetURL().SchemeIs(url::kHttpsScheme), model_->HasUserGesture());
  }
  if (!has_download_completion_been_logged_ &&
      model_->GetState() == download::DownloadItem::COMPLETE) {
    RecordDownloadBubbleDragInfo(DownloadDragInfo::DOWNLOAD_COMPLETE);
    has_download_completion_been_logged_ = true;
  }
}

void DownloadBubbleRowView::RecordDownloadDisplayed() {
  if (!model_->GetEphemeralWarningUiShownTime().has_value() &&
      model_->IsEphemeralWarning()) {
    model_->SetEphemeralWarningUiShownTime(base::Time::Now());
    bubble_controller_->ScheduleCancelForEphemeralWarning(
        model_->GetDownloadItem()->GetGuid());
  }
}

void DownloadBubbleRowView::OnDownloadUpdated() {
  UpdateRow(/*initial_setup=*/false);
}

void DownloadBubbleRowView::OnDownloadOpened() {
  bubble_controller_->RemoveContentIdFromPartialView(model_->GetContentId());
}

void DownloadBubbleRowView::OnDownloadDestroyed(const ContentId& id) {
  // This will return ownership and destroy this object at the end of the
  // method.
  std::unique_ptr<DownloadBubbleRowView> row_view_ptr =
      row_list_view_->RemoveChildViewT(this);
  if (row_list_view_->children().empty()) {
    navigation_handler_->CloseDialog(views::Widget::ClosedReason::kUnspecified);
  } else {
    navigation_handler_->ResizeDialog();
  }
}

views::MdTextButton* DownloadBubbleRowView::AddMainPageButton(
    DownloadCommands::Command command,
    const std::u16string& button_string) {
  // base::Unretained is fine as DownloadBubbleRowView owns the discard button
  // and the model, and has an ownership ancestry in
  // DownloadToolbarButtonView, which also owns bubble_controller. So, if the
  // discard button is alive, so should be its parents and their owned fields.
  views::MdTextButton* button =
      main_button_holder_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &DownloadBubbleUIController::ProcessDownloadButtonPress,
              base::Unretained(bubble_controller_),
              base::Unretained(model_.get()), command),
          button_string));
  button->SetMaxSize(gfx::Size(0, kDownloadButtonHeight));
  button->SetVisible(false);
  return button;
}

views::ImageButton* DownloadBubbleRowView::AddQuickAction(
    DownloadCommands::Command command) {
  views::ImageButton* quick_action = quick_action_holder_->AddChildView(
      views::CreateVectorImageButton(base::BindRepeating(
          &DownloadBubbleUIController::ProcessDownloadButtonPress,
          base::Unretained(bubble_controller_), base::Unretained(model_.get()),
          command)));
  InstallCircleHighlightPathGenerator(quick_action);
  quick_action->SetBorder(
      views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ICON)));
  quick_action->SetVisible(false);
  return quick_action;
}

views::ImageButton* DownloadBubbleRowView::GetActionButtonForCommand(
    DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::RESUME:
      return resume_action_;
    case DownloadCommands::PAUSE:
      return pause_action_;
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      return open_when_complete_action_;
    case DownloadCommands::CANCEL:
      return cancel_action_;
    case DownloadCommands::SHOW_IN_FOLDER:
      return show_in_folder_action_;
    default:
      return nullptr;
  }
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

BEGIN_METADATA(DownloadBubbleRowView, views::View)
END_METADATA
