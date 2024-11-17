// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"

#include <string>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_started_animation_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/progress_ring_utils.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

using offline_items_collection::ContentId;

using GetBadgeTextCallback = base::RepeatingCallback<gfx::RenderText&()>;

constexpr int kProgressRingRadius = 9;
constexpr int kProgressRingRadiusTouchMode = 12;
constexpr float kProgressRingStrokeWidth = 2.0f;

// Close the partial bubble after 5 seconds if the user doesn't interact with
// it.
constexpr base::TimeDelta kAutoClosePartialViewDelay = base::Seconds(5);

// Helper class to draw a circular badge with text.
class CircleBadgeImageSource : public gfx::CanvasImageSource {
 public:
  CircleBadgeImageSource(const gfx::Size& size,
                         SkColor background_color,
                         GetBadgeTextCallback get_text_callback)
      : gfx::CanvasImageSource(size),
        background_color_(background_color),
        get_text_callback_(std::move(get_text_callback)) {}

  CircleBadgeImageSource(const CircleBadgeImageSource&) = delete;
  CircleBadgeImageSource& operator=(const CircleBadgeImageSource&) = delete;

  ~CircleBadgeImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(background_color_);

    gfx::RenderText& render_text = get_text_callback_.Run();
    const gfx::Rect& badge_rect = render_text.display_rect();
    // Set the corner radius to make the rectangle appear like a circle.
    const int corner_radius = badge_rect.height() / 2;
    canvas->DrawRoundRect(badge_rect, corner_radius, flags);
    render_text.Draw(canvas);
  }

 private:
  const SkColor background_color_;
  GetBadgeTextCallback get_text_callback_;
};

gfx::Insets GetPrimaryViewMargin() {
  return gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_VERTICAL),
                         0);
}

gfx::Insets GetSecurityViewMargin() {
  return gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_VERTICAL),
                         0);
}
}  // namespace

DownloadToolbarButtonView::DownloadToolbarButtonView(BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&DownloadToolbarButtonView::ButtonPressed,
                              base::Unretained(this))),
      browser_(browser_view->browser()),
      auto_close_bubble_timer_(
          FROM_HERE,
          kAutoClosePartialViewDelay,
          base::BindRepeating(&DownloadToolbarButtonView::AutoClosePartialView,
                              base::Unretained(this))) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetVectorIcons(kDownloadToolbarButtonChromeRefreshIcon,
                 kDownloadToolbarButtonIcon);
  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kDialog);
  tooltip_texts_[0] = l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON);
  SetTooltipText(tooltip_texts_.at(0));
  SetVisible(false);
  SetProperty(views::kElementIdentifierKey, kToolbarDownloadButtonElementId);

  badge_image_view_ = AddChildView(std::make_unique<views::ImageView>());
  badge_image_view_->SetPaintToLayer();
  badge_image_view_->layer()->SetFillsBoundsOpaquely(false);
  badge_image_view_->SetCanProcessEventsWithinSubtree(false);

  scanning_animation_.SetSlideDuration(base::Milliseconds(2500));
  scanning_animation_.SetTweenType(gfx::Tween::LINEAR);

  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(browser_);

  BrowserList::GetInstance()->AddObserver(this);

  // Wait until we're done with everything else before creating `controller_`
  // since it can call `Show()` synchronously.
  controller_ = std::make_unique<DownloadDisplayController>(
      this, browser_, bubble_controller_.get());
}

DownloadToolbarButtonView::~DownloadToolbarButtonView() {
  BrowserList::GetInstance()->RemoveObserver(this);
  controller_.reset();
  bubble_controller_.reset();
}

ui::ImageModel DownloadToolbarButtonView::GetBadgeImage(
    bool is_active,
    int progress_download_count,
    SkColor badge_text_color,
    SkColor badge_background_color) {
  // Only display the badge if there are multiple downloads.
  if (!is_active || progress_download_count < 2) {
    return ui::ImageModel();
  }
  const int badge_height = badge_image_view_->bounds().height();
  // base::Unretained is safe because this owns the ImageView to which the
  // image source is applied.
  return ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<CircleBadgeImageSource>(
          gfx::Size(badge_height, badge_height), badge_background_color,
          base::BindRepeating(&DownloadToolbarButtonView::GetBadgeText,
                              base::Unretained(this), progress_download_count,
                              badge_text_color)));
}

gfx::RenderText& DownloadToolbarButtonView::GetBadgeText(
    int progress_download_count,
    SkColor badge_text_color) {
  CHECK_GE(progress_download_count, 2);
  const int badge_height = badge_image_view_->bounds().height();
  bool use_placeholder = progress_download_count > kMaxDownloadCountDisplayed;
  const int index = use_placeholder ? 0 : progress_download_count - 1;
  gfx::RenderText* render_text = render_texts_.at(index).get();
  if (render_text == nullptr) {
    ui::ResourceBundle* bundle = &ui::ResourceBundle::GetSharedInstance();
    gfx::FontList font = bundle->GetFontList(ui::ResourceBundle::BaseFont)
                             .DeriveWithHeightUpperBound(badge_height);
    std::u16string text =
        use_placeholder
            ? base::StrCat(
                  {base::FormatNumber(kMaxDownloadCountDisplayed), u"+"})
            : base::FormatNumber(progress_download_count);

    std::unique_ptr<gfx::RenderText> new_render_text =
        gfx::RenderText::CreateRenderText();
    new_render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    new_render_text->SetCursorEnabled(false);
    new_render_text->SetFontList(std::move(font));
    new_render_text->SetText(std::move(text));
    new_render_text->SetDisplayRect(
        gfx::Rect(gfx::Point(), gfx::Size(badge_height, badge_height)));

    render_text = new_render_text.get();
    render_texts_[index] = std::move(new_render_text);
  }
  render_text->SetColor(badge_text_color);
  return *render_text;
}

bool DownloadToolbarButtonView::ShouldShowScanningAnimation() const {
  return !is_dormant_ && (state_ == IconState::kDeepScanning ||
                          !progress_info_.progress_certain);
}

void DownloadToolbarButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  redraw_progress_soon_ = false;

  // Do not show the progress ring when there is no in progress download.
  if (state_ == IconState::kComplete || progress_info_.download_count == 0) {
    if (scanning_animation_.is_animating()) {
      scanning_animation_.End();
    }
    return;
  }

  bool is_disabled = GetVisualState() == Button::STATE_DISABLED;
  bool is_active = active_ == IconActive::kActive;
  SkColor background_color =
      is_disabled ? GetForegroundColor(ButtonState::STATE_DISABLED)
                  : GetColorProvider()->GetColor(
                        kColorDownloadToolbarButtonRingBackground);
  SkColor progress_color = GetProgressColor(is_disabled, is_active);

  int ring_radius = ui::TouchUiController::Get()->touch_ui()
                        ? kProgressRingRadiusTouchMode
                        : kProgressRingRadius;
  int x = width() / 2 - ring_radius;
  int y = height() / 2 - ring_radius;
  int diameter = 2 * ring_radius;
  gfx::RectF ring_bounds(x, y, /*width=*/diameter, /*height=*/diameter);

  if (is_dormant_) {
    // Draw a static solid ring.
    views::DrawProgressRing(canvas, gfx::RectFToSkRect(ring_bounds),
                            background_color, background_color,
                            kProgressRingStrokeWidth,
                            /*start_angle=*/0,
                            /*sweep_angle=*/0);
    return;
  }

  if (ShouldShowScanningAnimation()) {
    if (!scanning_animation_.is_animating()) {
      scanning_animation_.Reset();
      scanning_animation_.Show();
    }
    views::DrawSpinningRing(canvas, gfx::RectFToSkRect(ring_bounds),
                            background_color, progress_color,
                            kProgressRingStrokeWidth, /*start_angle=*/
                            gfx::Tween::IntValueBetween(
                                scanning_animation_.GetCurrentValue(), 0, 360));
    return;
  }

  views::DrawProgressRing(
      canvas, gfx::RectFToSkRect(ring_bounds), background_color, progress_color,
      kProgressRingStrokeWidth, /*start_angle=*/-90,
      /*sweep_angle=*/360 * progress_info_.progress_percentage / 100.0);
}

void DownloadToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();
}

void DownloadToolbarButtonView::Hide() {
  HideDetails();
  SetVisible(false);
  PreferredSizeChanged();
}

bool DownloadToolbarButtonView::IsShowing() const {
  return GetVisible();
}

void DownloadToolbarButtonView::Enable() {
  SetEnabled(true);
}

void DownloadToolbarButtonView::Disable() {
  SetEnabled(false);
}

void DownloadToolbarButtonView::UpdateDownloadIcon(
    const IconUpdateInfo& updates) {
  // Whether to update the icon after processing any changes.
  bool update_icon = false;

  if (updates.show_animation && show_download_started_animation_) {
    has_pending_download_started_animation_ = true;
    // Invalidate the layout to show the animation in Layout().
    PreferredSizeChanged();
  }
  if (updates.new_state && *updates.new_state != state_) {
    update_icon = true;
    state_ = *updates.new_state;
  }
  if (updates.new_active && *updates.new_active != active_) {
    update_icon = true;
    active_ = *updates.new_active;
  }

  if (updates.new_progress) {
    const ProgressInfo& new_progress = *updates.new_progress;
    // Only change the icon if the download count or progress certainty have
    // changed. If only the percentage changed, the icon itself doesn't
    // necessarily need to change; the ring change is captured by possibly
    // scheduling a paint.
    if (!new_progress.FieldsEqualExceptPercentage(progress_info_)) {
      update_icon = true;
    }

    // Schedule a paint when we hit 0 downloads, even if this button is
    // dormant. This will clear the ring. This is needed to avoid a ring being
    // left over on a dormant button when going from >0 to 0 downloads.
    if (new_progress.download_count == 0 && progress_info_.download_count > 0) {
      redraw_progress_soon_ = true;
    }

    if (!is_dormant_ && new_progress.progress_percentage !=
                            progress_info_.progress_percentage) {
      redraw_progress_soon_ = true;
    }
    progress_info_ = new_progress;
  }
  // We need to redraw the ring constantly while the scanning animation is
  // running.
  if (ShouldShowScanningAnimation()) {
    redraw_progress_soon_ = true;
  }

  if (redraw_progress_soon_ || update_icon) {
    UpdateIcon();
  }
}

void DownloadToolbarButtonView::AnnounceAccessibleAlertNow(
    const std::u16string& alert_text) {
  GetViewAccessibility().AnnounceText(alert_text);
}

bool DownloadToolbarButtonView::IsFullscreenWithParentViewHidden() const {
#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser_)) {
    return true;
  }
#endif

  // If immersive fullscreen, check if top chrome is visible.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (browser_view && browser_view->GetLocationBarView() &&
      browser_view->IsImmersiveModeEnabled()) {
    return !browser_view->immersive_mode_controller()->IsRevealed();
  }

  // Handle the remaining fullscreen case.
  return browser_->window() && browser_->window()->IsFullscreen() &&
         !browser_->window()->IsToolbarVisible();
}

bool DownloadToolbarButtonView::ShouldShowExclusiveAccessBubble() const {
  if (!IsFullscreenWithParentViewHidden()) {
    return false;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsKioskSession()) {
    return false;
  }
#endif
  return !browser_view->IsImmersiveModeEnabled() &&
         browser_view->CanUserExitFullscreen();
}

void DownloadToolbarButtonView::OpenSecuritySubpage(
    const offline_items_collection::ContentId& id) {
  OpenSecurityDialog(id);
}

// This function shows the partial view. If the main view is already showing,
// we do not show the partial view. If the partial view is already showing,
// there is nothing to do here, the controller should update the partial view.
void DownloadToolbarButtonView::ShowDetails() {
  if (bubble_delegate_) {
    return;
  }
  is_primary_partial_view_ = true;
  if (use_auto_close_bubble_timer_) {
    auto_close_bubble_timer_.Reset();
  }
  CreateBubbleDialogDelegate();
}

bool DownloadToolbarButtonView::OpenMostSpecificDialog(
    const offline_items_collection::ContentId& content_id) {
  if (!IsShowing()) {
    Show();
  }

  if (!bubble_delegate_) {
    // This should behave similarly to a normal button press on the toolbar
    // button, so create the main view.
    is_primary_partial_view_ = false;
    CreateBubbleDialogDelegate();
  }

  DownloadBubbleRowView* row = ShowPrimaryDialogRow(content_id);

  // Open the more specific security subpage if it has one.
  if (row && row->info().has_subpage()) {
    OpenSecurityDialog(content_id);
  }
  return row != nullptr;
}

void DownloadToolbarButtonView::HideDetails() {
  CloseDialog(views::Widget::ClosedReason::kUnspecified);
}

bool DownloadToolbarButtonView::IsShowingDetails() const {
  return bubble_delegate_ != nullptr &&
         bubble_delegate_->GetWidget()->IsVisible();
}

void DownloadToolbarButtonView::UpdateIcon() {
  if (!GetWidget()) {
    return;
  }

  // Schedule paint to update the progress ring.
  if (redraw_progress_soon_) {
    SchedulePaint();
  }

  const gfx::VectorIcon* new_icon;
  SkColor icon_color = GetIconColor();
  bool is_touch_mode = ui::TouchUiController::Get()->touch_ui();
  if (state_ == IconState::kProgress || state_ == IconState::kDeepScanning) {
    new_icon = is_touch_mode ? &kDownloadInProgressTouchIcon
                             : &kDownloadInProgressChromeRefreshIcon;
  } else {
    new_icon = is_touch_mode ? &kDownloadToolbarButtonTouchIcon
                             : &kDownloadToolbarButtonChromeRefreshIcon;
  }

  SetImageModel(ButtonState::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
  SetImageModel(ButtonState::STATE_HOVERED,
                ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
  SetImageModel(ButtonState::STATE_PRESSED,
                ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
  SetImageModel(
      Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(
          *new_icon, GetForegroundColor(ButtonState::STATE_DISABLED)));

  int progress_download_count = progress_info_.download_count;
  bool is_disabled = GetVisualState() == Button::STATE_DISABLED || is_dormant_;
  bool is_active = active_ == IconActive::kActive;
  badge_image_view_->SetImage(
      GetBadgeImage(is_active, progress_download_count,
                    GetProgressColor(is_disabled, is_active),
                    GetColorProvider()->GetColor(kColorToolbar)));

  // Update the toolbar button's tooltip.
  std::u16string& tooltip_for_progress_count =
      tooltip_texts_[progress_download_count];
  if (tooltip_for_progress_count.empty()) {
    // We already initialized the text for 0 downloads in the constructor.
    CHECK_GT(progress_download_count, 0);
    // "1 download in progress" or "N downloads in progress".
    tooltip_for_progress_count = l10n_util::GetPluralStringFUTF16(
        IDS_DOWNLOAD_BUBBLE_TOOLTIP_IN_PROGRESS_COUNT, progress_download_count);
  }
  SetTooltipText(tooltip_texts_.at(progress_download_count));
}

void DownloadToolbarButtonView::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);
  gfx::Size size = GetPreferredSize();
  // Badge width and height are the same.
  const int badge_height = std::min(size.width(), size.height()) / 2;
  const int badge_offset_x = size.width() - badge_height;
  const int badge_offset_y = size.height() - badge_height;
  // If the badge height has changed, clear the cache of render_texts_.
  if (badge_height != badge_image_view_->bounds().height()) {
    render_texts_ = std::array<std::unique_ptr<gfx::RenderText>,
                               kMaxDownloadCountDisplayed>{};
  }
  badge_image_view_->SetBoundsRect(
      gfx::Rect(badge_offset_x, badge_offset_y, badge_height, badge_height));

  // If there is a pending animation, show it now after we have laid out the
  // view properly.
  ShowPendingDownloadStartedAnimation();
}

bool DownloadToolbarButtonView::ShouldShowInkdropAfterIphInteraction() {
  return false;
}

std::vector<DownloadUIModel::DownloadUIModelPtr>
DownloadToolbarButtonView::GetPrimaryViewModels() {
  return is_primary_partial_view_ ? bubble_controller_->GetPartialView()
                                  : bubble_controller_->GetMainView();
}

void DownloadToolbarButtonView::OpenPrimaryDialog() {
  ShowPrimaryDialogRow(std::nullopt);
}

DownloadBubbleRowView* DownloadToolbarButtonView::ShowPrimaryDialogRow(
    std::optional<ContentId> content_id) {
  if (!bubble_delegate_) {
    return nullptr;
  }
  DownloadBubbleRowView* row = bubble_contents_->ShowPrimaryPage(content_id);
  bubble_delegate_->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->set_margins(GetPrimaryViewMargin());
  return row;
}

void DownloadToolbarButtonView::OpenSecurityDialog(
    const ContentId& content_id) {
  if (!bubble_delegate_) {
    is_primary_partial_view_ = false;
    CreateBubbleDialogDelegate();
  }
  bubble_contents_->ShowSecurityPage(content_id);
  bubble_delegate_->set_margins(GetSecurityViewMargin());
}

void DownloadToolbarButtonView::CloseDialog(
    views::Widget::ClosedReason reason) {
  if (bubble_delegate_) {
    bubble_delegate_->GetWidget()->CloseWithReason(reason);
  }
}

void DownloadToolbarButtonView::OnDialogInteracted() {
  DeactivateAutoClose();
}

void DownloadToolbarButtonView::OnSecurityDialogButtonPress(
    const DownloadUIModel& model,
    DownloadCommands::Command command) {
  if (model.GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT &&
      command == DownloadCommands::DISCARD) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DownloadToolbarButtonView::ShowIphPromo,
                                  weak_factory_.GetWeakPtr()));
  }
}

base::WeakPtr<DownloadBubbleNavigationHandler>
DownloadToolbarButtonView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DownloadToolbarButtonView::OnBubbleClosing() {
  immersive_revealed_lock_.reset();
  bubble_delegate_ = nullptr;
  bubble_contents_ = nullptr;
  bubble_closer_.reset();
  UpdateIconDormant();
}

std::unique_ptr<DownloadBubbleNavigationHandler::CloseOnDeactivatePin>
DownloadToolbarButtonView::PreventDialogCloseOnDeactivate() {
  if (!bubble_delegate_) {
    return nullptr;
  }
  return bubble_delegate_->PreventCloseOnDeactivate();
}

void DownloadToolbarButtonView::CreateBubbleDialogDelegate() {
  std::vector<DownloadUIModel::DownloadUIModelPtr> primary_view_models =
      GetPrimaryViewModels();
  if (primary_view_models.empty()) {
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  // If we are in immersive fullscreen, reveal the toolbar to show the bubble.
  if (browser_view && browser_view->immersive_mode_controller()) {
    immersive_revealed_lock_ =
        browser_view->immersive_mode_controller()->GetRevealedLock(
            ImmersiveModeController::ANIMATE_REVEAL_YES);
  }

  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      this, views::BubbleBorder::TOP_RIGHT, views::BubbleBorder::DIALOG_SHADOW,
      /*autosize=*/true);
  bubble_delegate->SetTitle(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_HEADER_LABEL));
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->set_internal_name(kBubbleName);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->RegisterWindowClosingCallback(base::BindOnce(
      &DownloadToolbarButtonView::OnBubbleClosing, weak_factory_.GetWeakPtr()));
  auto bubble_contents = std::make_unique<DownloadBubbleContentsView>(
      browser_->AsWeakPtr(), bubble_controller_->GetWeakPtr(), GetWeakPtr(),
      is_primary_partial_view_,
      std::make_unique<DownloadBubbleContentsViewInfo>(
          std::move(primary_view_models)),
      bubble_delegate.get());
  bubble_contents_ = bubble_contents.get();
  bubble_delegate->SetContentsView(std::move(bubble_contents));
  // The contents view displays the primary view by default.
  bubble_delegate->set_margins(GetPrimaryViewMargin());
  bubble_delegate->SetEnableArrowKeyTraversal(true);
  bubble_delegate_ = bubble_delegate.get();
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));

  if (!is_primary_partial_view_ && !button_click_time_.is_null()) {
    // If the main view was shown after clicking on the toolbar button,
    // record the time from click to shown. (The main view can be shown without
    // clicking the toolbar button, e.g. from clicking on a notification.)
    bubble_delegate_->GetWidget()
        ->GetCompositor()
        ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
            [](base::TimeTicks click_time,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_time =
                  frame_timing_details.presentation_feedback.timestamp;
              UmaHistogramTimes(
                  "Download.Bubble.ToolbarButtonClickToFullViewShownLatency",
                  presentation_time - click_time);
            },
            button_click_time_));
    // Reset click time.
    button_click_time_ = base::TimeTicks();
  }

  CloseAutofillPopup();
  if (ShouldShowBubbleAsInactive()) {
    bubble_delegate_->GetWidget()->ShowInactive();
    bubble_closer_ = std::make_unique<BubbleCloser>(this);
    bubble_delegate_->GetWidget()
        ->GetRootView()
        ->GetViewAccessibility()
        .AnnounceText(
            l10n_util::GetStringUTF16(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
  } else {
    bubble_delegate_->GetWidget()->Show();
  }

  // For IPH bubble. The IPH should show when the partial view is closed, either
  // manually or automatically.
  if (is_primary_partial_view_) {
    bubble_delegate_->SetCloseCallback(
        base::BindOnce(&DownloadToolbarButtonView::OnPartialViewClosed,
                       weak_factory_.GetWeakPtr()));
  }

  UpdateIconDormant();
}

// If the browser was inactive when the bubble was shown, then the bubble would
// be inactive. This would prevent close-on-deactivate, making the bubble
// unclosable. To work around this, we activate the bubble when the current
// browser becomes active, so that clicking outside the bubble will deactivate
// and close it.
void DownloadToolbarButtonView::OnBrowserSetLastActive(Browser* browser) {
  if (browser == browser_ && bubble_delegate_ &&
      !bubble_delegate_->GetWidget()->IsClosed()) {
    // We need to defer activating the download bubble when the browser window
    // is being activated, otherwise this is ineffective on macOS.
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&views::Widget::Activate,
                                  bubble_delegate_->GetWidget()->GetWeakPtr()));
  }
  UpdateIconDormant();
}

void DownloadToolbarButtonView::OnBrowserNoLongerActive(Browser* browser) {
  UpdateIconDormant();
}

DownloadToolbarButtonView::BubbleCloser::BubbleCloser(
    DownloadToolbarButtonView* toolbar_button)
    : toolbar_button_(toolbar_button) {
  CHECK(toolbar_button_);
  if (toolbar_button->GetWidget() &&
      toolbar_button->GetWidget()->GetNativeWindow()) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, toolbar_button->GetWidget()->GetNativeWindow(),
        {ui::EventType::kMousePressed, ui::EventType::kKeyPressed,
         ui::EventType::kTouchPressed});
  }
}

DownloadToolbarButtonView::BubbleCloser::~BubbleCloser() = default;

void DownloadToolbarButtonView::BubbleCloser::OnEvent(const ui::Event& event) {
  CHECK(event_monitor_);
  if (event.IsKeyEvent() && event.AsKeyEvent()->key_code() != ui::VKEY_ESCAPE) {
    return;
  }

  if (toolbar_button_->IsShowingDetails()) {
    toolbar_button_->HideDetails();
    // `this` will be deleted.
  }
}

void DownloadToolbarButtonView::ShowIphPromo() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  Profile* profile = browser_->profile();
  // Don't show IPH Promo if safe browsing level is set by policy.
  if (safe_browsing::SafeBrowsingPolicyHandler::
          IsSafeBrowsingProtectionLevelSetByPolicy(profile->GetPrefs())) {
    return;
  }
  if (safe_browsing::GetSafeBrowsingState(*profile->GetPrefs()) ==
          safe_browsing::SafeBrowsingState::STANDARD_PROTECTION &&
      !profile->IsOffTheRecord()) {
    browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHDownloadEsbPromoFeature);
  }
#endif
}

void DownloadToolbarButtonView::OnPartialViewClosed() {
  // We use PostTask to avoid calling the FocusAndActivateWindow
  // function reentrantly from ui/wm/core/focus_controller.cc.
  // We make sure each call to the FocusAndActivateWindow method
  // finishes before the next.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DownloadToolbarButtonView::ShowIphPromo,
                                weak_factory_.GetWeakPtr()));
}

void DownloadToolbarButtonView::DeactivateAutoClose() {
  auto_close_bubble_timer_.Stop();
}

void DownloadToolbarButtonView::AutoClosePartialView() {
  // Nothing to do if the bubble is not open.
  if (!bubble_contents_) {
    return;
  }
  // Don't close the security page.
  if (bubble_contents_->VisiblePage() ==
      DownloadBubbleContentsView::Page::kSecurity) {
    return;
  }
  if (!is_primary_partial_view_ || !use_auto_close_bubble_timer_) {
    return;
  }
  // Don't close if the user is hovering over the bubble.
  if (bubble_contents_->IsMouseHovered()) {
    return;
  }
  HideDetails();
}

// If the bubble delegate is set (either the main or the partial view), the
// button press is going to make the bubble lose focus, and will destroy
// the bubble.
// If the bubble delegate is not set, show the main view.
void DownloadToolbarButtonView::ButtonPressed() {
  if (!bubble_delegate_) {
    is_primary_partial_view_ = false;
    button_click_time_ = base::TimeTicks::Now();
    CreateBubbleDialogDelegate();
  }
  controller_->OnButtonPressed();
}

void DownloadToolbarButtonView::ShowPendingDownloadStartedAnimation() {
  if (!has_pending_download_started_animation_) {
    return;
  }
  CHECK(show_download_started_animation_);
  has_pending_download_started_animation_ = false;
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    return;
  }
  content::WebContents* const web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents ||
      !platform_util::IsVisible(web_contents->GetNativeView())) {
    return;
  }
  const ui::ColorProvider* color_provider = GetColorProvider();
  // Animation cleans itself up after it's done.
  new DownloadBubbleStartedAnimationViews(
      web_contents, image_container_view()->GetBoundsInScreen(),
      color_provider->GetColor(kColorDownloadToolbarButtonAnimationForeground),
      color_provider->GetColor(kColorDownloadToolbarButtonAnimationBackground));
}

bool DownloadToolbarButtonView::ShouldShowBubbleAsInactive() const {
  // The bubble can either be shown as active or inactive. When the current
  // browser is inactive, make the bubble inactive to avoid stealing focus from
  // non-Chrome windows or showing on a different workspace.
  if (!browser_->window() || !browser_->window()->IsActive()) {
    return true;
  }

  // Don't show as active if there is a running context menu, otherwise the
  // context menu will be closed.
  if (content::WebContents* web_contents =
          browser_->tab_strip_model()->GetActiveWebContents()) {
    if (web_contents->IsShowingContextMenu()) {
      return true;
    }
  }

  // The partial view shows up without user interaction, so it should not
  // steal focus from the web contents.
  return is_primary_partial_view_;
}

void DownloadToolbarButtonView::CloseAutofillPopup() {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  if (auto* autofill_client =
          autofill::ContentAutofillClient::FromWebContents(web_contents)) {
    autofill_client->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kOverlappingWithAnotherPrompt);
  }
}

SkColor DownloadToolbarButtonView::GetIconColor() const {
  if (is_dormant_) {
    return GetColorProvider()->GetColor(kColorDownloadToolbarButtonInactive);
  }
  return GetColorProvider()->GetColor(
      active_ == IconActive::kActive ||
              GetProperty(user_education::kHasInProductHelpPromoKey)
          ? kColorDownloadToolbarButtonActive
          : kColorDownloadToolbarButtonInactive);
}

SkColor DownloadToolbarButtonView::GetProgressColor(bool is_disabled,
                                                    bool is_active) const {
  if (is_disabled) {
    return GetForegroundColor(ButtonState::STATE_DISABLED);
  }
  return GetColorProvider()->GetColor(
      is_active ? kColorDownloadToolbarButtonActive
                : kColorDownloadToolbarButtonInactive);
}

void DownloadToolbarButtonView::UpdateIconDormant() {
  // Check if the current browser is the last active browser in this profile, or
  // if the bubble is currently open.
  bool should_update_button_progress =
      browser_ == chrome::FindBrowserWithProfile(browser_->profile()) ||
      (bubble_delegate_ && !bubble_delegate_->GetWidget()->IsClosed());
  if (is_dormant_ == !should_update_button_progress) {
    return;
  }
  is_dormant_ = !should_update_button_progress;
  redraw_progress_soon_ = true;
  UpdateIcon();
}

void DownloadToolbarButtonView::OnAnyRowRemoved() {
  if (bubble_contents_->info().row_list_view_info().rows().empty()) {
    CloseDialog(views::Widget::ClosedReason::kUnspecified);
  }
}

void DownloadToolbarButtonView::DisableAutoCloseTimerForTesting() {
  use_auto_close_bubble_timer_ = false;
  DeactivateAutoClose();
}

void DownloadToolbarButtonView::DisableDownloadStartedAnimationForTesting() {
  show_download_started_animation_ = false;
}

DownloadDisplay::IconState DownloadToolbarButtonView::GetIconState() const {
  return state_;
}

void DownloadToolbarButtonView::SetBubbleControllerForTesting(
    std::unique_ptr<DownloadBubbleUIController> bubble_controller) {
  bubble_controller_ = std::move(bubble_controller);
}

BEGIN_METADATA(DownloadToolbarButtonView)
END_METADATA
