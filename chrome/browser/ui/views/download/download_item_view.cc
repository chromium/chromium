// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_item_view.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/safe_browsing/deep_scanning_modal_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using download::DownloadItem;

namespace {

// The vertical distance between the item's visual upper bound (as delineated
// by the separator on the right) and the edge of the shelf.
constexpr int kTopBottomPadding = 6;

// The minimum vertical padding above and below contents of the download item.
// This is only used when the text size is large.
constexpr int kMinimumVerticalPadding = 2 + kTopBottomPadding;

// The normal height of the item which may be exceeded if text is large.
constexpr int kDefaultDownloadItemHeight = 48;

// Amount of time between accessible alert events.
constexpr auto kAccessibleAlertInterval = base::TimeDelta::FromSeconds(30);

// The size of the file icon.
constexpr int kFileIconSize = 24;

// The offset from the file icon to the danger icon.
constexpr int kDangerIconOffset = 8;

// The separator is drawn as a border. It's one dp wide.
class SeparatorBorder : public views::Border {
 public:
  explicit SeparatorBorder(SkColor separator_color)
      : separator_color_(separator_color) {}
  ~SeparatorBorder() override {}

  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    // The FocusRing replaces the separator border when we have focus.
    if (view.HasFocus())
      return;
    int end_x = base::i18n::IsRTL() ? 0 : view.width() - 1;
    canvas->DrawLine(gfx::Point(end_x, kTopBottomPadding),
                     gfx::Point(end_x, view.height() - kTopBottomPadding),
                     separator_color_);
  }

  gfx::Insets GetInsets() const override { return gfx::Insets(0, 0, 0, 1); }

  gfx::Size GetMinimumSize() const override {
    return gfx::Size(1, 2 * kTopBottomPadding + 1);
  }

 private:
  SkColor separator_color_;

  DISALLOW_COPY_AND_ASSIGN(SeparatorBorder);
};

base::string16 SplitStringWithNewLineAtPosition(const base::string16& text,
                                                size_t pos) {
  base::string16 new_text = text;
  // This can be a low surrogate codepoint, but u_isUWhiteSpace will
  // return false and inserting a new line after a surrogate pair
  // is perfectly ok.
  base::char16 line_end_char = text[pos - 1];
  if (u_isUWhiteSpace(line_end_char))
    new_text.replace(pos - 1, 1, 1, base::char16('\n'));
  else
    new_text.insert(pos, 1, base::char16('\n'));
  return new_text;
}

// A stub subclass of Button that has no visuals.
class TransparentButton : public views::Button {
 public:
  explicit TransparentButton(views::ButtonListener* listener)
      : Button(listener) {
    SetFocusForPlatform();
    views::InstallRectHighlightPathGenerator(this);
    SetInkDropMode(InkDropMode::ON);
  }
  ~TransparentButton() override {}

  // Button subclasses need to provide this because the default color is
  // kPlaceholderColor. In theory we could statically compute it in the
  // constructor but then it won't be correct after dark mode changes, and to
  // deal with that this class would have to observe NativeTheme and so on.
  SkColor GetInkDropBaseColor() const override {
    return color_utils::DeriveDefaultIconColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ButtonEnabledColor));
  }

  const char* GetClassName() const override { return "TransparentButton"; }

  // Forward dragging events, since this class doesn't have enough context to
  // handle them.
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    return parent()->OnMouseDragged(event);
  }

  void OnMouseCaptureLost() override { parent()->OnMouseCaptureLost(); }
};

}  // namespace

DownloadItemView::DownloadItemView(DownloadUIModel::DownloadUIModelPtr download,
                                   DownloadShelfView* parent,
                                   views::View* accessible_alert)
    : AnimationDelegateViews(this),
      shelf_(parent),
      dropdown_state_(NORMAL),
      mode_(NORMAL_MODE),
      dragging_(false),
      starting_drag_(false),
      model_(std::move(download)),
      save_button_(nullptr),
      discard_button_(nullptr),
      dangerous_download_label_(nullptr),
      dangerous_download_label_sized_(false),
      creation_time_(base::Time::Now()),
      time_download_warning_shown_(base::Time()),
      accessible_alert_(accessible_alert),
      announce_accessible_alert_soon_(false),
      deep_scanning_label_(nullptr),
      open_now_button_(nullptr) {
  views::InstallRectHighlightPathGenerator(this);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  model_->AddObserver(this);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  font_list_ = rb.GetFontListWithDelta(1);
  status_font_list_ = rb.GetFontListWithDelta(-2);

  auto open_button = std::make_unique<TransparentButton>(this);
  open_button->set_context_menu_controller(this);
  open_button_ = AddChildView(std::move(open_button));

  auto file_name_label = std::make_unique<views::Label>();
  file_name_label->SetFontList(font_list_);
  file_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  file_name_label->SetText(ElidedFilename());
  file_name_label->GetViewAccessibility().OverrideIsIgnored(true);
  file_name_label_ = AddChildView(std::move(file_name_label));

  auto status_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING));
  status_label->SetFontList(status_font_list_);
  status_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  status_label->GetViewAccessibility().OverrideIsIgnored(true);
  status_label_ = AddChildView(std::move(status_label));

  auto dropdown_button = views::CreateVectorImageButton(this);
  dropdown_button->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_DOWNLOAD_ITEM_DROPDOWN_BUTTON_ACCESSIBLE_TEXT));

  dropdown_button->SetBorder(views::CreateEmptyBorder(gfx::Insets(10)));
  dropdown_button->set_has_ink_drop_action_on_click(false);
  dropdown_button->SetFocusForPlatform();
  dropdown_button_ = AddChildView(std::move(dropdown_button));

  LoadIcon();

  OnDownloadUpdated();

  SetDropdownState(NORMAL);
  UpdateColorsFromTheme();
}

DownloadItemView::~DownloadItemView() {
  StopDownloadProgress();
  model_->RemoveObserver(this);
}

// Progress animation handlers.

void DownloadItemView::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_start_time_ = base::TimeTicks::Now();
  progress_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(30),
                        base::Bind(&DownloadItemView::ProgressTimerFired,
                                   base::Unretained(this)));
}

void DownloadItemView::StopDownloadProgress() {
  accessible_alert_timer_.AbandonAndStop();
  if (!progress_timer_.IsRunning())
    return;
  previous_progress_elapsed_ += base::TimeTicks::Now() - progress_start_time_;
  progress_start_time_ = base::TimeTicks();
  progress_timer_.Stop();
}

// static
SkColor DownloadItemView::GetTextColorForThemeProvider(
    const ui::ThemeProvider* theme) {
  return theme ? theme->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)
               : gfx::kPlaceholderColor;
}

void DownloadItemView::OnExtractIconComplete(IconLoader::IconSize icon_size,
                                             gfx::Image icon_bitmap) {
  if (!icon_bitmap.IsEmpty()) {
    if (icon_size == IconLoader::IconSize::NORMAL) {
      // We want a 24x24 icon, but on Windows only 16x16 and 32x32 are
      // available. So take the NORMAL icon and downsize it.
      icon_ = gfx::ImageSkiaOperations::CreateResizedImage(
          *icon_bitmap.ToImageSkia(), skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kFileIconSize, kFileIconSize));
    }
    shelf_->SchedulePaint();
  }
}

void DownloadItemView::MaybeSubmitDownloadToFeedbackService(
    DownloadCommands::Command download_command) {
  if (model_->ShouldAllowDownloadFeedback() &&
      SubmitDownloadToFeedbackService(download_command)) {
  } else {
    DownloadCommands(model_.get()).ExecuteCommand(download_command);
  }
}

// DownloadObserver interface.

// Update the progress graphic on the icon and our text status label
// to reflect our current bytes downloaded, time remaining.
// Also updates the accessible status view for screen reader users.
void DownloadItemView::OnDownloadUpdated() {
  if (!model_->ShouldShowInShelf()) {
    shelf_->RemoveDownloadView(this);  // This will delete us!
    return;
  }

  bool is_danger_type_async_scanning =
      (model_->GetDangerType() ==
       download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING);
  if (model_->IsDangerous()) {
    if (!IsShowingWarningDialog())
      TransitionToWarningDialog();
  } else if (is_danger_type_async_scanning &&
             model_->GetState() != DownloadItem::CANCELLED) {
    if (!IsShowingDeepScanning())
      TransitionToDeepScanningDialog();

    if (should_open_while_scanning_ &&
        model_->GetState() == DownloadItem::COMPLETE) {
      should_open_while_scanning_ = false;
      model_->OpenDownload();
    }
  } else {
    TransitionToNormalMode();
  }

  if (model_->GetState() == DownloadItem::COMPLETE &&
      model_->ShouldRemoveFromShelfWhenComplete()) {
    shelf_->RemoveDownloadView(this);  // This will delete us!
    return;
  }

  base::string16 new_tip = model_->GetTooltipText(font_list_, kTooltipMaxWidth);
  if (new_tip != tooltip_text_) {
    tooltip_text_ = new_tip;
    TooltipTextChanged();
  }

  UpdateAccessibleName();
}

void DownloadItemView::TransitionToNormalMode() {
  if (IsShowingDeepScanning())
    ClearDeepScanningDialog();
  if (IsShowingWarningDialog())
    ClearWarningDialog();

  status_label_->SetText(GetStatusText());
  status_label_->GetViewAccessibility().OverrideIsIgnored(
      status_label_->GetText().empty());
  AdjustTextAndGetSize(status_label_);
  file_name_label_->SetY(GetYForFilenameText());
  switch (model_->GetState()) {
    case DownloadItem::IN_PROGRESS:
      // No need to send accessible alert for "paused", as the button ends
      // up being refocused in the actual use case, and the name of the
      // button reports that the download has been paused.
      // Reset the status counter so that user receives immediate feedback
      // once the download is resumed.
      if (!model_->IsPaused())
        UpdateAccessibleAlert(GetInProgressAccessibleAlertText(), false);
      model_->IsPaused() ? StopDownloadProgress() : StartDownloadProgress();
      LoadIconIfItemPathChanged();
      break;
    case DownloadItem::INTERRUPTED:
      model_->GetFileNameToReportUser().LossyDisplayName();
      UpdateAccessibleAlert(
          l10n_util::GetStringFUTF16(
              IDS_DOWNLOAD_FAILED_ACCESSIBLE_ALERT,
              model_->GetFileNameToReportUser().LossyDisplayName()),
          true);
      StopDownloadProgress();
      complete_animation_ = std::make_unique<gfx::SlideAnimation>(this);
      complete_animation_->SetSlideDuration(
          base::TimeDelta::FromMilliseconds(2500));
      complete_animation_->SetTweenType(gfx::Tween::LINEAR);
      complete_animation_->Show();
      LoadIcon();
      break;
    case DownloadItem::COMPLETE:
      UpdateAccessibleAlert(
          l10n_util::GetStringFUTF16(
              IDS_DOWNLOAD_COMPLETE_ACCESSIBLE_ALERT,
              model_->GetFileNameToReportUser().LossyDisplayName()),
          true);
      StopDownloadProgress();
      complete_animation_ = std::make_unique<gfx::SlideAnimation>(this);
      complete_animation_->SetSlideDuration(
          base::TimeDelta::FromMilliseconds(2500));
      complete_animation_->SetTweenType(gfx::Tween::LINEAR);
      complete_animation_->Show();
      LoadIcon();
      break;
    case DownloadItem::CANCELLED:
      UpdateAccessibleAlert(
          l10n_util::GetStringFUTF16(
              IDS_DOWNLOAD_CANCELLED_ACCESSIBLE_ALERT,
              model_->GetFileNameToReportUser().LossyDisplayName()),
          true);
      StopDownloadProgress();
      if (complete_animation_)
        complete_animation_->Stop();
      LoadIcon();
      break;
    default:
      NOTREACHED();
  }

  // Force the shelf to layout as our size may have changed.
  shelf_->Layout();
  shelf_->SchedulePaint();
}

void DownloadItemView::OnDownloadDestroyed() {
  shelf_->RemoveDownloadView(this);  // This will delete us!
}

void DownloadItemView::OnDownloadOpened() {
  // First, Calculate the download status opening string width.
  base::string16 status_string =
      l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING, base::string16());
  int status_string_width = gfx::GetStringWidth(status_string, font_list_);
  // Then, elide the file name.
  base::string16 filename_string =
      gfx::ElideFilename(model_->GetFileNameToReportUser(), font_list_,
                         kTextWidth - status_string_width);
  // Last, concat the whole string to be set on the label.
  file_name_label_->SetText(
      l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING, filename_string));

  SetEnabled(false);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DownloadItemView::Reenable,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(3));

  // Notify our parent.
  shelf_->OpenedDownload();
}

// View overrides

// In dangerous mode we have to layout our buttons.
void DownloadItemView::Layout() {
  InkDropHostView::Layout();

  UpdateColorsFromTheme();

  open_button_->SetBoundsRect(GetLocalBounds());

  if (IsShowingWarningDialog()) {
    gfx::Point child_origin(
        kStartPadding + kWarningIconSize + kStartPadding,
        (height() - dangerous_download_label_->height()) / 2);
    dangerous_download_label_->SetPosition(child_origin);

    child_origin.Offset(dangerous_download_label_->width() + kLabelPadding, 0);
    gfx::Size button_size = GetButtonSize();
    child_origin.set_y((height() - button_size.height()) / 2);
    if (save_button_) {
      save_button_->SetBoundsRect(gfx::Rect(child_origin, button_size));
      child_origin.Offset(button_size.width() + kSaveDiscardButtonPadding, 0);
    }
    if (discard_button_)
      discard_button_->SetBoundsRect(gfx::Rect(child_origin, button_size));
  } else if (IsShowingDeepScanning()) {
    gfx::Point child_origin(kStartPadding + kWarningIconSize + kStartPadding,
                            (height() - deep_scanning_label_->height()) / 2);
    deep_scanning_label_->SetPosition(child_origin);

    if (open_now_button_) {
      child_origin.set_y(
          (height() - open_now_button_->GetPreferredSize().height()) / 2);
      child_origin.Offset(deep_scanning_label_->width() + kLabelPadding, 0);
      open_now_button_->SetBoundsRect(
          gfx::Rect(child_origin, open_now_button_->GetPreferredSize()));
    }
  } else {
    int mirrored_x = GetMirroredXWithWidthInView(
        kStartPadding + DownloadShelf::kProgressIndicatorSize +
            kProgressTextPadding,
        kTextWidth);
    int file_name_y = GetYForFilenameText();
    file_name_label_->SetBoundsRect(
        gfx::Rect(mirrored_x, file_name_y, kTextWidth, font_list_.GetHeight()));

    int status_y =
        file_name_y + font_list_.GetBaseline() + kVerticalTextPadding;
    status_label_->SetBoundsRect(gfx::Rect(mirrored_x, status_y, kTextWidth,
                                           status_font_list_.GetHeight()));
  }

  if (mode_ != DANGEROUS_MODE) {
    dropdown_button_->SizeToPreferredSize();
    dropdown_button_->SetPosition(
        gfx::Point(width() - dropdown_button_->width() - kEndPadding,
                   (height() - dropdown_button_->height()) / 2));
  }
}

void DownloadItemView::UpdateDropdownButton() {
  views::SetImageFromVectorIcon(
      dropdown_button_,
      dropdown_state_ == PUSHED ? kCaretDownIcon : kCaretUpIcon,
      GetTextColor());
}

gfx::Size DownloadItemView::CalculatePreferredSize() const {
  int width = 0;
  // We set the height to the height of two rows or text plus margins.
  int child_height = font_list_.GetBaseline() + kVerticalTextPadding +
                     status_font_list_.GetHeight();

  if (IsShowingWarningDialog()) {
    // Width.
    width = kStartPadding + kWarningIconSize + kStartPadding +
            dangerous_download_label_->width() + kLabelPadding;
    gfx::Size button_size = GetButtonSize();
    if (save_button_)
      width += button_size.width() + kSaveDiscardButtonPadding;
    width += button_size.width() + kEndPadding;

    // Height: make sure the button fits and the warning icon fits.
    child_height =
        std::max({child_height, button_size.height(), kWarningIconSize});
  } else if (IsShowingDeepScanning()) {
    width = kStartPadding + kWarningIconSize + kStartPadding +
            deep_scanning_label_->width() + kLabelPadding;
    if (open_now_button_) {
      width += open_now_button_->GetPreferredSize().width();
      // Height: make sure the button fits and the warning icon fits.
      child_height =
          std::max({child_height, open_now_button_->GetPreferredSize().height(),
                    kWarningIconSize});
      width += kEndPadding;
    }

  } else {
    gfx::Size label_size = file_name_label_->GetPreferredSize();
    label_size.SetToMax(status_label_->GetPreferredSize());
    width = kStartPadding + DownloadShelf::kProgressIndicatorSize +
            kProgressTextPadding + kTextWidth + kEndPadding;
  }

  if (mode_ != DANGEROUS_MODE)
    width += dropdown_button_->GetPreferredSize().width();

  return gfx::Size(width, std::max(kDefaultDownloadItemHeight,
                                   2 * kMinimumVerticalPadding + child_height));
}

// Handle drag (file copy) operations.
bool DownloadItemView::OnMouseDragged(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
    return true;

  if (!starting_drag_) {
    starting_drag_ = true;
    drag_start_point_ = event.location();
    AnimateInkDrop(views::InkDropState::HIDDEN, &event);
  }
  if (dragging_) {
    if (model_->GetState() == DownloadItem::COMPLETE) {
      IconManager* im = g_browser_process->icon_manager();
      gfx::Image* icon = im->LookupIconFromFilepath(model_->GetTargetFilePath(),
                                                    IconLoader::SMALL);
      views::Widget* widget = GetWidget();
      if (model_->download()) {
        // TODO(shaktisahu): Make DragDownloadItem work with a model.
        DragDownloadItem(model_->download(), icon,
                         widget ? widget->GetNativeView() : nullptr);
        RecordDownloadShelfDragEvent(DownloadShelfDragEvent::STARTED);
      }
    }
  } else if (ExceededDragThreshold(event.location() - drag_start_point_)) {
    dragging_ = true;
  }
  return true;
}

void DownloadItemView::OnMouseCaptureLost() {
  // Mouse should not activate us in dangerous mode.
  if (mode_ != NORMAL_MODE)
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    starting_drag_ = false;
  }
}

base::string16 DownloadItemView::GetTooltipText(const gfx::Point& p) const {
  return IsShowingWarningDialog() ? base::string16() : tooltip_text_;
}

void DownloadItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
  node_data->role = ax::mojom::Role::kGroup;

  // Set the description to the empty string, otherwise the tooltip will be
  // used, which is redundant with the accessible name.
  node_data->SetDescription(base::string16());
}

void DownloadItemView::AddedToWidget() {
  // Only required because OnThemeChanged is not called when a View is added to
  // a Widget.
  UpdateDropdownButton();
}

void DownloadItemView::OnThemeChanged() {
  UpdateColorsFromTheme();
  SchedulePaint();
  UpdateDropdownButton();
}

void DownloadItemView::OnInkDropCreated() {
  ConfigureInkDrop();
}

void DownloadItemView::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ShowContextMenuImpl(gfx::Rect(point, gfx::Size()), source_type);
}

void DownloadItemView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == open_now_button_) {
    OpenDownloadDuringAsyncScanning();
    return;
  }

  if (IsShowingDeepScanning() && sender == open_button_) {
    content::WebContents* current_web_contents =
        shelf_->browser()->tab_strip_model()->GetActiveWebContents();
    open_now_modal_dialog_ = TabModalConfirmDialog::Create(
        std::make_unique<safe_browsing::DeepScanningModalDialog>(
            current_web_contents,
            base::BindOnce(&DownloadItemView::OpenDownloadDuringAsyncScanning,
                           weak_ptr_factory_.GetWeakPtr())),
        current_web_contents);
    return;
  }

  if (sender == dropdown_button_) {
    // TODO(estade): this is copied from ToolbarActionView but should be shared
    // one way or another.
    ui::MenuSourceType type = ui::MENU_SOURCE_NONE;
    if (event.IsMouseEvent())
      type = ui::MENU_SOURCE_MOUSE;
    else if (event.IsKeyEvent())
      type = ui::MENU_SOURCE_KEYBOARD;
    else if (event.IsGestureEvent())
      type = ui::MENU_SOURCE_TOUCH;
    SetDropdownState(PUSHED);
    ShowContextMenuImpl(dropdown_button_->GetBoundsInScreen(), type);
    return;
  }

  base::TimeDelta warning_duration;
  if (!time_download_warning_shown_.is_null())
    warning_duration = base::Time::Now() - time_download_warning_shown_;

  if (save_button_ && sender == save_button_) {
    // The user has confirmed a dangerous download.  We'd record how quickly the
    // user did this to detect whether we're being clickjacked.
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download", warning_duration);
    // This will call ValidateDangerousDownload(), change download state and
    // notify us.
    MaybeSubmitDownloadToFeedbackService(DownloadCommands::KEEP);
    return;
  }

  if (sender == open_button_) {
    if (IsShowingWarningDialog())
      return;
    if (complete_animation_.get() && complete_animation_->is_animating())
      complete_animation_->End();
    OpenDownload();
    return;
  }

  DCHECK_EQ(discard_button_, sender);
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download", warning_duration);
  MaybeSubmitDownloadToFeedbackService(DownloadCommands::DISCARD);
  // WARNING: 'this' maybe deleted at this point. Don't access 'this'.
}

void DownloadItemView::AnimationProgressed(const gfx::Animation* animation) {
  // We don't care if what animation (body button/drop button/complete),
  // is calling back, as they all have to go through the same paint call.
  SchedulePaint();
}

void DownloadItemView::OnPaint(gfx::Canvas* canvas) {
  // Make sure to draw |this| opaquely. Since the toolbar color can be partially
  // transparent, start with a black backdrop (which is the default initialized
  // color for opaque canvases).
  canvas->DrawColor(SK_ColorBLACK);
  canvas->DrawColor(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_DOWNLOAD_SHELF));

  DrawIcon(canvas);
  OnPaintBorder(canvas);
}

int DownloadItemView::GetYForFilenameText() const {
  int text_height = font_list_.GetBaseline();
  if (!status_label_->GetText().empty())
    text_height += kVerticalTextPadding + status_font_list_.GetBaseline();
  return (height() - text_height) / 2;
}

void DownloadItemView::DrawIcon(gfx::Canvas* canvas) {
  // Paint download progress.
  DownloadItem::DownloadState state = model_->GetState();
  canvas->Save();
  int progress_x =
      base::i18n::IsRTL()
          ? width() - kStartPadding - DownloadShelf::kProgressIndicatorSize
          : kStartPadding;
  int progress_y = (height() - DownloadShelf::kProgressIndicatorSize) / 2;
  canvas->Translate(gfx::Vector2d(progress_x, progress_y));

  const gfx::ImageSkia* current_icon = nullptr;
  IconManager* im = g_browser_process->icon_manager();
  gfx::Image* image_ptr = im->LookupIconFromFilepath(
      model_->GetTargetFilePath(), IconLoader::SMALL);
  if (image_ptr)
    current_icon = image_ptr->ToImageSkia();

  if (state == DownloadItem::IN_PROGRESS && !IsShowingDeepScanning() &&
      !IsShowingWarningDialog()) {
    base::TimeDelta progress_time = previous_progress_elapsed_;
    if (!model_->IsPaused())
      progress_time += base::TimeTicks::Now() - progress_start_time_;
    DownloadShelf::PaintDownloadProgress(
        canvas, *GetThemeProvider(), progress_time, model_->PercentComplete());
  } else if (complete_animation_.get() && complete_animation_->is_animating()) {
    if (state == DownloadItem::INTERRUPTED) {
      DownloadShelf::PaintDownloadInterrupted(
          canvas, *GetThemeProvider(), complete_animation_->GetCurrentValue());
    } else {
      DCHECK_EQ(DownloadItem::COMPLETE, state);
      DownloadShelf::PaintDownloadComplete(
          canvas, *GetThemeProvider(), complete_animation_->GetCurrentValue());
    }
  } else {
    current_icon = &icon_;
  }
  canvas->Restore();

  if (!current_icon)
    return;

  // Draw the icon image.
  int kFiletypeIconOffset =
      (DownloadShelf::kProgressIndicatorSize - current_icon->height()) / 2;
  int icon_x = progress_x + kFiletypeIconOffset;
  int icon_y = progress_y + kFiletypeIconOffset;
  cc::PaintFlags flags;
  // Use an alpha to make the image look disabled.
  if (!GetEnabled())
    flags.setAlpha(120);
  canvas->DrawImageInt(*current_icon, icon_x, icon_y, flags);

  // Overlay the danger icon if appropriate.
  if (IsShowingWarningDialog() || IsShowingDeepScanning()) {
    int icon_x =
        (base::i18n::IsRTL() ? width() - kWarningIconSize - kStartPadding
                             : kStartPadding) +
        kDangerIconOffset;
    int icon_y = (height() - kWarningIconSize) / 2 + kDangerIconOffset;
    canvas->DrawImageInt(GetWarningIcon(), icon_x, icon_y);
  }
}

void DownloadItemView::OpenDownload() {
  DCHECK(!IsShowingWarningDialog());
  // We're interested in how long it takes users to open downloads.  If they
  // open downloads super quickly, we should be concerned about clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - creation_time_);

  // If this is still around for the next status update, it will be read.
  announce_accessible_alert_soon_ = true;

  // Calling download()->OpenDownload may delete this, so this must be
  // the last thing we do.
  model_->OpenDownload();
}

bool DownloadItemView::SubmitDownloadToFeedbackService(
    DownloadCommands::Command download_command) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service)
    return false;
  safe_browsing::DownloadProtectionService* download_protection_service =
      sb_service->download_protection_service();
  if (!download_protection_service)
    return false;
  // TODO(shaktisahu): Enable feedback service for offline item.
  if (model_->download()) {
    return download_protection_service->MaybeBeginFeedbackForDownload(
        shelf_->browser()->profile(), model_->download(), download_command);
  }
  // WARNING: we are deleted at this point.  Don't access 'this'.
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

void DownloadItemView::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  last_download_item_path_ = model_->GetTargetFilePath();
  im->LoadIcon(last_download_item_path_, IconLoader::SMALL,
               base::Bind(&DownloadItemView::OnExtractIconComplete,
                          base::Unretained(this), IconLoader::IconSize::SMALL),
               &cancelable_task_tracker_);
  im->LoadIcon(last_download_item_path_, IconLoader::NORMAL,
               base::Bind(&DownloadItemView::OnExtractIconComplete,
                          base::Unretained(this), IconLoader::NORMAL),
               &cancelable_task_tracker_);
}

void DownloadItemView::LoadIconIfItemPathChanged() {
  base::FilePath current_download_path = model_->GetTargetFilePath();
  if (last_download_item_path_ == current_download_path)
    return;

  LoadIcon();
}

void DownloadItemView::UpdateColorsFromTheme() {
  if (!GetThemeProvider())
    return;

  open_button_->SetBorder(
      std::make_unique<SeparatorBorder>(GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR)));

  // Use a slightly dimmed version of the base text color.
  SkColor dimmed_text_color = SkColorSetA(GetTextColor(), 0xC7);
  file_name_label_->SetEnabledColor(GetEnabled() ? GetTextColor()
                                                 : dimmed_text_color);
  if (model_->GetDangerType() ==
      download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE) {
    status_label_->SetEnabledColor(SK_ColorGREEN);
  } else if (model_->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS) {
    status_label_->SetEnabledColor(SK_ColorRED);
  } else {
    status_label_->SetEnabledColor(dimmed_text_color);
  }
  SkColor background_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_DOWNLOAD_SHELF);
  file_name_label_->SetBackgroundColor(background_color);
  status_label_->SetBackgroundColor(background_color);

  if (dangerous_download_label_)
    dangerous_download_label_->SetEnabledColor(GetTextColor());
  if (save_button_)
    shelf_->ConfigureButtonForTheme(save_button_);
  if (discard_button_)
    shelf_->ConfigureButtonForTheme(discard_button_);
  if (deep_scanning_label_)
    deep_scanning_label_->SetEnabledColor(GetTextColor());
  if (open_now_button_)
    shelf_->ConfigureButtonForTheme(open_now_button_);
}

void DownloadItemView::ShowContextMenuImpl(const gfx::Rect& rect,
                                           ui::MenuSourceType source_type) {
  // Similar hack as in MenuButton.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to null.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())
      ->SetMouseHandler(nullptr);

  AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);

  if (!context_menu_.get())
    context_menu_ = std::make_unique<DownloadShelfContextMenuView>(this);
  context_menu_->Run(GetWidget()->GetTopLevelWidget(), rect, source_type,
                     base::Bind(&DownloadItemView::ReleaseDropdown,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DownloadItemView::SetDropdownState(State new_state) {
  // Avoid extra SchedulePaint()s if the state is going to be the same and
  // |dropdown_button_| has already been initialized.
  if (dropdown_state_ == new_state &&
      !dropdown_button_->GetImage(views::Button::STATE_NORMAL).isNull())
    return;

  if (new_state != dropdown_state_) {
    dropdown_button_->AnimateInkDrop(new_state == PUSHED
                                         ? views::InkDropState::ACTIVATED
                                         : views::InkDropState::DEACTIVATED,
                                     nullptr);
  }
  dropdown_state_ = new_state;
  UpdateDropdownButton();
  SchedulePaint();
}

void DownloadItemView::ConfigureInkDrop() {
  if (HasInkDrop())
    GetInkDrop()->SetShowHighlightOnHover(!IsShowingWarningDialog());
}

SkColor DownloadItemView::GetInkDropBaseColor() const {
  return color_utils::DeriveDefaultIconColor(GetTextColor());
}

void DownloadItemView::SetMode(Mode mode) {
  mode_ = mode;
  ConfigureInkDrop();
}

void DownloadItemView::TransitionToWarningDialog() {
  if (IsShowingDeepScanning())
    ClearDeepScanningDialog();

  ShowWarningDialog();

  // We need to load the icon now that the download has the real path.
  LoadIcon();

  // Force the shelf to layout again as our size has changed.
  shelf_->Layout();
  shelf_->SchedulePaint();
}

void DownloadItemView::TransitionToDeepScanningDialog() {
  if (IsShowingWarningDialog())
    ClearWarningDialog();

  ShowDeepScanningDialog();

  // We need to load the icon now that the download has the real path.
  LoadIcon();

  // Force the shelf to layout again as our size has changed.
  shelf_->Layout();
  shelf_->SchedulePaint();
}

void DownloadItemView::ClearWarningDialog() {
  DCHECK(IsShowingWarningDialog());

  SetMode(NORMAL_MODE);
  dropdown_state_ = NORMAL;

  // Remove the views used by the warning dialog.
  delete save_button_;
  save_button_ = nullptr;
  delete discard_button_;
  discard_button_ = nullptr;
  delete dangerous_download_label_;
  dangerous_download_label_ = nullptr;
  dangerous_download_label_sized_ = false;

  // We need to load the icon now that the download has the real path.
  LoadIcon();

  open_button_->SetEnabled(true);
  file_name_label_->SetVisible(true);
  status_label_->SetVisible(true);
  dropdown_button_->SetVisible(true);
}

void DownloadItemView::ShowWarningDialog() {
  DCHECK(!IsShowingWarningDialog());
  time_download_warning_shown_ = base::Time::Now();
  download::DownloadDangerType danger_type = model_->GetDangerType();
  RecordDangerousDownloadWarningShown(danger_type);
#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (model_->ShouldAllowDownloadFeedback()) {
    safe_browsing::DownloadFeedbackService::RecordEligibleDownloadShown(
        danger_type);
  }
#endif
  SetMode(model_->MightBeMalicious() ? MALICIOUS_MODE : DANGEROUS_MODE);

  dropdown_state_ = NORMAL;
  if (mode_ == DANGEROUS_MODE) {
    auto save_button = views::MdTextButton::Create(
        this, model_->GetWarningConfirmButtonText());
    save_button_ = AddChildView(std::move(save_button));
  }

  if (model_->GetDangerType() !=
          download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED &&
      model_->GetDangerType() !=
          download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE &&
      model_->GetDangerType() !=
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    auto discard_button = views::MdTextButton::Create(
        this, l10n_util::GetStringUTF16(IDS_DISCARD_DOWNLOAD));
    discard_button_ = AddChildView(std::move(discard_button));
  }

  base::string16 dangerous_label =
      model_->GetWarningText(font_list_, kTextWidth);
  auto dangerous_download_label =
      std::make_unique<views::Label>(dangerous_label);
  dangerous_download_label->SetMultiLine(true);
  dangerous_download_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  dangerous_download_label->SetAutoColorReadabilityEnabled(false);
  dangerous_download_label_ = AddChildView(std::move(dangerous_download_label));
  SizeLabelToMinWidth();

  open_button_->SetEnabled(false);
  file_name_label_->SetVisible(false);
  status_label_->SetVisible(false);
  dropdown_button_->SetVisible(mode_ == MALICIOUS_MODE);
}

gfx::ImageSkia DownloadItemView::GetWarningIcon() {
  switch (model_->GetDangerType()) {
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      if (safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
              model()->profile())
              ->RequestsAdvancedProtectionVerdicts()) {
        return gfx::CreateVectorIcon(
            vector_icons::kErrorIcon, kErrorIconSize,
            GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_AlertSeverityMedium));
      }
      FALLTHROUGH;

    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return gfx::CreateVectorIcon(
          vector_icons::kWarningIcon, kWarningIconSize,
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_AlertSeverityHigh));

    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return gfx::CreateVectorIcon(vector_icons::kErrorIcon, kErrorIconSize,
                                   gfx::kGoogleGrey600);

    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
      break;
  }
  return gfx::ImageSkia();
}

void DownloadItemView::ShowDeepScanningDialog() {
  DCHECK_EQ(mode_, NORMAL_MODE);
  SetMode(DEEP_SCANNING_MODE);

  base::string16 elided_filename =
      gfx::ElideFilename(model_->GetFileNameToReportUser(), font_list_,
                         kTextWidth, gfx::Typesetter::BROWSER);
  base::string16 deep_scanning_text = l10n_util::GetStringFUTF16(
      IDS_PROMPT_DEEP_SCANNING_DOWNLOAD, elided_filename);
  auto deep_scanning_label = std::make_unique<views::Label>(deep_scanning_text);
  deep_scanning_label->SetMultiLine(true);
  deep_scanning_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  deep_scanning_label->SetAutoColorReadabilityEnabled(false);
  deep_scanning_label_ = AddChildView(std::move(deep_scanning_label));
  deep_scanning_label_->SetSize(AdjustTextAndGetSize(deep_scanning_label_));

  int delay_delivery = g_browser_process->local_state()->GetInteger(
      prefs::kDelayDeliveryUntilVerdict);
  if (delay_delivery != safe_browsing::DELAY_DOWNLOADS &&
      delay_delivery != safe_browsing::DELAY_UPLOADS_AND_DOWNLOADS) {
    auto open_now_button = views::MdTextButton::Create(
        this, l10n_util::GetStringUTF16(IDS_OPEN_DOWNLOAD_NOW));
    open_now_button_ = AddChildView(std::move(open_now_button));
    open_button_->SetEnabled(true);
  } else {
    open_button_->SetEnabled(false);
  }

  file_name_label_->SetVisible(false);
  status_label_->SetVisible(false);
}

void DownloadItemView::ClearDeepScanningDialog() {
  DCHECK(IsShowingDeepScanning());

  SetMode(NORMAL_MODE);
  dropdown_state_ = NORMAL;

  delete deep_scanning_label_;
  deep_scanning_label_ = nullptr;

  delete open_now_button_;
  open_now_button_ = nullptr;

  LoadIcon();

  open_button_->SetEnabled(true);
  file_name_label_->SetVisible(true);
  status_label_->SetVisible(true);
}

gfx::Size DownloadItemView::GetButtonSize() const {
  gfx::Size size;
  if (discard_button_)
    size.SetToMax(discard_button_->GetPreferredSize());
  if (save_button_)
    size.SetToMax(save_button_->GetPreferredSize());
  return size;
}

// This method computes the minimum width of the label for displaying its text
// on 2 lines.  It just breaks the string in 2 lines on the spaces and keeps the
// configuration with minimum width.
void DownloadItemView::SizeLabelToMinWidth() {
  if (dangerous_download_label_sized_)
    return;

  dangerous_download_label_->SetSize(
      AdjustTextAndGetSize(dangerous_download_label_));
  dangerous_download_label_sized_ = true;
}

// static
gfx::Size DownloadItemView::AdjustTextAndGetSize(views::Label* label) {
  gfx::Size size = label->GetPreferredSize();

  // If the label's width is already narrower than 200, we don't need to
  // linebreak it, as it will fit on a single line.
  if (size.width() <= 200)
    return size;

  base::string16 label_text = label->GetText();
  base::TrimWhitespace(label_text, base::TRIM_ALL, &label_text);
  DCHECK_EQ(base::string16::npos, label_text.find('\n'));

  // Make the label big so that GetPreferredSize() is not constrained by the
  // current width.
  label->SetBounds(0, 0, 1000, 1000);

  // Use a const string from here. BreakIterator requies that text.data() not
  // change during its lifetime.
  const base::string16 original_text(label_text);
  // Using BREAK_WORD can work in most cases, but it can also break
  // lines where it should not. Using BREAK_LINE is safer although
  // slower for Chinese/Japanese. This is not perf-critical at all, though.
  base::i18n::BreakIterator iter(original_text,
                                 base::i18n::BreakIterator::BREAK_LINE);
  bool status = iter.Init();
  DCHECK(status);

  // Create strings with line break position before and after the mid point, and
  // compare the width to determine whether the best break position is before or
  // after them.
  base::string16 prev_text = original_text;
  std::vector<size_t> break_points;

  while (iter.pos() < original_text.length() / 2) {
    iter.Advance();
    break_points.emplace_back(iter.pos());
  }

  size_t pos = iter.pos();
  bool searching_backward = false;
  gfx::Size min_width_size = size;
  // First add a line break after the mid point. If there is a very long
  // word in the text, |pos| could reach the end of the text.
  if (pos < original_text.length()) {
    searching_backward = true;
    prev_text = SplitStringWithNewLineAtPosition(original_text, pos);
    label->SetText(prev_text);
    min_width_size = label->GetPreferredSize();
  }

  pos = iter.prev();
  base::string16 current_text;
  if (pos != 0) {
    base::string16 current_text =
        SplitStringWithNewLineAtPosition(original_text, pos);
    label->SetText(current_text);
    size = label->GetPreferredSize();

    if (size.width() == min_width_size.width()) {
      // We found the best line break position.
      label->SetText(prev_text);
      return size;
    } else if (size.width() > min_width_size.width()) {
      // The best line break position is after |pos|.
      label->SetText(prev_text);
    } else {
      // The best line break position is before |prev|.
      searching_backward = false;
      prev_text = current_text;
      min_width_size = size;
      break_points.pop_back();
    }
  }

  // Go through the string and try each line break (starting with no line break)
  // searching for the optimal line break position. Stop if we find one that
  // yields minimum label width.
  while (true) {
    if (searching_backward) {
      iter.Advance();
      pos = iter.pos();
      if (pos >= original_text.length())
        break;
    } else {
      break_points.pop_back();
      if (break_points.empty())
        break;
      pos = break_points.back();
    }
    current_text = SplitStringWithNewLineAtPosition(original_text, pos);
    label->SetText(current_text);
    size = label->GetPreferredSize();

    // If the width is growing again, it means we passed the optimal width spot.
    if (size.width() > min_width_size.width()) {
      label->SetText(prev_text);
      return min_width_size;
    }
    prev_text = current_text;
    min_width_size = size;
  }
  return size;
}

void DownloadItemView::Reenable() {
  file_name_label_->SetText(ElidedFilename());
  SetEnabled(true);  // Triggers a repaint.
}

void DownloadItemView::ReleaseDropdown() {
  SetDropdownState(NORMAL);
  // Make sure any new status from activating a context menu option is read.
  announce_accessible_alert_soon_ = true;
}

void DownloadItemView::UpdateAccessibleName() {
  base::string16 new_name;
  if (IsShowingWarningDialog()) {
    new_name = dangerous_download_label_->GetText();
  } else {
    new_name = status_label_->GetText() + base::char16(' ') +
               model_->GetFileNameToReportUser().LossyDisplayName();
  }

  // Do not fire text changed notifications. Screen readers are notified of
  // status changes via the accessible alert notifications, and text change
  // notifications would be redundant.
  accessible_name_ = new_name;
  open_button_->SetAccessibleName(new_name);
}

base::string16 DownloadItemView::GetInProgressAccessibleAlertText() {
  // If opening when complete or there is a warning, use the full status text.
  if (model_->GetOpenWhenComplete() || IsShowingWarningDialog()) {
    UpdateAccessibleName();
    return accessible_name_;
  }

  // Prefer to announce the time remaining, if known.
  base::TimeDelta remaining;
  if (model_->TimeRemaining(&remaining)) {
    // If complete, skip this round: a completion status update is coming soon.
    if (remaining.is_zero())
      return base::string16();
    base::string16 remaining_string =
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                               ui::TimeFormat::LENGTH_SHORT, remaining);
    return l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_STATUS_TIME_REMAINING_ACCESSIBLE_ALERT, remaining_string);
  }

  // Time remaining is unknown, try to announce percent remaining.
  if (model_->PercentComplete() > 0) {
    DCHECK_LE(model_->PercentComplete(), 100);
    return l10n_util::GetStringFUTF16Int(
        IDS_DOWNLOAD_STATUS_PERCENT_COMPLETE_ACCESSIBLE_ALERT,
        100 - model_->PercentComplete());
  }

  // Percent remaining is also unknown, announce bytes to download.
  base::string16 file_name =
      model_->GetFileNameToReportUser().LossyDisplayName();
  return l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_STATUS_IN_PROGRESS_ACCESSIBLE_ALERT,
      ui::FormatBytes(model_->GetTotalBytes()), file_name);
}

void DownloadItemView::UpdateAccessibleAlert(
    const base::string16& accessible_alert_text,
    bool is_last_update) {
  views::ViewAccessibility& ax = accessible_alert_->GetViewAccessibility();
  ax.OverrideRole(ax::mojom::Role::kAlert);
  ax.OverrideName(accessible_alert_text);
  if (is_last_update) {
    // Last update: stop the announcement interval timer and make the last
    // announcement immediately.
    accessible_alert_timer_.AbandonAndStop();
    AnnounceAccessibleAlert();
  } else if (!accessible_alert_timer_.IsRunning()) {
    // First update: start the announcement interval timer and make the first
    // announcement immediately.
    accessible_alert_timer_.Start(FROM_HERE, kAccessibleAlertInterval, this,
                                  &DownloadItemView::AnnounceAccessibleAlert);
    AnnounceAccessibleAlert();
  } else if (announce_accessible_alert_soon_) {
    accessible_alert_timer_.Reset();
    AnnounceAccessibleAlert();
  }
}

void DownloadItemView::AnnounceAccessibleAlert() {
  accessible_alert_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  announce_accessible_alert_soon_ = false;
}

void DownloadItemView::AnimateStateTransition(State from,
                                              State to,
                                              gfx::SlideAnimation* animation) {
  if (from == NORMAL && to == HOT) {
    animation->Show();
  } else if (from == HOT && to == NORMAL) {
    animation->Hide();
  } else if (from != to) {
    animation->Reset((to == HOT) ? 1.0 : 0.0);
  }
}

void DownloadItemView::ProgressTimerFired() {
  // Only repaint for the indeterminate size case. Otherwise, we'll repaint only
  // when there's an update notified via OnDownloadUpdated().
  if (model_->PercentComplete() < 0)
    SchedulePaint();
}

SkColor DownloadItemView::GetTextColor() const {
  return GetTextColorForThemeProvider(GetThemeProvider());
}

base::string16 DownloadItemView::GetStatusText() const {
  if (model_->GetDangerType() ==
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE) {
    return l10n_util::GetStringUTF16(IDS_PROMPT_DOWNLOAD_DEEP_SCANNED_SAFE);
  } else if (model_->GetDangerType() ==
             download::DownloadDangerType::
                 DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS) {
    return l10n_util::GetStringUTF16(
        IDS_PROMPT_DOWNLOAD_DEEP_SCANNED_OPENED_DANGEROUS);
  }

  if (!model_->ShouldPromoteOrigin() ||
      model_->GetOriginalURL().GetOrigin().is_empty()) {
    // Use the default status text.
    return model_->GetStatusText();
  }

#if !defined(OS_ANDROID)
  return url_formatter::ElideUrl(model_->GetOriginalURL().GetOrigin(),
                                 status_font_list_, kTextWidth,
                                 gfx::Typesetter::BROWSER);
#else
  NOTREACHED();
  return base::string16();
#endif
}

base::string16 DownloadItemView::ElidedFilename() {
  return gfx::ElideFilename(model_->GetFileNameToReportUser(), font_list_,
                            kTextWidth);
}

void DownloadItemView::OpenDownloadDuringAsyncScanning() {
  model_->CompleteSafeBrowsingScan();
  should_open_while_scanning_ = true;
}
