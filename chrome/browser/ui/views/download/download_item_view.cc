// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_item_view.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/numerics/math_constants.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/safe_browsing/deep_scanning_modal_dialog.h"
#include "chrome/browser/ui/views/safe_browsing/prompt_for_scanning_modal_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/features.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/native_theme/themed_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

enum class DownloadItemView::Mode {
  kNormal,             // Showing download item.
  kDangerous,          // Displaying the dangerous download warning.
  kMalicious,          // Displaying the malicious download warning.
  kMixedContentWarn,   // Displaying the mixed-content download warning.
  kMixedContentBlock,  // Displaying the mixed-content download block error.
  kDeepScanning,       // Displaying in-progress deep scanning information.
};

namespace {

// TODO(pkasting): Replace bespoke constants in file with standard metrics from
// e.g. LayoutProvider.

constexpr int kTextWidth = 140;

// Padding before the icon and at end of the item.
constexpr int kStartPadding = 12;
constexpr int kEndPadding = 6;

// Horizontal padding between progress indicator and filename/status text.
constexpr int kProgressTextPadding = 8;

// The space between the Save and Discard buttons when prompting for a
// dangerous download.
constexpr int kSaveDiscardButtonPadding = 5;

// The space on the right side of the dangerous download label.
constexpr int kLabelPadding = 8;

// Size of the space used for the progress indicator.
constexpr int kProgressIndicatorSize = 25;

// The vertical distance between the item's visual upper bound (as delineated
// by the separator on the right) and the edge of the shelf.
constexpr int kTopBottomPadding = 6;

// The minimum vertical padding above and below contents of the download item.
// This is only used when the text size is large.
constexpr int kMinimumVerticalPadding = 2 + kTopBottomPadding;

// A stub subclass of Button that has no visuals.
class TransparentButton : public views::Button {
 public:
  METADATA_HEADER(TransparentButton);

  explicit TransparentButton(DownloadItemView* parent)
      : Button(Button::PressedCallback()) {
    views::InstallRectHighlightPathGenerator(this);
    SetInkDropMode(InkDropMode::ON);
    set_context_menu_controller(parent);
  }
  ~TransparentButton() override = default;

  // Button subclasses need to provide this because the default color is
  // kPlaceholderColor. In theory we could statically compute it in the
  // constructor but then it won't be correct after dark mode changes, and to
  // deal with that this class would have to observe NativeTheme and so on.
  SkColor GetInkDropBaseColor() const override {
    // This button will be used like a LabelButton, so use the same foreground
    // base color as a label button.
    return color_utils::DeriveDefaultIconColor(views::style::GetColor(
        *this, views::style::CONTEXT_BUTTON, views::style::STYLE_PRIMARY));
  }

  // Forward dragging and capture loss events, since this class doesn't have
  // enough context to handle them. Let the button class manage visual
  // transitions.
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    Button::OnMouseDragged(event);
    return parent()->OnMouseDragged(event);
  }

  void OnMouseCaptureLost() override {
    parent()->OnMouseCaptureLost();
    Button::OnMouseCaptureLost();
  }

  std::u16string GetTooltipText(const gfx::Point& point) const override {
    return parent()->GetTooltipText(point);
  }
};

BEGIN_METADATA(TransparentButton, views::Button)
END_METADATA

bool UseNewWarnings() {
  return base::FeatureList::IsEnabled(safe_browsing::kUseNewDownloadWarnings);
}

int GetFilenameStyle(const views::Label& label) {
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (UseNewWarnings())
    return STYLE_EMPHASIZED;
#endif
  return label.GetTextStyle();
}

int GetFilenameStyle(const views::StyledLabel& label) {
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (UseNewWarnings())
    return STYLE_EMPHASIZED;
#endif
  return label.GetDefaultTextStyle();
}

void StyleFilename(views::Label& label, size_t pos, size_t len) {
  label.SetTextStyleRange(GetFilenameStyle(label), gfx::Range(pos, pos + len));
}

void StyleFilename(views::StyledLabel& label, size_t pos, size_t len) {
  // Ensure the label contains a nonempty filename.
  if ((pos == std::u16string::npos) || (len == 0))
    return;

  views::StyledLabel::RangeStyleInfo style;
  style.text_style = GetFilenameStyle(label);
  label.ClearStyleRanges();
  label.AddStyleRange(gfx::Range(pos, pos + len), style);
}

// Whether we are warning about a dangerous/malicious download.
bool is_download_warning(DownloadItemView::Mode mode) {
  return (mode == DownloadItemView::Mode::kDangerous) ||
         (mode == DownloadItemView::Mode::kMalicious);
}

// Whether we are in the mixed content mode.
bool is_mixed_content(DownloadItemView::Mode mode) {
  return (mode == DownloadItemView::Mode::kMixedContentWarn) ||
         (mode == DownloadItemView::Mode::kMixedContentBlock);
}

// Whether a warning label is visible.
bool has_warning_label(DownloadItemView::Mode mode) {
  return is_download_warning(mode) || is_mixed_content(mode);
}

}  // namespace

DownloadItemView::DownloadItemView(DownloadUIModel::DownloadUIModelPtr model,
                                   DownloadShelfView* shelf,
                                   views::View* accessible_alert)
    : AnimationDelegateViews(this),
      model_(std::move(model)),
      shelf_(shelf),
      mode_(Mode::kNormal),
      indeterminate_progress_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(30),
          base::BindRepeating(
              [](DownloadItemView* view) {
                if (view->model_->PercentComplete() < 0)
                  view->SchedulePaint();
              },
              base::Unretained(this))),
      accessible_alert_(accessible_alert),
      accessible_alert_timer_(
          FROM_HERE,
          base::TimeDelta::FromMinutes(3),
          base::BindRepeating(&DownloadItemView::AnnounceAccessibleAlert,
                              base::Unretained(this))) {
  views::InstallRectHighlightPathGenerator(this);
  observation_.Observe(this->model());

  // TODO(pkasting): Use bespoke file-scope subclasses for some of these child
  // views to localize functionality and simplify this class.

  open_button_ = AddChildView(std::make_unique<TransparentButton>(this));
  open_button_->SetCallback(base::BindRepeating(
      &DownloadItemView::OpenButtonPressed, base::Unretained(this)));

  file_name_label_ = AddChildView(std::make_unique<views::Label>());
  file_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  file_name_label_->SetTextContext(CONTEXT_DOWNLOAD_SHELF);
  file_name_label_->GetViewAccessibility().OverrideIsIgnored(true);
  const std::u16string filename = ElidedFilename(*file_name_label_);
  file_name_label_->SetText(filename);
  file_name_label_->SetCanProcessEventsWithinSubtree(false);
  StyleFilename(*file_name_label_, 0, filename.length());

  status_label_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), CONTEXT_DOWNLOAD_SHELF_STATUS));
  status_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  warning_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  warning_label_->SetTextContext(CONTEXT_DOWNLOAD_SHELF);
  warning_label_->SetCanProcessEventsWithinSubtree(false);

  deep_scanning_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  deep_scanning_label_->SetTextContext(CONTEXT_DOWNLOAD_SHELF);
  deep_scanning_label_->SetCanProcessEventsWithinSubtree(false);

  open_now_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DownloadItemView::OpenDownloadDuringAsyncScanning,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_OPEN_DOWNLOAD_NOW)));

  save_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DownloadItemView::SaveOrDiscardButtonPressed,
                          base::Unretained(this), DownloadCommands::KEEP)));

  discard_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DownloadItemView::SaveOrDiscardButtonPressed,
                          base::Unretained(this), DownloadCommands::DISCARD),
      l10n_util::GetStringUTF16(IDS_DISCARD_DOWNLOAD)));

  scan_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DownloadItemView::ExecuteCommand,
                          base::Unretained(this), DownloadCommands::DEEP_SCAN),
      l10n_util::GetStringUTF16(IDS_SCAN_DOWNLOAD)));

  dropdown_button_ =
      AddChildView(views::CreateVectorImageButton(base::BindRepeating(
          &DownloadItemView::DropdownButtonPressed, base::Unretained(this))));
  dropdown_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_DOWNLOAD_ITEM_DROPDOWN_BUTTON_ACCESSIBLE_TEXT));
  dropdown_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets(10)));
  dropdown_button_->SetHasInkDropActionOnClick(false);
  dropdown_button_->SizeToPreferredSize();

  complete_animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(2500));
  complete_animation_.SetTweenType(gfx::Tween::LINEAR);

  scanning_animation_.SetThrobDuration(base::TimeDelta::FromMilliseconds(2500));
  scanning_animation_.SetTweenType(gfx::Tween::LINEAR);

  // Further configure default state, e.g. child visibility.
  OnDownloadUpdated();
}

DownloadItemView::~DownloadItemView() = default;

void DownloadItemView::Layout() {
  // TODO(crbug.com/1005568): Replace Layout()/CalculatePreferredSize() with a
  // LayoutManager.

  View::Layout();

  open_button_->SetBoundsRect(GetLocalBounds());
  dropdown_button_->SetPosition(
      gfx::Point(width() - kEndPadding - dropdown_button_->width(),
                 CenterY(dropdown_button_->height())));

  if (mode_ == Mode::kNormal) {
    const int text_x =
        kStartPadding + kProgressIndicatorSize + kProgressTextPadding;
    const int text_end = dropdown_button_->GetVisible()
                             ? (dropdown_button_->x() - kEndPadding)
                             : dropdown_button_->bounds().right();
    const int text_width = text_end - text_x;
    const int file_name_height = file_name_label_->GetLineHeight();
    int text_height = file_name_height;
    if (!status_label_->GetText().empty())
      text_height += status_label_->GetLineHeight();

    file_name_label_->SetBounds(text_x, CenterY(text_height), text_width,
                                file_name_height);
    status_label_->SetBounds(text_x, file_name_label_->bounds().bottom(),
                             text_width,
                             status_label_->GetPreferredSize().height());
  } else {
    auto* const label =
        (mode_ == Mode::kDeepScanning) ? deep_scanning_label_ : warning_label_;
    label->SetPosition(gfx::Point(kStartPadding * 2 + GetIcon().Size().width(),
                                  CenterY(label->height())));

    const gfx::Size button_size = GetButtonSize();
    gfx::Rect button_bounds(gfx::Point(label->bounds().right() + kLabelPadding,
                                       CenterY(button_size.height())),
                            button_size);
    for (auto* button :
         {save_button_, discard_button_, scan_button_, open_now_button_}) {
      button->SetBoundsRect(button_bounds);
      if (button->GetVisible())
        button_bounds.set_x(button_bounds.right() + kSaveDiscardButtonPadding);
    }
  }
}

bool DownloadItemView::OnMouseDragged(const ui::MouseEvent& event) {
  // Handle drag (file copy) operations.

  // Mouse should not activate us in dangerous mode.
  if (has_warning_label(mode_))
    return true;

  if (!drag_start_point_)
    drag_start_point_ = event.location();
  if (!dragging_) {
    dragging_ = ExceededDragThreshold(event.location() - *drag_start_point_);
  } else if ((model_->GetState() == download::DownloadItem::COMPLETE) &&
             model_->download()) {
    const gfx::Image* const file_icon =
        g_browser_process->icon_manager()->LookupIconFromFilepath(
            model_->GetTargetFilePath(), IconLoader::SMALL);
    const views::Widget* const widget = GetWidget();
    // TODO(shaktisahu): Make DragDownloadItem work with a model.
    DragDownloadItem(model_->download(), file_icon,
                     widget ? widget->GetNativeView() : nullptr);
    RecordDownloadShelfDragEvent(DownloadShelfDragEvent::STARTED);
  }
  return true;
}

void DownloadItemView::OnMouseCaptureLost() {
  // Mouse should not activate us in dangerous mode.
  if (mode_ != Mode::kNormal)
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    drag_start_point_.reset();
  }
}

std::u16string DownloadItemView::GetTooltipText(const gfx::Point& p) const {
  return has_warning_label(mode_) ? std::u16string() : tooltip_text_;
}

void DownloadItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
  node_data->role = ax::mojom::Role::kGroup;

  // Set the description to the empty string, otherwise the tooltip will be
  // used, which is redundant with the accessible name.
  node_data->SetDescription(std::u16string());
}

void DownloadItemView::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ShowContextMenuImpl(gfx::Rect(point, gfx::Size()), source_type);
}

void DownloadItemView::OnDownloadUpdated() {
  if (!model_->ShouldShowInShelf()) {
    shelf_->RemoveDownloadView(this);
    // WARNING: |this| has been deleted!
    return;
  }

  SetMode(GetDesiredMode());

  if (model_->GetState() == download::DownloadItem::COMPLETE &&
      model_->ShouldRemoveFromShelfWhenComplete()) {
    shelf_->RemoveDownloadView(this);
    // WARNING: |this| has been deleted!
    return;
  }

  const std::u16string new_tooltip_text = model_->GetTooltipText();
  if (new_tooltip_text != tooltip_text_) {
    tooltip_text_ = new_tooltip_text;
    TooltipTextChanged();
  }
}

void DownloadItemView::OnDownloadOpened() {
  SetEnabled(false);
  file_name_label_->SetTextStyle(views::style::STYLE_DISABLED);
  const std::u16string filename = ElidedFilename(*file_name_label_);
  size_t filename_offset;
  file_name_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_STATUS_OPENING, filename, &filename_offset));
  StyleFilename(*file_name_label_, filename_offset, filename.length());

  const auto reenable = [](base::WeakPtr<DownloadItemView> view) {
    if (!view)
      return;
    view->SetEnabled(true);
    auto* label = view->file_name_label_;
    label->SetTextStyle(views::style::STYLE_PRIMARY);
    const std::u16string filename = view->ElidedFilename(*label);
    label->SetText(filename);
    StyleFilename(*label, 0, filename.length());
  };
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(reenable), weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(3));

  shelf_->AutoClose();
}

void DownloadItemView::OnDownloadDestroyed() {
  shelf_->RemoveDownloadView(this);  // This will delete us!
}

void DownloadItemView::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void DownloadItemView::AnimationEnded(const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

void DownloadItemView::MaybeSubmitDownloadToFeedbackService(
    DownloadCommands::Command command) {
  if (!model_->ShouldAllowDownloadFeedback() ||
      !SubmitDownloadToFeedbackService(command))
    ExecuteCommand(command);
}

gfx::Size DownloadItemView::CalculatePreferredSize() const {
  int height, width = dropdown_button_->GetVisible()
                          ? (dropdown_button_->width() + kEndPadding)
                          : 0;

  if (mode_ == Mode::kNormal) {
    int label_width =
        std::max(file_name_label_->GetPreferredSize().width(), kTextWidth);
    if (model_->GetDangerType() ==
        download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE) {
      label_width =
          std::max(label_width, status_label_->GetPreferredSize().width());
    }
    width += kStartPadding + kProgressIndicatorSize + kProgressTextPadding +
             label_width + kEndPadding;
    height = file_name_label_->GetLineHeight() + status_label_->GetLineHeight();
  } else {
    auto* const label =
        (mode_ == Mode::kDeepScanning) ? deep_scanning_label_ : warning_label_;
    height = label->GetLineHeight() * 2;
    const gfx::Size icon_size = GetIcon().Size();
    width +=
        kStartPadding * 2 + icon_size.width() + label->width() + kEndPadding;
    height = std::max(height, icon_size.height());
    const int visible_buttons = base::ranges::count(
        std::array<const views::View*, 4>{save_button_, discard_button_,
                                          scan_button_, open_now_button_},
        true, &views::View::GetVisible);
    if (visible_buttons > 0) {
      const gfx::Size button_size = GetButtonSize();
      width += kLabelPadding + button_size.width() * visible_buttons +
               kSaveDiscardButtonPadding * (visible_buttons - 1);
      height = std::max(height, button_size.height());
    }
  }

  // The normal height of the item which may be exceeded if text is large.
  constexpr int kDefaultDownloadItemHeight = 48;
  return gfx::Size(width, std::max(kDefaultDownloadItemHeight,
                                   2 * kMinimumVerticalPadding + height));
}

void DownloadItemView::OnPaintBackground(gfx::Canvas* canvas) {
  View::OnPaintBackground(canvas);

  // Draw the separator as part of the background. It will be covered by the
  // focus ring when the view has focus.
  gfx::Rect rect(width() - 1, 0, 1, height());
  rect.Inset(0, kTopBottomPadding);
  canvas->FillRect(GetMirroredRect(rect),
                   GetThemeProvider()->GetColor(
                       ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR));
}

void DownloadItemView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);

  const bool use_new_warnings = UseNewWarnings();

  const gfx::Image* const file_icon_image =
      g_browser_process->icon_manager()->LookupIconFromFilepath(
          model_->GetTargetFilePath(), IconLoader::SMALL);
  const gfx::ImageSkia* file_icon = (file_icon_image && mode_ == Mode::kNormal)
                                        ? file_icon_image->ToImageSkia()
                                        : nullptr;

  // Paint download progress.
  // TODO(pkasting): Use a child view to display this.
  const int progress_x =
      GetMirroredXWithWidthInView(kStartPadding, kProgressIndicatorSize);
  const int progress_y = CenterY(kProgressIndicatorSize);
  const gfx::RectF progress_bounds(
      progress_x, progress_y, kProgressIndicatorSize, kProgressIndicatorSize);
  const download::DownloadItem::DownloadState state = model_->GetState();
  if (mode_ == Mode::kNormal && state == download::DownloadItem::IN_PROGRESS) {
    base::TimeDelta indeterminate_progress_time =
        indeterminate_progress_time_elapsed_;
    if (!model_->IsPaused()) {
      indeterminate_progress_time +=
          base::TimeTicks::Now() - indeterminate_progress_start_time_;
    }
    PaintDownloadProgress(canvas, progress_bounds, indeterminate_progress_time,
                          model_->PercentComplete());
  } else if (complete_animation_.is_animating()) {
    DCHECK_EQ(Mode::kNormal, mode_);
    // Loop back and forth five times.
    double start = 0, end = 5;
    if (model_->GetState() == download::DownloadItem::INTERRUPTED)
      std::swap(start, end);
    const double value = gfx::Tween::DoubleValueBetween(
        complete_animation_.GetCurrentValue(), start, end);
    const double opacity = std::sin((value + 0.5) * base::kPiDouble) / 2 + 0.5;
    canvas->SaveLayerAlpha(
        static_cast<uint8_t>(gfx::Tween::IntValueBetween(opacity, 0, 255)));
    PaintDownloadProgress(canvas, progress_bounds, base::TimeDelta(), 100);
    canvas->Restore();
  } else if (scanning_animation_.is_animating()) {
    DCHECK_EQ(Mode::kDeepScanning, mode_);
    const double value = gfx::Tween::DoubleValueBetween(
        scanning_animation_.GetCurrentValue(), 0, 2 * base::kPiDouble);
    const double opacity = std::sin(value + base::kPiDouble / 2) / 2 + 0.5;
    canvas->SaveLayerAlpha(
        static_cast<uint8_t>(gfx::Tween::IntValueBetween(opacity, 0, 255)));
    PaintDownloadProgress(canvas, GetIconBounds(), base::TimeDelta(), 100);
    canvas->Restore();
  } else if (use_new_warnings) {
    file_icon = &file_icon_;
  }

  // Draw the file icon.
  if (file_icon) {
    const int offset = (progress_bounds.height() - file_icon->height()) / 2;
    cc::PaintFlags flags;
    // Use an alpha to make the image look disabled.
    if (!GetEnabled())
      flags.setAlpha(120);
    canvas->DrawImageInt(*file_icon, progress_x + offset, progress_y + offset,
                         flags);
  }

  // Overlay the warning icon if appropriate.
  if (mode_ != Mode::kNormal) {
    const gfx::ImageSkia icon = ui::ThemedVectorIcon(GetIcon().GetVectorIcon())
                                    .GetImageSkia(GetNativeTheme());
    gfx::RectF bounds = GetIconBounds();
    canvas->DrawImageInt(icon, bounds.x(), bounds.y());
  }

  OnPaintBorder(canvas);
}

void DownloadItemView::OnThemeChanged() {
  views::View::OnThemeChanged();

  const SkColor background_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_DOWNLOAD_SHELF);
  SetBackground(views::CreateSolidBackground(background_color));
  file_name_label_->SetBackgroundColor(background_color);
  status_label_->SetBackgroundColor(background_color);
  warning_label_->SetDisplayedOnBackgroundColor(background_color);
  deep_scanning_label_->SetDisplayedOnBackgroundColor(background_color);

  shelf_->ConfigureButtonForTheme(open_now_button_);
  shelf_->ConfigureButtonForTheme(save_button_);
  shelf_->ConfigureButtonForTheme(discard_button_);
  shelf_->ConfigureButtonForTheme(scan_button_);

  UpdateDropdownButtonImage();
}

DownloadItemView::Mode DownloadItemView::GetDesiredMode() const {
  if (model_->IsMixedContent()) {
    const bool warn = model_->GetMixedContentStatus() ==
                      download::DownloadItem::MixedContentStatus::WARN;
    return warn ? Mode::kMixedContentWarn : Mode::kMixedContentBlock;
  }

  if (model_->IsDangerous() &&
      (model_->GetState() != download::DownloadItem::CANCELLED))
    return model_->MightBeMalicious() ? Mode::kMalicious : Mode::kDangerous;

  return ((model_->GetDangerType() ==
           download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) &&
          (model_->GetState() != download::DownloadItem::CANCELLED))
             ? Mode::kDeepScanning
             : Mode::kNormal;
}

void DownloadItemView::SetMode(Mode mode) {
  if (mode_ == mode && mode != Mode::kNormal)
    return;
  mode_ = mode;
  UpdateFilePathAndIcons();
  UpdateLabels();
  UpdateButtons();
  UpdateAnimationForDeepScanningMode();

  // Update the accessible name to contain the status text, filename, and
  // warning message (if any). The name will be presented when the download item
  // receives focus.
  const std::u16string unelided_filename =
      model_->GetFileNameToReportUser().LossyDisplayName();
  accessible_name_ =
      has_warning_label(mode_)
          ? warning_label_->GetText()
          : (status_label_->GetText() + u' ' + unelided_filename);
  open_button_->SetAccessibleName(accessible_name_);
  // Do not fire text changed notifications. Screen readers are notified of
  // status changes via the accessible alert notifications, and text change
  // notifications would be redundant.

  if (mode_ == Mode::kNormal) {
    UpdateAccessibleAlertAndAnimationsForNormalMode();
  } else if (is_download_warning(mode_)) {
    const auto danger_type = model_->GetDangerType();
    RecordDangerousDownloadWarningShown(danger_type);
    announce_accessible_alert_soon_ = true;
    if (danger_type == download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
      UpdateAccessibleAlert(l10n_util::GetStringFUTF16(
          base::FeatureList::IsEnabled(safe_browsing::kPromptEsbForDeepScanning)
              ? IDS_PROMPT_DEEP_SCANNING_ACCESSIBLE_ALERT
              : IDS_PROMPT_APP_DEEP_SCANNING_ACCESSIBLE_ALERT,
          unelided_filename));
    } else {
      size_t ignore;
      UpdateAccessibleAlert(model_->GetWarningText(unelided_filename, &ignore));
      accessible_alert_timer_.Stop();
    }
  } else if (is_mixed_content(mode_)) {
    announce_accessible_alert_soon_ = true;
    UpdateAccessibleAlert(l10n_util::GetStringFUTF16(
        IDS_PROMPT_DOWNLOAD_MIXED_CONTENT_BLOCKED_ACCESSIBLE_ALERT,
        unelided_filename));
  } else if (mode_ == Mode::kDeepScanning) {
    UpdateAccessibleAlert(l10n_util::GetStringFUTF16(
        IDS_DEEP_SCANNING_ACCESSIBLE_ALERT, unelided_filename));
  }

  shelf_->InvalidateLayout();
  OnPropertyChanged(&mode_, views::kPropertyEffectsNone);
}

DownloadItemView::Mode DownloadItemView::GetMode() const {
  return mode_;
}

void DownloadItemView::UpdateFilePathAndIcons() {
  // The file icon may change when the download completes, and the path to look
  // up is |file_path_| and thus changes if that changes.  If neither of those
  // is the case, there's nothing to do.
  const base::FilePath file_path = model_->GetTargetFilePath();
  if ((model_->GetState() != download::DownloadItem::COMPLETE) &&
      (file_path_ == file_path))
    return;

  file_path_ = file_path;
  cancelable_task_tracker_.TryCancelAll();

  // The small icon is not stored directly, but will be requested in other
  // functions, so ask the icon manager to load it so it's cached.
  IconManager* const im = g_browser_process->icon_manager();
  im->LoadIcon(file_path_, IconLoader::SMALL,
               base::BindOnce(&DownloadItemView::OnFileIconLoaded,
                              base::Unretained(this), IconLoader::SMALL),
               &cancelable_task_tracker_);

  im->LoadIcon(file_path_, IconLoader::NORMAL,
               base::BindOnce(&DownloadItemView::OnFileIconLoaded,
                              base::Unretained(this), IconLoader::NORMAL),
               &cancelable_task_tracker_);
}

void DownloadItemView::UpdateLabels() {
  file_name_label_->SetVisible(mode_ == Mode::kNormal);

  status_label_->SetVisible(mode_ == Mode::kNormal);
  if (status_label_->GetVisible()) {
    const auto text_and_style = GetStatusTextAndStyle();
    status_label_->SetText(text_and_style.first);
    status_label_->SetTextStyle(text_and_style.second);
    status_label_->GetViewAccessibility().OverrideIsIgnored(
        status_label_->GetText().empty());
  }

  warning_label_->SetVisible(has_warning_label(mode_));
  if (warning_label_->GetVisible()) {
    const std::u16string filename = ElidedFilename(*warning_label_);
    size_t filename_offset;
    warning_label_->SetText(model_->GetWarningText(filename, &filename_offset));
    StyleFilename(*warning_label_, filename_offset, filename.length());
    warning_label_->SizeToFit(GetLabelWidth(*warning_label_));
  }

  deep_scanning_label_->SetVisible(mode_ == Mode::kDeepScanning);
  if (deep_scanning_label_->GetVisible()) {
    const int id = (model_->download() &&
                    safe_browsing::DeepScanningRequest::ShouldUploadBinary(
                        model_->download()))
                       ? IDS_PROMPT_DEEP_SCANNING_DOWNLOAD
                       : IDS_PROMPT_DEEP_SCANNING_APP_DOWNLOAD;
    const std::u16string filename = ElidedFilename(*deep_scanning_label_);
    size_t filename_offset;
    deep_scanning_label_->SetText(
        l10n_util::GetStringFUTF16(id, filename, &filename_offset));
    StyleFilename(*deep_scanning_label_, filename_offset, filename.length());
    deep_scanning_label_->SizeToFit(GetLabelWidth(*deep_scanning_label_));
  }
}

void DownloadItemView::UpdateButtons() {
  bool prompt_to_scan = false, prompt_to_discard = false;
  if (is_download_warning(mode_)) {
    const auto danger_type = model_->GetDangerType();
    prompt_to_scan =
        danger_type == download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
    prompt_to_discard =
        !prompt_to_scan &&
        !ChromeDownloadManagerDelegate::IsDangerTypeBlocked(danger_type);
  }

  const bool allow_open_during_deep_scan =
      (mode_ == Mode::kDeepScanning) &&
      !enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
           model_->profile())
           ->DelayUntilVerdict(
               enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
  open_button_->SetEnabled((mode_ == Mode::kNormal) || prompt_to_scan ||
                           allow_open_during_deep_scan);

  open_now_button_->SetVisible(allow_open_during_deep_scan);

  save_button_->SetVisible((mode_ == Mode::kDangerous) ||
                           (mode_ == Mode::kMixedContentWarn));
  save_button_->SetText(model_->GetWarningConfirmButtonText());

  discard_button_->SetVisible((mode_ == Mode::kMixedContentBlock) ||
                              prompt_to_discard);
  scan_button_->SetVisible(prompt_to_scan);

  dropdown_button_->SetVisible(model_->ShouldShowDropdown());
}

void DownloadItemView::UpdateAccessibleAlertAndAnimationsForNormalMode() {
  using State = download::DownloadItem::DownloadState;
  const State state = model_->GetState();
  if ((state == State::IN_PROGRESS) && !model_->IsPaused()) {
    UpdateAccessibleAlert(GetInProgressAccessibleAlertText());

    if (!indeterminate_progress_timer_.IsRunning()) {
      indeterminate_progress_start_time_ = base::TimeTicks::Now();
      indeterminate_progress_timer_.Reset();
    }

    // For determinate progress, this function is called each time more data is
    // received, which should result in updating the progress indicator.
    if (model_->PercentComplete() > 0)
      SchedulePaint();
    return;
  }

  if (state != State::IN_PROGRESS) {
    if (state == State::CANCELLED) {
      complete_animation_.Stop();
    } else {
      complete_animation_.Reset();
      complete_animation_.Show();
    }

    // Send accessible alert since the download has terminated. No need to alert
    // for "in progress but paused", as the button ends up being refocused in
    // the actual use case, and the name of the button reports that the download
    // has been paused.
    static constexpr auto kMap = base::MakeFixedFlatMap<State, int>({
        {State::INTERRUPTED, IDS_DOWNLOAD_FAILED_ACCESSIBLE_ALERT},
        {State::COMPLETE, IDS_DOWNLOAD_COMPLETE_ACCESSIBLE_ALERT},
        {State::CANCELLED, IDS_DOWNLOAD_CANCELLED_ACCESSIBLE_ALERT},
    });
    const std::u16string alert_text = l10n_util::GetStringFUTF16(
        kMap.at(state), model_->GetFileNameToReportUser().LossyDisplayName());
    announce_accessible_alert_soon_ = true;
    UpdateAccessibleAlert(alert_text);
  }

  accessible_alert_timer_.Stop();
  if (indeterminate_progress_timer_.IsRunning()) {
    indeterminate_progress_time_elapsed_ +=
        base::TimeTicks::Now() - indeterminate_progress_start_time_;
    indeterminate_progress_timer_.Stop();
  }
}

void DownloadItemView::UpdateAccessibleAlert(
    const std::u16string& accessible_alert_text) {
  views::ViewAccessibility& ax = accessible_alert_->GetViewAccessibility();
  ax.OverrideRole(ax::mojom::Role::kAlert);
  ax.OverrideName(accessible_alert_text);
  if (announce_accessible_alert_soon_ || !accessible_alert_timer_.IsRunning()) {
    AnnounceAccessibleAlert();
    accessible_alert_timer_.Reset();
  }
}

void DownloadItemView::UpdateAnimationForDeepScanningMode() {
  if (mode_ == Mode::kDeepScanning) {
    // -1 to throb indefinitely.
    scanning_animation_.StartThrobbing(-1);
  } else {
    scanning_animation_.End();
  }
}

std::u16string DownloadItemView::GetInProgressAccessibleAlertText() const {
  // If opening when complete or there is a warning, use the full status text.
  if (model_->GetOpenWhenComplete() || has_warning_label(mode_))
    return accessible_name_;

  // Prefer to announce the time remaining, if known.
  base::TimeDelta remaining;
  if (model_->TimeRemaining(&remaining)) {
    // If complete, skip this round: a completion status update is coming soon.
    if (remaining.is_zero())
      return std::u16string();

    const std::u16string remaining_string =
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
  return l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_STATUS_IN_PROGRESS_ACCESSIBLE_ALERT,
      ui::FormatBytes(model_->GetTotalBytes()),
      model_->GetFileNameToReportUser().LossyDisplayName());
}

void DownloadItemView::AnnounceAccessibleAlert() {
  accessible_alert_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  announce_accessible_alert_soon_ = false;
}

void DownloadItemView::OnFileIconLoaded(IconLoader::IconSize icon_size,
                                        gfx::Image icon_bitmap) {
  if (!icon_bitmap.IsEmpty()) {
    if (icon_size == IconLoader::NORMAL) {
      // We want a 24x24 icon, but on Windows only 16x16 and 32x32 are
      // available. So take the NORMAL icon and downsize it.
      constexpr gfx::Size kFileIconSize(24, 24);
      file_icon_ = gfx::ImageSkiaOperations::CreateResizedImage(
          *icon_bitmap.ToImageSkia(), skia::ImageOperations::RESIZE_BEST,
          kFileIconSize);
    }
    SchedulePaint();
  }
}

void DownloadItemView::PaintDownloadProgress(
    gfx::Canvas* canvas,
    const gfx::RectF& bounds,
    const base::TimeDelta& indeterminate_progress_time,
    int percent_done) const {
  const SkColor color = GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TAB_THROBBER_SPINNING);

  // Draw background.
  cc::PaintFlags bg_flags;
  bg_flags.setColor(SkColorSetA(color, 0x33));
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);
  bg_flags.setAntiAlias(true);
  canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2, bg_flags);

  // Calculate progress.
  SkScalar start_pos = SkIntToScalar(270);  // 12 o'clock
  SkScalar sweep_angle = SkDoubleToScalar(360 * percent_done / 100.0);
  if (percent_done < 0) {
    // Download size unknown.  Draw a 50 degree sweep that moves at 80 degrees
    // per second.
    start_pos +=
        SkDoubleToScalar(indeterminate_progress_time.InSecondsF() * 80);
    sweep_angle = SkIntToScalar(50);
  }

  // Draw progress.
  SkPath progress;
  progress.addArc(gfx::RectFToSkRect(bounds), start_pos, sweep_angle);
  cc::PaintFlags progress_flags;
  progress_flags.setColor(color);
  progress_flags.setStyle(cc::PaintFlags::kStroke_Style);
  progress_flags.setStrokeWidth(1.7f);
  progress_flags.setAntiAlias(true);
  canvas->DrawPath(progress, progress_flags);
}

ui::ImageModel DownloadItemView::GetIcon() const {
  // TODO(pkasting): Use a child view (ImageView subclass?) to display the icon
  // instead of recomputing this and drawing manually.

  // TODO(drubery): Replace these sizes with layout provider constants when the
  // new UX is fully launched.
  const int non_error_icon_size = UseNewWarnings() ? 20 : 27;
  const auto kWarning = ui::ImageModel::FromVectorIcon(
      vector_icons::kWarningIcon, ui::NativeTheme::kColorId_AlertSeverityMedium,
      non_error_icon_size);
  const auto kError = ui::ImageModel::FromVectorIcon(
      vector_icons::kErrorIcon, ui::NativeTheme::kColorId_AlertSeverityHigh,
      UseNewWarnings() ? 20 : 24);

  const auto danger_type = model_->GetDangerType();
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return safe_browsing::AdvancedProtectionStatusManagerFactory::
                     GetForProfile(model_->profile())
                         ->IsUnderAdvancedProtection()
                 ? kWarning
                 : kError;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return kError;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return kWarning;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return ui::ImageModel::FromVectorIcon(
          (danger_type == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING)
              ? views::kInfoIcon
              : vector_icons::kHelpIcon,
          ui::NativeTheme::kColorId_DefaultIconColor, non_error_icon_size);
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  switch (model_->GetMixedContentStatus()) {
    case download::DownloadItem::MixedContentStatus::BLOCK:
      return kError;
    case download::DownloadItem::MixedContentStatus::WARN:
      return kWarning;
    case download::DownloadItem::MixedContentStatus::UNKNOWN:
    case download::DownloadItem::MixedContentStatus::SAFE:
    case download::DownloadItem::MixedContentStatus::VALIDATED:
    case download::DownloadItem::MixedContentStatus::SILENT_BLOCK:
      break;
  }

  NOTREACHED();
  return ui::ImageModel();
}

gfx::RectF DownloadItemView::GetIconBounds() const {
  // TODO(drubery): When launching the new warnings, turn these numbers into
  // appropriately named constants.
  const int offset = UseNewWarnings() ? 8 : 0;
  const gfx::Size size = GetIcon().Size();
  const int icon_x =
      GetMirroredXWithWidthInView(kStartPadding, size.width()) + offset;
  const int icon_y = CenterY(size.height()) + offset;
  return gfx::RectF(icon_x, icon_y, size.width(), size.height());
}

std::pair<std::u16string, int> DownloadItemView::GetStatusTextAndStyle() const {
  using DangerType = download::DownloadDangerType;
  const auto type = model_->GetDangerType();
  if (type == DangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE) {
    return {l10n_util::GetStringUTF16(IDS_PROMPT_DOWNLOAD_DEEP_SCANNED_SAFE),
            STYLE_GREEN};
  }
  constexpr int kDangerous = IDS_PROMPT_DOWNLOAD_DEEP_SCANNED_OPENED_DANGEROUS;
  if (type == DangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS)
    return {l10n_util::GetStringUTF16(kDangerous), STYLE_RED};

  const GURL url = model_->GetOriginalURL().GetOrigin();
  const std::u16string text =
      (!model_->ShouldPromoteOrigin() || url.is_empty())
          ? model_->GetStatusText()
#if defined(OS_ANDROID)
          // url_formatter::ElideUrl() doesn't exist on Android.
          : std::u16string();
#else
          : url_formatter::ElideUrl(url, status_label_->font_list(),
                                    kTextWidth);
#endif
  return {text, views::style::STYLE_PRIMARY};
}

gfx::Size DownloadItemView::GetButtonSize() const {
  if (mode_ == Mode::kDeepScanning)
    return open_now_button_->GetPreferredSize();

  gfx::Size size;
  if (discard_button_->GetVisible())
    size.SetToMax(discard_button_->GetPreferredSize());
  if (save_button_->GetVisible())
    size.SetToMax(save_button_->GetPreferredSize());
  if (scan_button_->GetVisible())
    size.SetToMax(scan_button_->GetPreferredSize());
  return size;
}

std::u16string DownloadItemView::ElidedFilename(
    const views::Label& label) const {
  const gfx::FontList& font_list =
      views::style::GetFont(CONTEXT_DOWNLOAD_SHELF, GetFilenameStyle(label));
  return gfx::ElideFilename(model_->GetFileNameToReportUser(), font_list,
                            kTextWidth);
}

std::u16string DownloadItemView::ElidedFilename(
    const views::StyledLabel& label) const {
  const gfx::FontList& font_list =
      views::style::GetFont(CONTEXT_DOWNLOAD_SHELF, GetFilenameStyle(label));
  return gfx::ElideFilename(model_->GetFileNameToReportUser(), font_list,
                            kTextWidth);
}

int DownloadItemView::CenterY(int element_height) const {
  return (height() - element_height) / 2;
}

int DownloadItemView::GetLabelWidth(const views::StyledLabel& label) const {
  auto lines_for_width = [&label](int width) {
    return label.GetLayoutSizeInfoForWidth(width).line_sizes.size();
  };

  // Return 200 if that much width is sufficient to fit |label| on one line.
  int width = 200;
  if (lines_for_width(width) < 2)
    return width;

  // Find an upper bound width sufficient to fit |label| on two lines.
  int min_width = 1, max_width;
  for (max_width = width; lines_for_width(max_width) > 2; max_width *= 2)
    min_width = max_width;

  // Binary-search for the smallest width that fits on two lines.
  // TODO(pkasting): Can use std::iota_view() when C++20 is available.
  std::vector<int> widths(max_width + 1 - min_width);
  std::iota(widths.begin(), widths.end(), min_width);
  return *base::ranges::lower_bound(widths, 2, base::ranges::greater{},
                                    std::move(lines_for_width));
}

void DownloadItemView::SetDropdownPressed(bool pressed) {
  if (dropdown_pressed_ == pressed)
    return;
  dropdown_pressed_ = pressed;
  dropdown_button_->SetHighlighted(dropdown_pressed_);
  UpdateDropdownButtonImage();
  OnPropertyChanged(&dropdown_pressed_, views::kPropertyEffectsNone);
}

bool DownloadItemView::GetDropdownPressed() const {
  return dropdown_pressed_;
}

void DownloadItemView::UpdateDropdownButtonImage() {
  views::SetImageFromVectorIcon(
      dropdown_button_, dropdown_pressed_ ? kCaretDownIcon : kCaretUpIcon,
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
}

void DownloadItemView::OpenButtonPressed() {
  if (mode_ == Mode::kNormal) {
    complete_animation_.End();
    announce_accessible_alert_soon_ = true;
    model_->OpenDownload();
    // WARNING: |this| may be deleted!
  } else {
    ShowOpenDialog(
        shelf_->browser()->tab_strip_model()->GetActiveWebContents());
  }
}

void DownloadItemView::SaveOrDiscardButtonPressed(
    DownloadCommands::Command command) {
  if (is_mixed_content(mode_))
    ExecuteCommand(command);
  else
    MaybeSubmitDownloadToFeedbackService(command);
  // WARNING: |this| may be deleted!
}

void DownloadItemView::DropdownButtonPressed(const ui::Event& event) {
  SetDropdownPressed(true);
  ShowContextMenuImpl(dropdown_button_->GetBoundsInScreen(),
                      ui::GetMenuSourceTypeForEvent(event));
}

void DownloadItemView::ShowOpenDialog(content::WebContents* web_contents) {
  if (mode_ == Mode::kDeepScanning) {
    TabModalConfirmDialog::Create(
        std::make_unique<safe_browsing::DeepScanningModalDialog>(
            web_contents,
            base::BindOnce(&DownloadItemView::OpenDownloadDuringAsyncScanning,
                           weak_ptr_factory_.GetWeakPtr())),
        web_contents);
  } else {
    safe_browsing::PromptForScanningModalDialog::ShowForWebContents(
        web_contents, model_->GetFileNameToReportUser().LossyDisplayName(),
        base::BindOnce(&DownloadItemView::ExecuteCommand,
                       weak_ptr_factory_.GetWeakPtr(),
                       DownloadCommands::DEEP_SCAN),
        base::BindOnce(&DownloadItemView::ExecuteCommand,
                       weak_ptr_factory_.GetWeakPtr(),
                       DownloadCommands::BYPASS_DEEP_SCANNING));
  }
}

void DownloadItemView::ShowContextMenuImpl(const gfx::Rect& rect,
                                           ui::MenuSourceType source_type) {
  // Similar hack as in MenuButtonController.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to null.
  // TODO(pkasting): Use an actual MenuButtonController and get rid of the
  // one-off reimplementation of pressed-locking and similar.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())
      ->SetMouseAndGestureHandler(nullptr);

  const auto release_dropdown = [](DownloadItemView* view) {
    view->SetDropdownPressed(false);
    // Make sure any new status from activating a context menu option is read.
    view->announce_accessible_alert_soon_ = true;
  };
  context_menu_.Run(
      GetWidget()->GetTopLevelWidget(), rect, source_type,
      base::BindRepeating(std::move(release_dropdown), base::Unretained(this)));
}

void DownloadItemView::OpenDownloadDuringAsyncScanning() {
  model_->CompleteSafeBrowsingScan();
  model_->SetOpenWhenComplete(true);
}

bool DownloadItemView::SubmitDownloadToFeedbackService(
    DownloadCommands::Command command) const {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  auto* const sb_service = g_browser_process->safe_browsing_service();
  if (!sb_service)
    return false;
  auto* const dp_service = sb_service->download_protection_service();
  if (!dp_service)
    return false;
  // TODO(shaktisahu): Enable feedback service for offline item.
  return !model_->download() ||
         dp_service->MaybeBeginFeedbackForDownload(shelf_->browser()->profile(),
                                                   model_->download(), command);
#else
  NOTREACHED();
  return false;
#endif
}

void DownloadItemView::ExecuteCommand(DownloadCommands::Command command) {
  commands_.ExecuteCommand(command);
}

DEFINE_ENUM_CONVERTERS(DownloadItemView::Mode,
                       {DownloadItemView::Mode::kNormal, u"kNormal"},
                       {DownloadItemView::Mode::kDangerous, u"kDangerous"},
                       {DownloadItemView::Mode::kMalicious, u"kMalicious"},
                       {DownloadItemView::Mode::kMixedContentWarn,
                        u"kMixedContentWarn"},
                       {DownloadItemView::Mode::kMixedContentBlock,
                        u"kMixedContentBlock"})

BEGIN_METADATA(DownloadItemView, views::View)
ADD_READONLY_PROPERTY_METADATA(Mode, Mode)
ADD_READONLY_PROPERTY_METADATA(std::u16string, InProgressAccessibleAlertText)
ADD_READONLY_PROPERTY_METADATA(gfx::RectF, IconBounds)
ADD_READONLY_PROPERTY_METADATA(gfx::Size, ButtonSize)
ADD_PROPERTY_METADATA(bool, DropdownPressed)
END_METADATA
