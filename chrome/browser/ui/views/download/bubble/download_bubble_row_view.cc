// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link_fragment.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/notreached.h"
#endif

namespace {

ui::ImageModel GetDefaultIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kInsertDriveFileOutlineIcon, ui::kColorIcon,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE));
}

gfx::Image GetDefaultIconImage(const ui::ColorProvider* color_provider) {
  return gfx::Image(GetDefaultIcon().Rasterize(color_provider));
}

constexpr int kDownloadButtonHeight = 24;
constexpr int kDownloadSubpageIconMargin = 2;
// Padding between elements in the row (except icon and label).
constexpr gfx::Insets kRowInterElementPadding = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr int kProgressBarHeight = 3;
// Num of columns in the table layout, the width of which progress bar will
// span. The 5 columns are Download Icon, Padding, Status text,
// Main Button, Subpage Icon.
constexpr int kNumColumns = 5;

// A stub subclass of Button that has no visuals.
class DownloadBubbleTransparentButton : public views::Button {
  METADATA_HEADER(DownloadBubbleTransparentButton, views::Button)

 public:
  explicit DownloadBubbleTransparentButton(PressedCallback callback,
                                           DownloadBubbleRowView* row_view)
      : Button(std::move(callback)), row_view_(row_view) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    SetInstallFocusRingOnFocus(false);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  }
  ~DownloadBubbleTransparentButton() override = default;

  // Forward dragging and capture loss events, since this class doesn't have
  // enough context to handle them. Let the `DownloadBubbleRowView` manage
  // visual transitions.
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    Button::OnMouseDragged(event);
    return parent()->OnMouseDragged(event);
  }

  void OnMouseCaptureLost() override {
    parent()->OnMouseCaptureLost();
    Button::OnMouseCaptureLost();
  }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    if (reverse) {
      row_view_->UpdateRowForFocus(
          /*visible=*/true, /*request_focus_on_last_quick_action=*/true);
    }
  }

  void NotifyClickForTesting(const ui::Event& event) { NotifyClick(event); }

 private:
  raw_ptr<DownloadBubbleRowView> row_view_;
};

BEGIN_METADATA(DownloadBubbleTransparentButton)
END_METADATA

#if !BUILDFLAG(IS_CHROMEOS)
class DownloadBubbleDeepScanNotice : public views::View {
  METADATA_HEADER(DownloadBubbleDeepScanNotice, views::View)
 public:
  explicit DownloadBubbleDeepScanNotice(base::WeakPtr<Browser> browser) {
    const gfx::Insets insets = GetLayoutInsets(DOWNLOAD_ROW);
    const int icon_label_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL);
    const size_t vertical_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL);

    SetLayoutManager(std::make_unique<views::TableLayout>())
        // Left inset
        ->AddPaddingColumn(views::TableLayout::kFixedSize, insets.left())
        // Download Icon
        .AddColumn(views::LayoutAlignment::kCenter,
                   views::LayoutAlignment::kStart,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        // Download name label (primary_label_)
        .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
        .AddColumn(views::LayoutAlignment::kStart,
                   views::LayoutAlignment::kCenter, 1.0f,
                   views::TableLayout::ColumnSize::kFixed, 0, 0)
        // Right inset
        .AddPaddingColumn(views::TableLayout::kFixedSize, insets.right())
        .AddPaddingRow(1.0, vertical_spacing)
        .AddRows(1, 1.0f)
        .AddPaddingRow(1.0, vertical_spacing);

    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetCanProcessEventsWithinSubtree(false);
    icon_->SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ICON)));

    size_t link_offset;
    std::u16string link_text =
        l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_PROMPT_REMOVAL_NOTICE_LINK);
    std::u16string notice_text = l10n_util::GetStringFUTF16(
        IDS_DEEP_SCANNING_PROMPT_REMOVAL_NOTICE, link_text, &link_offset);
    auto* label = AddChildView(std::make_unique<views::StyledLabel>());
    label->SetText(notice_text);
    label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
    label->SetDefaultTextStyle(views::style::STYLE_BODY_5);
    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            [](base::WeakPtr<Browser> browser) {
              if (!browser) {
                return;
              }
              chrome::ShowSafeBrowsingEnhancedProtection(browser.get());
            },
            browser));
    link_style.text_style = views::style::STYLE_LINK_5;
    label->AddStyleRange(
        gfx::Range{link_offset, link_offset + link_text.length()}, link_style);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded,
                                 /*adjust_height_for_width=*/true));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect bounds = GetLocalBounds();
    // Shift downward to not paint the margin between the notice and row.
    const size_t kVerticalOffset =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL);
    bounds.set_y(bounds.y() + kVerticalOffset);
    bounds.set_height(bounds.height() - kVerticalOffset);
    // Shrink horizontally. We want to be a small offset into the insets
    const size_t kHorizontalOffset =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN);
    gfx::Insets insets = GetLayoutInsets(DOWNLOAD_ROW);
    insets.set_top_bottom(0, 0);
    insets -= gfx::Insets::VH(0, kHorizontalOffset);
    bounds.Inset(insets);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        GetColorProvider()->GetColor(kColorDownloadBubbleInfoBackground));

    const size_t kCornerRadius = 8;
    canvas->DrawRoundRect(bounds, kCornerRadius, flags);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const gfx::VectorIcon& vector_icon = views::kInfoChromeRefreshIcon;
    icon_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icon, ui::kColorSecondaryForeground,
        GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
  }

 private:
  raw_ptr<views::ImageView> icon_;
};

BEGIN_METADATA(DownloadBubbleDeepScanNotice)
END_METADATA
#endif

}  // namespace

void DownloadBubbleRowView::UpdateRow(bool initial_setup) {
  RecordMetricsOnUpdate();
  SetIcon();
  UpdateButtons();
  RecordDownloadDisplayed();
  UpdateLabels();
  UpdateProgressBar();
  UpdateDeepScanNotice();
  if (!update_status_text_timer_.IsRunning()) {
    update_status_text_timer_.Reset();
  }
}

void DownloadBubbleRowView::AddedToWidget() {
  current_scale_ =
      display::Screen::GetScreen()
          ->GetPreferredScaleFactorForView(GetWidget()->GetNativeView())
          .value_or(1.0);
  SetIcon();
  auto* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->AddFocusChangeListener(this);
    RegisterAccelerators(focus_manager);
  }
}

void DownloadBubbleRowView::RemovedFromWidget() {
  auto* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->RemoveFocusChangeListener(this);
    UnregisterAccelerators(focus_manager);
  }
}

void DownloadBubbleRowView::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  current_scale_ = new_device_scale_factor;
  SetIcon();
}

void DownloadBubbleRowView::SetIconFromImageModel(const ui::ImageModel& icon) {
  if (icon.IsEmpty()) {
    icon_->SetImage(GetDefaultIcon());
  } else {
    icon_->SetImage(icon);
  }
}

void DownloadBubbleRowView::SetIconFromImage(gfx::Image icon) {
  SetIconFromImageModel(ui::ImageModel::FromImage(icon));
}

bool DownloadBubbleRowView::StartLoadFileIcon() {
  base::FilePath file_path = info_->model()->GetTargetFilePath();
  // Use a default icon (drive file outline icon) in case we have an empty
  // target path, which is empty for non download offline items, and newly
  // started in-progress downloads.
  if (file_path.empty()) {
    file_icon_ = GetDefaultIconImage(GetColorProvider());
    SetFileIconAsIcon(/*is_default_icon=*/true);
    return true;
  }

  const IconLoader::IconSize icon_loader_size = IconLoader::NORMAL;
  // IconLoader::SMALL returns 16x16 icon and IconLoader::NORMAL returns 20x20
  // icon.
  IconManager* const im = g_browser_process->icon_manager();
  // Can be null in tests.
  if (!im) {
    return false;
  }
  const gfx::Image* const image =
      im->LookupIconFromFilepath(file_path, icon_loader_size, current_scale_);
  if (image && !image->IsEmpty()) {
    OnFileIconLoaded(*image);
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS the LookupIconFromFilepath() call should always succeed.
  NOTREACHED();
#else
  im->LoadIcon(file_path, icon_loader_size, current_scale_,
               base::BindOnce(&DownloadBubbleRowView::OnFileIconLoaded,
                              weak_factory_.GetWeakPtr()),
               &cancelable_task_tracker_);
  return false;
#endif
}

void DownloadBubbleRowView::OnFileIconLoaded(gfx::Image icon) {
  const int icon_size = GetLayoutConstant(DOWNLOAD_ICON_SIZE);
  file_icon_ = ResizedImage(
      icon.IsEmpty() ? GetDefaultIconImage(GetColorProvider()) : icon,
      {icon_size, icon_size});
  // Don't overwrite an override icon from the most recent invocation of
  // SetIcon.
  if (last_overridden_icon_) {
    return;
  }
  SetFileIconAsIcon(/*is_default_icon=*/icon.IsEmpty());
}

void DownloadBubbleRowView::SetFileIconAsIcon(bool is_default_icon) {
  DCHECK(!file_icon_.IsEmpty());
  has_default_icon_ = is_default_icon;
  SetIconFromImage(file_icon_);
}

void DownloadBubbleRowView::SetIcon() {
  // The correct scale_factor is set only in the AddedToWidget()
  if (!GetWidget()) {
    return;
  }

  // Load the file icon unconditionally because it is used for drag-and-drop,
  // even if it is not used as the |icon_|. If this returns true, it set the
  // |icon_| to a |file_icon_|, which may be a filetype icon or a default icon.
  // But if there is an override, it will be re-set below.
  bool file_type_icon_set = StartLoadFileIcon();

  if (info_->icon_override()) {
    last_overridden_icon_ = info_->icon_override();
    has_default_icon_ = false;
    SetIconFromImageModel(ui::ImageModel::FromVectorIcon(
        *info_->icon_override(), info_->secondary_color(),
        GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    return;
  }

  // For downloads in incognito mode.
  if (info_->model()->profile() &&
      info_->model()->profile()->IsIncognitoProfile()) {
    if (last_overridden_icon_ == &kIncognitoIcon) {
      return;
    }
    last_overridden_icon_ = &kIncognitoIcon;
    has_default_icon_ = false;
    SetIconFromImageModel(ui::ImageModel::FromVectorIcon(
        kIncognitoIcon, ui::kColorIcon, GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    return;
  }

  // For downloads in guest sessions.
  if (info_->model()->profile() &&
      info_->model()->profile()->IsGuestSession()) {
    if (last_overridden_icon_ == &kUserAccountAvatarIcon) {
      return;
    }
    last_overridden_icon_ = &kUserAccountAvatarIcon;
    has_default_icon_ = false;
    SetIconFromImageModel(
        profiles::GetGuestAvatar(GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    return;
  }

  last_overridden_icon_ = nullptr;

  if (file_type_icon_set || has_default_icon_) {
    return;
  }
  has_default_icon_ = true;
  // Set the default icon. This may be overwritten with the filetype icon when
  // the IconManager returns a real filetype icon from its lookup.
  SetIconFromImageModel(GetDefaultIcon());
}

DownloadBubbleRowView::~DownloadBubbleRowView() {
  // Explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
  info_->RemoveObserver(this);
}

DownloadBubbleRowView::DownloadBubbleRowView(
    const DownloadBubbleRowViewInfo& info,
    base::WeakPtr<DownloadBubbleUIController> bubble_controller,
    base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
    base::WeakPtr<Browser> browser,
    int fixed_width,
    bool is_in_partial_view)
    : info_(info),
      context_menu_(std::make_unique<DownloadShelfContextMenuView>(
          info_->model()->GetWeakPtr(),
          bubble_controller)),
      bubble_controller_(std::move(bubble_controller)),
      navigation_handler_(std::move(navigation_handler)),
      browser_(std::move(browser)),
      inkdrop_container_(
          AddChildView(std::make_unique<views::InkDropContainerView>())),
      update_status_text_timer_(
          FROM_HERE,
          base::Minutes(1),
          base::BindRepeating(&DownloadBubbleRowView::UpdateStatusText,
                              base::Unretained(this))),
      input_protector_(
          std::make_unique<views::InputEventActivationProtector>()),
      shown_time_(base::Time::Now()),
      is_in_partial_view_(is_in_partial_view),
      fixed_width_(fixed_width) {
  CHECK(info_->model());
  info_->AddObserver(this);
  gfx::Insets insets = GetLayoutInsets(DOWNLOAD_ROW);
  // The DeepScanNotice has a background that extends into the insets on the
  // left and right. To support this, we include vertical insets here, and the
  // left and right inset are manually handled as columns in the table
  // layout. This is temporary until the DeepScanNotice is removed. (Targeting
  // 2024-10)
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(insets.top(), 0, insets.bottom(), 0)));

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  views::InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
  views::InstallRectHighlightPathGenerator(this);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/true,
                                               /*highlight_on_focus=*/true);
  views::InkDrop::Get(this)->SetBaseColorId(kColorDownloadBubbleRowHover);
  views::InkDrop::Get(this)->SetHighlightOpacity(1.0f);

  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  SetLayoutManager(std::make_unique<views::TableLayout>())
      // Left inset
      ->AddColumn(
          views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
          views::TableLayout::kFixedSize,
          views::TableLayout::ColumnSize::kFixed, insets.left(), insets.left())
      // Download Icon
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kStart, views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      // Download name label (primary_label_)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      // Download Buttons: Cancel, Discard, Scan, Open Now, only one may be
      // active
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kStart, views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      // Subpage icon
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kStart, views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      // Right inset
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kFixed, insets.right(),
                 insets.right())
#if BUILDFLAG(IS_CHROMEOS)
      // Three rows, one for name, one for status, one for the progress bar.
      .AddRows(3, 1.0f);
#else
      // Four rows, one for name, one for status, one for the progress bar, and
      // one for the deep scan notice.
      .AddRows(4, 1.0f);
#endif

  inkdrop_container_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  transparent_button_ =
      AddChildView(std::make_unique<DownloadBubbleTransparentButton>(
          base::BindRepeating(&DownloadBubbleRowView::OnMainButtonPressed,
                              base::Unretained(this)),
          this));
  transparent_button_->set_context_menu_controller(this);
  transparent_button_->SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON);
  transparent_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  // Left inset, first row.
  AddChildView(std::make_unique<views::View>());

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetCanProcessEventsWithinSubtree(false);
  icon_->SetBorder(views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ICON)));
  // Make sure the icon is above the inkdrops.
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);
  icon_->SetProperty(views::kTableColAndRowSpanKey, gfx::Size(1, 2));
  const int icon_size = GetLayoutConstant(DOWNLOAD_ICON_SIZE);
  icon_->SetImageSize({icon_size, icon_size});

  primary_label_ = AddChildView(std::make_unique<views::Label>(
      info_->model()->GetFileNameToReportUser().LossyDisplayName(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  primary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  primary_label_->SetCanProcessEventsWithinSubtree(false);
  primary_label_->SetMultiLine(true);
  primary_label_->SetAllowCharacterBreak(true);
  primary_label_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);

  main_button_holder_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  AddMainPageButton(DownloadCommands::CANCEL,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CANCEL));
  AddMainPageButton(DownloadCommands::DISCARD,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_DELETE));
  AddMainPageButton(DownloadCommands::KEEP,
                    l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD));
  AddMainPageButton(DownloadCommands::DEEP_SCAN,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_SCAN));
  AddMainPageButton(DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_NOW));
  AddMainPageButton(DownloadCommands::RESUME,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RESUME));
  AddMainPageButton(DownloadCommands::REVIEW,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_REVIEW));
  AddMainPageButton(DownloadCommands::RETRY,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_RETRY));
  AddMainPageButton(DownloadCommands::OPEN_WHEN_COMPLETE,
                    l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_OPEN_ANYWAY));

  // Note that the addition order of these quick actions matches the visible
  // order, i.e. buttons added first will appear first (left in LTR)
  quick_action_holder_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  // All the quick action buttons are added as children. Later on, only the ones
  // that are enabled will be made visible in UpdateButtons().
  resume_action_ = AddQuickAction(DownloadCommands::RESUME);
  pause_action_ = AddQuickAction(DownloadCommands::PAUSE);
  show_in_folder_action_ = AddQuickAction(DownloadCommands::SHOW_IN_FOLDER);
  cancel_action_ = AddQuickAction(DownloadCommands::CANCEL);
  open_when_complete_action_ =
      AddQuickAction(DownloadCommands::OPEN_WHEN_COMPLETE);
  quick_action_holder_->SetVisible(false);
  quick_action_holder_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  quick_action_holder_->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDialogBackground));

  subpage_icon_holder_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  subpage_icon_holder_->SetCanProcessEventsWithinSubtree(false);
  subpage_icon_ =
      subpage_icon_holder_->AddChildView(std::make_unique<views::ImageView>());
  subpage_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowIcon, ui::kColorIcon));
  subpage_icon_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(kDownloadSubpageIconMargin) + kRowInterElementPadding);
  subpage_icon_->SetVisible(false);
  subpage_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kChevronRightChromeRefreshIcon, ui::kColorIcon,
      GetLayoutConstant(DOWNLOAD_ICON_SIZE)));

  // Right inset, first row.
  AddChildView(std::make_unique<views::View>());

  // Left inset, second row.
  AddChildView(std::make_unique<views::View>());

  // The content of the label will be populated in the `UpdateRow` function.
  secondary_label_ = AddChildView(std::make_unique<views::Label>(
      u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  secondary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // The 2 columns being removed are icon, and padding.
  secondary_label_->SetProperty(views::kTableColAndRowSpanKey,
                                gfx::Size(kNumColumns - 2, 1));
  secondary_label_->SetCanProcessEventsWithinSubtree(false);
  secondary_label_->SetMultiLine(true);
  secondary_label_->SetAllowCharacterBreak(true);
  secondary_label_->SetTextStyle(views::style::STYLE_BODY_5);

  // Right inset, second row.
  AddChildView(std::make_unique<views::View>());

  // Left inset, third row.
  AddChildView(std::make_unique<views::View>());

  // TODO(crbug.com/40875578): Remove the progress bar holder view here.
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
  progress_bar_->SetPreferredHeight(kProgressBarHeight);
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

  // Right inset, third row.
  AddChildView(std::make_unique<views::View>());

  SetNotifyEnterExitOnChild(true);

  // TODO(https://crbug.com/332382747): Remove after 2024-10
#if !BUILDFLAG(IS_CHROMEOS)
  deep_scan_notice_ =
      AddChildView(std::make_unique<DownloadBubbleDeepScanNotice>(browser_));
  deep_scan_notice_->SetProperty(views::kTableColAndRowSpanKey,
                                 gfx::Size(7, 1));
  deep_scan_notice_->SetProperty(views::kTableHorizAlignKey,
                                 views::LayoutAlignment::kStretch);
  deep_scan_notice_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets().set_top(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL))));
  deep_scan_notice_->SetVisible(false);
#endif

  // Set up initial state.
  UpdateRow(/*initial_setup=*/true);
}

views::View::Views DownloadBubbleRowView::GetChildrenInZOrder() {
  auto children = views::View::GetChildrenInZOrder();
  const auto move_child_to_top = [&](View* child) {
    auto it = base::ranges::find(children, child);
    CHECK(it != children.end(), base::NotFatalUntil::M130);
    std::rotate(it, it + 1, children.end());
  };
  move_child_to_top(transparent_button_);
  move_child_to_top(quick_action_holder_);
  move_child_to_top(main_button_holder_);
#if !BUILDFLAG(IS_CHROMEOS)
  move_child_to_top(deep_scan_notice_);
#endif
  return children;
}

bool DownloadBubbleRowView::OnMouseDragged(const ui::MouseEvent& event) {
  // Handle drag (file copy) operations.
  // Drag and drop should only be activated in normal mode.
  if (info_->mode() != download::DownloadItemMode::kNormal) {
    return true;
  }

  if (!drag_start_point_)
    drag_start_point_ = event.location();
  if (!dragging_) {
    dragging_ = ExceededDragThreshold(event.location() - *drag_start_point_);
  } else if ((info_->model()->GetState() == download::DownloadItem::COMPLETE) &&
             info_->model()->GetDownloadItem()) {
    const views::Widget* const widget = GetWidget();
    // In most cases we should either have a |file_icon_| already synchronously
    // or the asynchronous lookup should have finished, but in case it hasn't,
    // ensure that we have a nonempty icon to drag.
    if (file_icon_.IsEmpty()) {
      file_icon_ = GetDefaultIconImage(GetColorProvider());
    }
    // Make this a local to avoid UAF if `this` is deleted during dragging.
    std::unique_ptr<DownloadBubbleNavigationHandler::CloseOnDeactivatePin>
        download_dragging_pin;
    if (navigation_handler_) {
      download_dragging_pin =
          navigation_handler_->PreventDialogCloseOnDeactivate();
    }
    DragDownloadItem(info_->model()->GetDownloadItem(), &file_icon_,
                     widget ? widget->GetNativeView() : nullptr);
    // DragDownloadItem returns when the drag is over.
    // `this` may be deleted by now!
  }
  return true;
}

void DownloadBubbleRowView::OnMouseEntered(const ui::MouseEvent& event) {
  View::OnMouseEntered(event);
  UpdateRowForHover(/*hovered=*/true);
}

void DownloadBubbleRowView::OnMouseExited(const ui::MouseEvent& event) {
  View::OnMouseExited(event);
  UpdateRowForHover(/*hovered=*/false);
}

void DownloadBubbleRowView::OnMouseCaptureLost() {
  // Drag and drop should only be activated in normal mode.
  if (info_->mode() != download::DownloadItemMode::kNormal) {
    return;
  }

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    drag_start_point_.reset();
  }
}

gfx::Size DownloadBubbleRowView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  return {fixed_width_,
          GetLayoutManager()->GetPreferredHeightForWidth(this, fixed_width_)};
}

void DownloadBubbleRowView::AddLayerToRegion(ui::Layer* layer,
                                             views::LayerRegion region) {
  inkdrop_container_->AddLayerToRegion(layer, region);
}

void DownloadBubbleRowView::RemoveLayerFromRegions(ui::Layer* layer) {
  inkdrop_container_->RemoveLayerFromRegions(layer);
}

void DownloadBubbleRowView::VisibilityChanged(views::View* starting_from,
                                              bool is_visible) {
  views::View::VisibilityChanged(starting_from, is_visible);
  input_protector_->VisibilityChanged(is_visible);
}

void DownloadBubbleRowView::OnWillChangeFocus(views::View* before,
                                              views::View* now) {
  if (now) {
    UpdateRowForFocus(/*visible=*/Contains(now),
                      /*request_focus_on_last_quick_action=*/false);
  }
}

void DownloadBubbleRowView::UpdateRowForHover(bool hovered) {
  quick_action_holder_->SetVisible(hovered);
}

void DownloadBubbleRowView::UpdateRowForFocus(
    bool visible,
    bool request_focus_on_last_quick_action) {
  quick_action_holder_->SetVisible(visible);
  views::InkDrop::Get(this)->GetInkDrop()->SetFocused(visible);
  // Update focus only if focus received from a different row.
  bool should_set_focus = request_focus_on_last_quick_action &&
                          GetFocusManager() &&
                          !Contains(GetFocusManager()->GetFocusedView());
  if (should_set_focus && !info_->quick_actions().empty()) {
    GetActionButtonForCommand(info_->quick_actions().back().command)
        ->RequestFocus();
  }
}

void DownloadBubbleRowView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  transparent_button_->SetBoundsRect(GetLocalBounds());
  gfx::Size quick_actions_size = quick_action_holder_->GetPreferredSize();
  gfx::Insets insets = GetLayoutInsets(DOWNLOAD_ROW);
  quick_action_holder_->SetBoundsRect(
      gfx::Rect(gfx::Point(GetLocalBounds().width() -
                               quick_actions_size.width() - insets.right(),
                           insets.top()),
                quick_actions_size));
  inkdrop_container_->SetBoundsRect(GetLocalBounds());
}

void DownloadBubbleRowView::OnMainButtonPressed(const ui::Event& event) {
  if (!bubble_controller_ || !navigation_handler_ ||
      !info_->main_button_enabled() || !info_->model()) {
    return;
  }
  // Log histograms for how long users take to open a download by clicking the
  // main button, if the download has no warning. This is logged before the
  // input_protector_ check to capture attempted clicks sooner than 500 ms.
  if (!info_->has_subpage() && is_in_partial_view_) {
    base::Time now = base::Time::Now();
    base::UmaHistogramTimes("Download.PartialView.StartTimeToOpenDownloadClick",
                            now - model()->GetStartTime());
    base::UmaHistogramTimes(
        "Download.PartialView.RowShownTimeToOpenDownloadClick",
        now - shown_time_);
  }
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }
  if (info_->has_subpage()) {
    DownloadItemWarningData::AddWarningActionEvent(
        info_->model()->GetDownloadItem(),
        DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
        DownloadItemWarningData::WarningAction::OPEN_SUBPAGE);
    navigation_handler_->OpenSecurityDialog(info_->model()->GetContentId());
  } else {
    info_->model()->OpenDownload();
  }
}

void DownloadBubbleRowView::OnActionButtonPressed(
    DownloadCommands::Command command,
    const ui::Event& event) {
  if (!bubble_controller_ || !info_->model() ||
      input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }
  bubble_controller_->ProcessDownloadButtonPress(info_->model()->GetWeakPtr(),
                                                 command,
                                                 /*is_main_view=*/true);
}

void DownloadBubbleRowView::UpdateButtons() {
  resume_action_->SetVisible(false);
  pause_action_->SetVisible(false);
  open_when_complete_action_->SetVisible(false);
  cancel_action_->SetVisible(false);
  show_in_folder_action_->SetVisible(false);

  for (const auto& action : info_->quick_actions()) {
    if (!DownloadCommands(info_->model()->GetWeakPtr())
             .IsCommandEnabled(action.command)) {
      continue;
    }
    views::ImageButton* action_button =
        GetActionButtonForCommand(action.command);
    action_button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*(action.icon), ui::kColorIcon,
                                       GetLayoutConstant(DOWNLOAD_ICON_SIZE)));
    action_button->GetViewAccessibility().SetName(
        GetAccessibleNameForQuickAction(action.command));
    action_button->SetTooltipText(action.hover_text);
    action_button->SetVisible(true);
  }

  for (const auto& [command, button] : main_page_buttons_) {
    button->SetVisible(false);
  }

  if (info_->primary_button_command()) {
    views::MdTextButton* main_button =
        main_page_buttons_[*info_->primary_button_command()];
    main_button->GetViewAccessibility().SetName(
        GetAccessibleNameForMainPageButton(
            info_->primary_button_command().value()));
    main_button->SetVisible(true);
  }

  subpage_icon_->SetVisible(info_->has_subpage());
}

void DownloadBubbleRowView::UpdateProgressBar() {
  if (!navigation_handler_) {
    return;
  }
  if (info_->has_progress_bar()) {
    if (!progress_bar_->GetVisible()) {
      progress_bar_->SetVisible(true);
    }
    progress_bar_->SetValue(
        info_->is_progress_bar_looping()
            ? -1
            : static_cast<double>(info_->model()->PercentComplete()) / 100);
    progress_bar_->SetPaused(info_->model()->IsPaused());
  } else if (progress_bar_->GetVisible()) {
    // Hide the progress bar.
    progress_bar_->SetVisible(false);
  }
}

void DownloadBubbleRowView::UpdateLabels() {
  primary_label_->SetText(
      info_->model()->GetFileNameToReportUser().LossyDisplayName());
  UpdateStatusText();

  if (info_->has_subpage()) {
    transparent_button_->GetViewAccessibility().SetName(
        l10n_util::GetStringFUTF16(IDS_DOWNLOAD_BUBBLE_MAIN_BUTTON_SUBPAGE,
                                   primary_label_->GetText(),
                                   secondary_label_->GetText()));
  } else {
    transparent_button_->GetViewAccessibility().SetName(base::JoinString(
        {primary_label_->GetText(), secondary_label_->GetText()}, u" "));
  }

  secondary_label_->SetEnabledColorId(info_->secondary_text_color());
}

void DownloadBubbleRowView::UpdateDeepScanNotice() {
  if (info_->ShouldShowDeepScanNotice()) {
#if !BUILDFLAG(IS_CHROMEOS)
    deep_scan_notice_->SetVisible(true);
    bubble_controller_->SetDeepScanNoticeSeen();
#endif
  }
}

void DownloadBubbleRowView::RecordMetricsOnUpdate() {
  // This should only be logged once per download.
  MaybeRecordDangerousDownloadWarningShown(*info_->model());
  if (!has_download_completion_been_logged_ &&
      info_->model()->GetState() == download::DownloadItem::COMPLETE) {
    has_download_completion_been_logged_ = true;
  }
}

void DownloadBubbleRowView::RecordDownloadDisplayed() {
  if (info_->model()->IsDangerous()) {
    DownloadItemWarningData::AddWarningActionEvent(
        info_->model()->GetDownloadItem(),
        DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
        DownloadItemWarningData::WarningAction::SHOWN);
  }
  if (!info_->model()->GetEphemeralWarningUiShownTime().has_value() &&
      info_->model()->IsEphemeralWarning()) {
    info_->model()->SetEphemeralWarningUiShownTime(base::Time::Now());
    if (bubble_controller_) {
      bubble_controller_->ScheduleCancelForEphemeralWarning(
          info_->model()->GetDownloadItem()->GetGuid());
    }
  }
}

void DownloadBubbleRowView::AddMainPageButton(
    DownloadCommands::Command command,
    const std::u16string& button_string) {
  // Unretained is safe because this owns and outlives the main page button.
  views::MdTextButton* button =
      main_button_holder_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&DownloadBubbleRowView::OnActionButtonPressed,
                              base::Unretained(this), command),
          button_string));
  button->SetMaxSize(gfx::Size(0, kDownloadButtonHeight));
  button->SetProperty(views::kMarginsKey, kRowInterElementPadding);
  button->SetVisible(false);
  button->SetStyle(ui::ButtonStyle::kText);

  main_page_buttons_[command] = button;
}

views::ImageButton* DownloadBubbleRowView::AddQuickAction(
    DownloadCommands::Command command) {
  // Unretained is safe because this owns and outlives the quick action button.
  views::ImageButton* quick_action =
      quick_action_holder_->AddChildView(views::CreateVectorImageButton(
          base::BindRepeating(&DownloadBubbleRowView::OnActionButtonPressed,
                              base::Unretained(this), command)));
  InstallCircleHighlightPathGenerator(quick_action);
  quick_action->SetBorder(
      views::CreateEmptyBorder(GetLayoutInsets(DOWNLOAD_ICON)));
  quick_action->SetProperty(views::kMarginsKey, kRowInterElementPadding);
  quick_action->SetVisible(false);
  views::InkDrop::Get(quick_action)
      ->SetBaseColorId(views::TypographyProvider::Get().GetColorId(
          views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY));
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

std::u16string DownloadBubbleRowView::GetAccessibleNameForQuickAction(
    DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::RESUME:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_RESUME_QUICK_ACTION_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::PAUSE:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_PAUSE_QUICK_ACTION_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_OPEN_QUICK_ACTION_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::CANCEL:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_CANCEL_QUICK_ACTION_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::SHOW_IN_FOLDER:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_SHOW_IN_FOLDER_QUICK_ACTION_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    default:
      NOTREACHED();
  }
}

std::u16string DownloadBubbleRowView::GetAccessibleNameForMainPageButton(
    DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::CANCEL:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_CANCEL_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::RESUME:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_RESUME_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::DISCARD:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_DELETE_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::KEEP:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_KEEP_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::DEEP_SCAN:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_SCAN_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_OPEN_NOW_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::REVIEW:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_REVIEW_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::RETRY:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_RETRY_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      return l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_BUBBLE_OPEN_MAIN_BUTTON_ACCESSIBILITY,
          info_->model()->GetFileNameToReportUser().LossyDisplayName());
    default:
      NOTREACHED();
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

void DownloadBubbleRowView::UpdateStatusText() {
  secondary_label_->SetText(info_->model()->GetStatusTextForLabel(
      secondary_label_->font_list(), secondary_label_->width()));
}

bool DownloadBubbleRowView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (info_->model()->GetState() != download::DownloadItem::COMPLETE) {
    return false;
  }

  // The only accelerator we registered is for copy, so we know that's what
  // `accelerator` contains. If DCHECKs are enabled, we can confirm that.
#if DCHECK_IS_ON()
  if (browser_) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser_.get());
    ui::Accelerator registered_accelerator;
    if (browser_view &&
        browser_view->GetAccelerator(IDC_COPY, &registered_accelerator)) {
      DCHECK(accelerator == registered_accelerator);
    }
  }
#endif

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  std::string uri_list = ui::FileInfosToURIList(
      {ui::FileInfo(info_->model()->GetTargetFilePath(),
                    info_->model()->GetFileNameToReportUser())});
  scw.WriteFilenames(uri_list);
  return true;
}

bool DownloadBubbleRowView::CanHandleAccelerators() const {
  bool focused = Contains(GetFocusManager()->GetFocusedView());
  return focused;
}

const std::u16string& DownloadBubbleRowView::GetSecondaryLabelTextForTesting() {
  return secondary_label_->GetText();
}

void DownloadBubbleRowView::RegisterAccelerators(
    views::FocusManager* focus_manager) {
  if (!browser_) {
    return;
  }
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_.get());
  if (!browser_view) {
    return;
  }

  ui::Accelerator accelerator;
  if (!browser_view->GetAccelerator(IDC_COPY, &accelerator)) {
    return;
  }

  focus_manager->RegisterAccelerator(
      accelerator, ui::AcceleratorManager::kNormalPriority, this);
}

void DownloadBubbleRowView::UnregisterAccelerators(
    views::FocusManager* focus_manager) {
  if (!browser_) {
    return;
  }
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_.get());
  if (!browser_view) {
    return;
  }

  ui::Accelerator accelerator;
  if (!browser_view->GetAccelerator(IDC_COPY, &accelerator)) {
    return;
  }

  focus_manager->UnregisterAccelerator(accelerator, this);
}

void DownloadBubbleRowView::OnInfoChanged() {
  if (!navigation_handler_) {
    return;
  }
  UpdateRow(/*initial_setup=*/false);
  // Resize is needed because the height of the row can change when the text
  // (primary_label_ or secondary_label_) is updated.
  PreferredSizeChanged();
}

void DownloadBubbleRowView::SimulateMainButtonClickForTesting(
    const ui::Event& event) {
  static_cast<DownloadBubbleTransparentButton*>(transparent_button_)
      ->NotifyClickForTesting(event);  // IN-TEST
}

bool DownloadBubbleRowView::IsQuickActionButtonVisibleForTesting(
    DownloadCommands::Command command) {
  views::ImageButton* button = GetActionButtonForCommand(command);
  if (!button) {
    return false;
  }
  return button->GetVisible();
}

views::ImageButton* DownloadBubbleRowView::GetQuickActionButtonForTesting(
    DownloadCommands::Command command) {
  return GetActionButtonForCommand(command);
}

void DownloadBubbleRowView::SetInputProtectorForTesting(
    std::unique_ptr<views::InputEventActivationProtector> input_protector) {
  input_protector_ = std::move(input_protector);
}

views::View* DownloadBubbleRowView::TargetForRect(View* root,
                                                  const gfx::Rect& rect) {
  views::View* target = views::ViewTargeterDelegate::TargetForRect(root, rect);
#if !BUILDFLAG(IS_CHROMEOS)
  // The deep scan notice is on top of the transparent button to make the link
  // clickable, but we want to target the button for all other input events.
  if (views::IsViewClass<DownloadBubbleDeepScanNotice>(target) ||
      views::IsViewClass<DownloadBubbleDeepScanNotice>(target->parent())) {
    return transparent_button_;
  }
#endif

  return target;
}

BEGIN_METADATA(DownloadBubbleRowView)
END_METADATA
