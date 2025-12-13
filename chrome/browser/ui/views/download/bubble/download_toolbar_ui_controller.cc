// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_started_animation_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/progress_ring_utils.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

using GetBadgeTextCallback = base::RepeatingCallback<gfx::RenderText&()>;

constexpr int kProgressRingRadius = 9;
constexpr int kProgressRingRadiusTouchMode = 12;
constexpr float kProgressRingStrokeWidth = 2.0f;

// Close the partial bubble after 5 seconds if the user doesn't interact with
// it.
constexpr base::TimeDelta kAutoClosePartialViewDelay = base::Seconds(5);

PinnedToolbarActionsContainer* GetPinnedToolbarActionsContainer(
    BrowserView* browser_view) {
  auto* toolbar_button_provider = browser_view->toolbar_button_provider();
  return toolbar_button_provider
             ? toolbar_button_provider->GetPinnedToolbarActionsContainer()
             : nullptr;
}

ToolbarButton* GetDownloadsButton(BrowserView* browser_view) {
  auto* container = GetPinnedToolbarActionsContainer(browser_view);
  return container ? container->GetButtonFor(kActionShowDownloads) : nullptr;
}

class DownloadProgressRing : public views::View, gfx::AnimationDelegate {
  METADATA_HEADER(DownloadProgressRing, views::View)
 public:
  enum class DownloadStatus { kIdle, kDormant, kScanning, kDownloading };

  DownloadProgressRing(DownloadProgressRing&) = delete;
  DownloadProgressRing& operator=(const DownloadProgressRing&) = delete;
  ~DownloadProgressRing() override = default;

  // Create a DownloadProgressRing and adds it to |parent|. The
  // returned progress ring is owned by the |parent|.
  static DownloadProgressRing* Install(ToolbarButton* parent) {
    auto progress_ring =
        base::WrapUnique<DownloadProgressRing>(new DownloadProgressRing());
    auto* ring = parent->AddChildView(std::move(progress_ring));
    return ring;
  }

  // Returns the progress ring if it is a direct child of the `parent`.
  static DownloadProgressRing* GetProgressRing(ToolbarButton* parent) {
    for (auto& child : parent->children()) {
      if (views::IsViewClass<DownloadProgressRing>(child)) {
        return views::AsViewClass<DownloadProgressRing>(child);
      }
    }
    return nullptr;
  }

  void SetIdle() {
    status_ = DownloadStatus::kIdle;
    scanning_animation_.End();
    SchedulePaint();
  }

  void SetDormant() {
    status_ = DownloadStatus::kDormant;
    scanning_animation_.End();
    SchedulePaint();
  }

  void SetScanning() {
    status_ = DownloadStatus::kScanning;
    scanning_animation_.Show();
    SchedulePaint();
  }

  void SetDownloading(int progress_percentage) {
    status_ = DownloadStatus::kDownloading;
    download_progress_percentage_ = progress_percentage;
    scanning_animation_.End();
    SchedulePaint();
  }

  DownloadStatus GetStatus() { return status_; }

  void UpdateColors(SkColor background_color, SkColor progress_color) {
    background_color_ = background_color;
    progress_color_ = progress_color;
    SchedulePaint();
  }

 private:
  DownloadProgressRing() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    // Don't allow the view to process events.
    SetCanProcessEventsWithinSubtree(false);
    scanning_animation_.SetSlideDuration(base::Milliseconds(2500));
    scanning_animation_.SetTweenType(gfx::Tween::LINEAR);
  }

  // AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    SchedulePaint();
  }

  // View:
  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);
    // Fill the parent completely.
    SetBoundsRect(parent()->GetLocalBounds());
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Do not show the progress ring when there is no in progress download.
    if (status_ == DownloadStatus::kIdle) {
      return;
    }

    int ring_radius = ui::TouchUiController::Get()->touch_ui()
                          ? kProgressRingRadiusTouchMode
                          : kProgressRingRadius;
    int x = width() / 2 - ring_radius;
    int y = height() / 2 - ring_radius;
    int diameter = 2 * ring_radius;
    gfx::RectF ring_bounds(x, y, /*width=*/diameter, /*height=*/diameter);

    if (status_ == DownloadStatus::kDormant) {
      // Draw a static solid ring.
      views::DrawProgressRing(canvas, gfx::RectFToSkRect(ring_bounds),
                              background_color_, background_color_,
                              kProgressRingStrokeWidth,
                              /*start_angle=*/0,
                              /*sweep_angle=*/0);
      return;
    }

    if (status_ == DownloadStatus::kScanning) {
      if (!scanning_animation_.is_animating()) {
        scanning_animation_.Reset();
        scanning_animation_.Show();
      }
      views::DrawSpinningRing(
          canvas, gfx::RectFToSkRect(ring_bounds), background_color_,
          progress_color_, kProgressRingStrokeWidth, /*start_angle=*/
          gfx::Tween::IntValueBetween(scanning_animation_.GetCurrentValue(), 0,
                                      360));
      return;
    }

    if (status_ == DownloadStatus::kDownloading) {
      views::DrawProgressRing(
          canvas, gfx::RectFToSkRect(ring_bounds), background_color_,
          progress_color_, kProgressRingStrokeWidth, /*start_angle=*/-90,
          /*sweep_angle=*/360 * download_progress_percentage_ / 100.0);
    }
  }

  DownloadStatus status_ = DownloadStatus::kIdle;
  int download_progress_percentage_ = 0;
  SkColor background_color_ = SK_ColorBLACK;
  SkColor progress_color_ = SK_ColorBLACK;
  gfx::SlideAnimation scanning_animation_{this};
};
BEGIN_METADATA(DownloadProgressRing)
END_METADATA

DownloadProgressRing* GetProgressRing(BrowserView* browser_view) {
  auto* button = GetDownloadsButton(browser_view);
  if (!button) {
    return nullptr;
  }
  auto* progress_ring = DownloadProgressRing::GetProgressRing(button);
  if (!progress_ring) {
    progress_ring = DownloadProgressRing::Install(button);
  }
  return progress_ring;
}

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

class DownloadsImageBadge : public views::ImageView {
  METADATA_HEADER(DownloadsImageBadge, views::ImageView)
 public:
  DownloadsImageBadge(DownloadsImageBadge&) = delete;
  DownloadsImageBadge& operator=(const DownloadsImageBadge&) = delete;
  ~DownloadsImageBadge() override = default;

  // Create a DownloadsImageBadge and adds it to |parent|. The
  // returned badge is owned by the |parent|.
  static DownloadsImageBadge* Install(ToolbarButton* parent) {
    auto badge =
        base::WrapUnique<DownloadsImageBadge>(new DownloadsImageBadge());
    return parent->AddChildView(std::move(badge));
  }

  // Returns the image badge if it is a direct child of the `parent`.
  static DownloadsImageBadge* GetImageBadge(ToolbarButton* parent) {
    for (auto& child : parent->children()) {
      if (views::IsViewClass<DownloadsImageBadge>(child)) {
        return views::AsViewClass<DownloadsImageBadge>(child);
      }
    }
    return nullptr;
  }

  void UpdateImage(bool is_active,
                   int progress_download_count,
                   SkColor badge_text_color,
                   SkColor badge_background_color) {
    const int badge_size = std::min(bounds().height(), bounds().width());
    // Only display the badge if there are multiple downloads, or this image
    // view is visible. Use 2dp to make sure that the image has size even with
    // scale factor < 1.0. (this can happen on CrOS).
    if (!is_active || progress_download_count < 2 || badge_size < 2) {
      SetImage(ui::ImageModel());
      return;
    }
    // base::Unretained is safe because this owns the ImageView to which the
    // image source is applied.
    SetImage(ui::ImageModel::FromImageSkia(
        gfx::CanvasImageSource::MakeImageSkia<CircleBadgeImageSource>(
            gfx::Size(badge_size, badge_size), badge_background_color,
            base::BindRepeating(&DownloadsImageBadge::GetBadgeText,
                                base::Unretained(this), progress_download_count,
                                badge_text_color))));
  }

 private:
  // Max download count to show in the badge. Any higher number of downloads
  // results in a placeholder ("9+").
  static constexpr int kMaxDownloadCountDisplayed = 9;

  DownloadsImageBadge() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
  }

  gfx::RenderText& GetBadgeText(int progress_download_count,
                                SkColor badge_text_color) {
    CHECK_GE(progress_download_count, 2);
    const int badge_height = bounds().height();
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

  // View:
  void Layout(PassKey) override {
    LayoutSuperclass<views::ImageView>(this);
    gfx::Size parent_size = parent()->GetPreferredSize();
    const int badge_height =
        std::min(parent_size.width(), parent_size.height()) / 2;
    const int badge_offset_x = parent_size.width() - badge_height;
    const int badge_offset_y = parent_size.height() - badge_height;
    // If the badge height has changed, clear the cache of render_texts_.
    if (badge_height != bounds().height()) {
      render_texts_ = std::array<std::unique_ptr<gfx::RenderText>,
                                 kMaxDownloadCountDisplayed>{};
    }
    SetBoundsRect(
        gfx::Rect(badge_offset_x, badge_offset_y, badge_height, badge_height));
  }

  // RenderTexts used for the number in the badge. Stores the text for "n" at
  // index n - 1, and stores the text for the placeholder ("9+") at index 0.
  // This is done to avoid re-creating the same RenderText on each paint. Text
  // color of each RenderText is reset upon each paint.
  std::array<std::unique_ptr<gfx::RenderText>, kMaxDownloadCountDisplayed>
      render_texts_{};
};
BEGIN_METADATA(DownloadsImageBadge)
END_METADATA

DownloadsImageBadge* GetImageBadge(BrowserView* browser_view) {
  auto* button = GetDownloadsButton(browser_view);
  if (!button) {
    return nullptr;
  }
  auto* badge = DownloadsImageBadge::GetImageBadge(button);
  if (!badge) {
    badge = DownloadsImageBadge::Install(button);
  }
  return badge;
}

}  // namespace

DownloadToolbarUIController::DownloadToolbarUIController(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      auto_close_bubble_timer_(
          FROM_HERE,
          kAutoClosePartialViewDelay,
          base::BindRepeating(
              &DownloadToolbarUIController::AutoClosePartialView,
              base::Unretained(this))) {
  Browser* const browser = browser_view_->browser();
  action_item_ = actions::ActionManager::Get().FindAction(
      kActionShowDownloads, browser->browser_actions()->root_action_item());
  CHECK(action_item_);
  tooltip_texts_[0] = l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON);
  action_item_->SetTooltipText(tooltip_texts_.at(0));

  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(browser);

  browser_list_observation_.Observe(BrowserList::GetInstance());
}

DownloadToolbarUIController::~DownloadToolbarUIController() {
  controller_.reset();
  bubble_controller_.reset();
}

void DownloadToolbarUIController::Init() {
  // `controller_` can call `Show()` synchronously so it must be initialized
  // separately at a point where the PinnedToolbarActionsContainer will exist.
  controller_ = std::make_unique<DownloadDisplayController>(
      this, browser_view_->browser(), bubble_controller_.get());
}

void DownloadToolbarUIController::TearDownPreBrowserWindowDestruction() {
  immersive_revealed_lock_.reset();
  // DownloadDisplayController depends on BrowserView.
  controller_.reset();
  browser_view_ = nullptr;
}

void DownloadToolbarUIController::Show() {
  auto* container = GetPinnedToolbarActionsContainer(browser_view_);
  if (!container) {
    return;
  }
  container->ShowActionEphemerallyInToolbar(kActionShowDownloads, true);
}

void DownloadToolbarUIController::Hide() {
  HideDetails();
  auto* container = GetPinnedToolbarActionsContainer(browser_view_);
  if (!container) {
    return;
  }
  container->ShowActionEphemerallyInToolbar(kActionShowDownloads, false);
}

bool DownloadToolbarUIController::IsShowing() const {
  auto* button = GetDownloadsButton(browser_view_);
  return button && button->GetVisible();
}

void DownloadToolbarUIController::Enable() {
  action_item_->SetEnabled(true);
}

void DownloadToolbarUIController::Disable() {
  action_item_->SetEnabled(false);
}

void DownloadToolbarUIController::UpdateDownloadIcon(
    const IconUpdateInfo& updates) {
  // Whether to update the icon after processing any changes.
  bool update_icon = false;

  if (updates.show_animation && show_download_started_animation_) {
    has_pending_download_started_animation_ = true;
    if (auto* container = GetPinnedToolbarActionsContainer(browser_view_)) {
      container->GetAnimatingLayoutManager()->PostOrQueueAction(base::BindOnce(
          &DownloadToolbarUIController::ShowPendingDownloadStartedAnimation,
          weak_factory_.GetWeakPtr()));
    }
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
    redraw_progress_soon_ = false;
  }
}

void DownloadToolbarUIController::AnnounceAccessibleAlertNow(
    const std::u16string& alert_text) {
  if (auto* button = GetDownloadsButton(browser_view_)) {
    button->GetViewAccessibility().AnnounceText(alert_text);
  }
}

bool DownloadToolbarUIController::IsFullscreenWithParentViewHidden() const {
#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return true;
  }
#endif

  // If immersive fullscreen, check if top chrome is visible.
  auto* const controller =
      ImmersiveModeController::From(browser_view_->browser());
  if (browser_view_ && browser_view_->GetLocationBarView() &&
      controller->IsEnabled()) {
    return !controller->IsRevealed();
  }

  // Handle the remaining fullscreen case.
  return browser_view_->browser()->window() &&
         browser_view_->browser()->window()->IsFullscreen() &&
         !browser_view_->browser()->window()->IsToolbarVisible();
}

bool DownloadToolbarUIController::ShouldShowExclusiveAccessBubble() const {
  if (!IsFullscreenWithParentViewHidden()) {
    return false;
  }
  if (!browser_view_) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  // In content fullscreen, we do not show the download bubble and the toolbar
  // is not visible. Therefore, we must show the ExclusiveAccessBubble notice.
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return true;
  }
#endif
  return !ImmersiveModeController::From(browser_view_->browser())
              ->IsEnabled() &&
         browser_view_->GetExclusiveAccessContext()->CanUserExitFullscreen();
}

void DownloadToolbarUIController::OpenSecuritySubpage(
    const offline_items_collection::ContentId& id) {
  OpenSecurityDialog(id);
}

// This function shows the partial view. If the main view is already showing,
// we do not show the partial view. If the partial view is already showing,
// there is nothing to do here, the controller should update the partial view.
void DownloadToolbarUIController::ShowDetails() {
  if (bubble_delegate_) {
    return;
  }
  is_primary_partial_view_ = true;
  if (use_auto_close_bubble_timer_) {
    auto_close_bubble_timer_.Reset();
  }
  CreateBubbleDialogDelegate();
}

void DownloadToolbarUIController::HideDetails() {
  if (IsShowingDetails()) {
    CloseDialog(views::Widget::ClosedReason::kUnspecified);
  }
}

bool DownloadToolbarUIController::IsShowingDetails() const {
  return bubble_delegate_ != nullptr &&
         bubble_delegate_->GetWidget()->IsVisible();
}

void DownloadToolbarUIController::UpdateIcon() {
  auto* button = GetDownloadsButton(browser_view_);
  if (!button) {
    return;
  }

  int progress_download_count = progress_info_.download_count;
  bool is_disabled = !action_item_->GetEnabled() || is_dormant_;
  bool is_active = active_ == IconActive::kActive;
  SkColor disabled_color =
      button->GetColorProvider()->GetColor(kColorToolbarButtonIconInactive);
  SkColor background_color =
      is_disabled ? disabled_color
                  : button->GetColorProvider()->GetColor(
                        kColorDownloadToolbarButtonRingBackground);
  SkColor progress_color =
      is_disabled ? disabled_color
                  : button->GetColorProvider()->GetColor(
                        is_active ? kColorDownloadToolbarButtonActive
                                  : kColorDownloadToolbarButtonInactive);
  SkColor badge_background_color =
      button->GetColorProvider()->GetColor(kColorToolbar);

  const gfx::VectorIcon* new_icon;
  // An active icon is indicated by the color and the presence of an underline
  // under the icon button.
  bool is_icon_active = !is_dormant_ && is_active;
  SkColor icon_color = browser_view_->GetColorProvider()->GetColor(
      is_icon_active ? kColorDownloadToolbarButtonActive
                     : kColorDownloadToolbarButtonInactive);
  bool is_touch_mode = ui::TouchUiController::Get()->touch_ui();
  if (state_ == IconState::kProgress || state_ == IconState::kDeepScanning) {
    new_icon = is_touch_mode ? &kDownloadInProgressTouchIcon
                             : &kDownloadInProgressChromeRefreshIcon;
  } else {
    new_icon = is_touch_mode ? &kDownloadToolbarButtonTouchIcon
                             : &kDownloadToolbarButtonChromeRefreshIcon;
  }
  action_item_->SetProperty(kActionItemUnderlineIndicatorKey, is_icon_active);

  action_item_->SetImage(ui::ImageModel::FromVectorIcon(*new_icon, icon_color));

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
  if (progress_download_count == 0 && is_icon_active) {
    // If there are 0 in-progress downloads but the icon is still active, use
    // the tooltip text to indicate to a11y users (along with the visual
    // indications of the icon color and underline) that there is a new
    // "unactioned" complete download.
    action_item_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON_NEW_DOWNLOAD));
  } else {
    action_item_->SetTooltipText(tooltip_for_progress_count);
  }

  redraw_progress_soon_ = false;

  DownloadProgressRing* progress_ring = GetProgressRing(browser_view_);

  GetImageBadge(browser_view_)
      ->UpdateImage(is_active, progress_download_count, progress_color,
                    badge_background_color);

  // Do not show the progress ring when there is no in progress download.
  if (state_ == IconState::kComplete || progress_info_.download_count == 0) {
    progress_ring->SetIdle();
    return;
  }

  progress_ring->UpdateColors(background_color, progress_color);

  if (is_dormant_) {
    progress_ring->SetDormant();
    return;
  }

  if (ShouldShowScanningAnimation()) {
    progress_ring->SetScanning();
    return;
  }
  progress_ring->SetDownloading(progress_info_.progress_percentage);
}

void DownloadToolbarUIController::OpenPrimaryDialog() {
  if (!bubble_delegate_) {
    return;
  }
  bubble_contents_->ShowPrimaryPage(std::nullopt);
  bubble_delegate_->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->set_margins(GetPrimaryViewMargin());
}

void DownloadToolbarUIController::OpenSecurityDialog(
    const ContentId& content_id) {
  if (!bubble_delegate_) {
    is_primary_partial_view_ = false;
    CreateBubbleDialogDelegate();
  }
  bubble_contents_->ShowSecurityPage(content_id);
  bubble_delegate_->set_margins(GetSecurityViewMargin());
}

void DownloadToolbarUIController::CloseDialog(
    views::Widget::ClosedReason reason) {
  if (bubble_delegate_) {
    bubble_delegate_->GetWidget()->CloseWithReason(reason);
  }
}

void DownloadToolbarUIController::OnSecurityDialogButtonPress(
    const DownloadUIModel& model,
    DownloadCommands::Command command) {
  if (model.GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT &&
      command == DownloadCommands::DISCARD) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DownloadToolbarUIController::ShowIphPromo,
                                  weak_factory_.GetWeakPtr()));
  }
}

void DownloadToolbarUIController::OnDialogInteracted() {
  DeactivateAutoClose();
}

std::unique_ptr<DownloadBubbleNavigationHandler::CloseOnDeactivatePin>
DownloadToolbarUIController::PreventDialogCloseOnDeactivate() {
  if (!bubble_delegate_) {
    return nullptr;
  }
  return bubble_delegate_->PreventCloseOnDeactivate();
}

base::WeakPtr<DownloadBubbleNavigationHandler>
DownloadToolbarUIController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// If the browser was inactive when the bubble was shown, then the bubble would
// be inactive. This would prevent close-on-deactivate, making the bubble
// unclosable. To work around this, we activate the bubble when the current
// browser becomes active, so that clicking outside the bubble will deactivate
// and close it.
void DownloadToolbarUIController::OnBrowserSetLastActive(Browser* browser) {
  if (browser_view_ && browser == browser_view_->browser() &&
      bubble_delegate_ && !bubble_delegate_->GetWidget()->IsClosed()) {
    // We need to defer activating the download bubble when the browser window
    // is being activated, otherwise this is ineffective on macOS.
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&views::Widget::Activate,
                                  bubble_delegate_->GetWidget()->GetWeakPtr()));
  }
  UpdateIconDormant();
}

void DownloadToolbarUIController::OnBrowserNoLongerActive(Browser* browser) {
  UpdateIconDormant();
}

void DownloadToolbarUIController::DeactivateAutoClose() {
  auto_close_bubble_timer_.Stop();
}

void DownloadToolbarUIController::InvokeUI() {
  if (!bubble_delegate_ && !bubble_controller_->GetMainView().empty()) {
    is_primary_partial_view_ = false;
    button_click_time_ = base::TimeTicks::Now();
    CreateBubbleDialogDelegate();
  } else {
    chrome::ShowDownloads(browser_view_->browser());
  }
  controller_->OnButtonPressed();
}

void DownloadToolbarUIController::ShowPendingDownloadStartedAnimation() {
  if (!has_pending_download_started_animation_) {
    return;
  }
  CHECK(show_download_started_animation_);
  has_pending_download_started_animation_ = false;
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    return;
  }
  content::WebContents* const web_contents =
      browser_view_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents ||
      !platform_util::IsVisible(web_contents->GetNativeView())) {
    return;
  }
  // Animation cleans itself up after it's done.
  if (auto* button = GetDownloadsButton(browser_view_)) {
    const ui::ColorProvider* color_provider = button->GetColorProvider();
    new DownloadBubbleStartedAnimationViews(
        web_contents, button->GetBoundsInScreen(),
        color_provider->GetColor(
            kColorDownloadToolbarButtonAnimationForeground),
        color_provider->GetColor(
            kColorDownloadToolbarButtonAnimationBackground));
  }
}

bool DownloadToolbarUIController::IsProgressRingInDownloadingStateForTesting() {
  if (auto* progress_ring = GetProgressRing(browser_view_)) {
    return progress_ring->GetStatus() ==
           DownloadProgressRing::DownloadStatus::kDownloading;
  }
  return false;
}

bool DownloadToolbarUIController::IsProgressRingInDormantStateForTesting() {
  if (auto* progress_ring = GetProgressRing(browser_view_)) {
    return progress_ring->GetStatus() ==
           DownloadProgressRing::DownloadStatus::kDormant;
  }
  return false;
}

views::ImageView* DownloadToolbarUIController::GetImageBadgeForTesting() {
  return GetImageBadge(browser_view_);
}

DownloadToolbarUIController::BubbleCloser::BubbleCloser(
    views::Button* toolbar_button,
    base::WeakPtr<DownloadDisplay> download_display)
    : download_display_(download_display) {
  CHECK(toolbar_button);
  if (toolbar_button->GetWidget() &&
      toolbar_button->GetWidget()->GetTopLevelWidget()->GetNativeWindow()) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this,
        toolbar_button->GetWidget()->GetTopLevelWidget()->GetNativeWindow(),
        {ui::EventType::kMousePressed, ui::EventType::kKeyPressed,
         ui::EventType::kTouchPressed});
  }
}

DownloadToolbarUIController::BubbleCloser::~BubbleCloser() = default;

void DownloadToolbarUIController::BubbleCloser::OnEvent(
    const ui::Event& event) {
  CHECK(event_monitor_);
  if (event.IsKeyEvent() && event.AsKeyEvent()->key_code() != ui::VKEY_ESCAPE) {
    return;
  }

  if (download_display_) {
    download_display_->HideDetails();
  }
  // `this` will be deleted.
}

void DownloadToolbarUIController::CreateBubbleDialogDelegate() {
  std::vector<DownloadUIModel::DownloadUIModelPtr> primary_view_models =
      GetPrimaryViewModels();
  if (primary_view_models.empty()) {
    return;
  }

  auto* button = GetDownloadsButton(browser_view_);
  // The bubble should not show if the button doesn't exist since it would have
  // nothing to anchor to.
  if (!button) {
    return;
  }

  // If we are in immersive fullscreen, reveal the toolbar to show the bubble.
  if (browser_view_) {
    auto* const controller =
        ImmersiveModeController::From(browser_view_->browser());
    if (controller) {
      immersive_revealed_lock_ = controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES);
    }
  }
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      button, views::BubbleBorder::TOP_RIGHT,
      views::BubbleBorder::DIALOG_SHADOW,
      /*autosize=*/true);
  bubble_delegate->SetOwnedByWidget(
      views::WidgetDelegate::OwnedByWidgetPassKey());
  bubble_delegate->SetTitle(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_HEADER_LABEL));
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->set_internal_name(kBubbleName);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->RegisterWindowClosingCallback(
      base::BindOnce(&DownloadToolbarUIController::OnBubbleClosing,
                     weak_factory_.GetWeakPtr()));
  auto bubble_contents = std::make_unique<DownloadBubbleContentsView>(
      browser_view_->browser()->AsWeakPtr(), bubble_controller_->GetWeakPtr(),
      GetWeakPtr(), is_primary_partial_view_,
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
    if (button) {
      bubble_delegate_->GetWidget()->ShowInactive();
      bubble_closer_ =
          std::make_unique<BubbleCloser>(button, weak_factory_.GetWeakPtr());
      bubble_delegate_->GetWidget()
          ->GetRootView()
          ->GetViewAccessibility()
          .AnnounceText(
              l10n_util::GetStringUTF16(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
    }
  } else {
    bubble_delegate_->GetWidget()->Show();
  }

  action_item_->SetIsShowingBubble(true);

  // For IPH bubble. The IPH should show when the partial view is closed, either
  // manually or automatically.
  if (is_primary_partial_view_) {
    bubble_delegate_->SetCloseCallback(
        base::BindOnce(&DownloadToolbarUIController::OnPartialViewClosed,
                       weak_factory_.GetWeakPtr()));
  }

  UpdateIconDormant();
}

void DownloadToolbarUIController::OnBubbleClosing() {
  immersive_revealed_lock_.reset();
  bubble_delegate_ = nullptr;
  bubble_contents_ = nullptr;
  bubble_closer_.reset();
  UpdateIconDormant();
  action_item_->SetIsShowingBubble(false);
}

void DownloadToolbarUIController::OnPartialViewClosed() {
  // We use PostTask to avoid calling the FocusAndActivateWindow
  // function reentrantly from ui/wm/core/focus_controller.cc.
  // We make sure each call to the FocusAndActivateWindow method
  // finishes before the next.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DownloadToolbarUIController::ShowIphPromo,
                                weak_factory_.GetWeakPtr()));
}

void DownloadToolbarUIController::ShowIphPromo() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (auto* button = GetDownloadsButton(browser_view_)) {
    button->SetProperty(views::kElementIdentifierKey,
                        kToolbarDownloadButtonElementId);
  }
  Profile* profile = browser_view_->GetProfile();
  // Don't show IPH Promo if safe browsing level is set by policy.
  if (safe_browsing::SafeBrowsingPolicyHandler::
          IsSafeBrowsingProtectionLevelSetByPolicy(profile->GetPrefs())) {
    return;
  }
  if (safe_browsing::GetSafeBrowsingState(*profile->GetPrefs()) ==
          safe_browsing::SafeBrowsingState::STANDARD_PROTECTION &&
      !profile->IsOffTheRecord()) {
    BrowserUserEducationInterface::From(browser_view_->browser())
        ->MaybeShowFeaturePromo(
            feature_engagement::kIPHDownloadEsbPromoFeature);
  }
#endif
}

void DownloadToolbarUIController::AutoClosePartialView() {
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

std::vector<DownloadUIModel::DownloadUIModelPtr>
DownloadToolbarUIController::GetPrimaryViewModels() {
  return is_primary_partial_view_ ? bubble_controller_->GetPartialView()
                                  : bubble_controller_->GetMainView();
}

bool DownloadToolbarUIController::ShouldShowBubbleAsInactive() const {
  // The bubble can either be shown as active or inactive. When the current
  // browser is inactive, make the bubble inactive to avoid stealing focus from
  // non-Chrome windows or showing on a different workspace.
  if (!browser_view_->browser()->window() ||
      !browser_view_->browser()->window()->IsActive()) {
    return true;
  }

  // Don't show as active if there is a running context menu, otherwise the
  // context menu will be closed.
  if (content::WebContents* web_contents =
          browser_view_->browser()->tab_strip_model()->GetActiveWebContents()) {
    if (web_contents->IsShowingContextMenu()) {
      return true;
    }
  }

  // The partial view shows up without user interaction, so it should not
  // steal focus from the web contents.
  return is_primary_partial_view_;
}

void DownloadToolbarUIController::CloseAutofillPopup() {
  content::WebContents* web_contents =
      browser_view_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  if (auto* autofill_client =
          autofill::ContentAutofillClient::FromWebContents(web_contents)) {
    autofill_client->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kOverlappingWithAnotherPrompt);
  }
}

bool DownloadToolbarUIController::ShouldShowScanningAnimation() const {
  bool should_show = !is_dormant_ && (state_ == IconState::kDeepScanning ||
                                      !progress_info_.progress_certain);
  return should_show;
}

void DownloadToolbarUIController::UpdateIconDormant() {
  // Ensure no updates are attempted once BrowserView destruction has started or
  // if the host Widget has already been closed.
  if (!browser_view_ || !browser_view_->GetWidget() ||
      browser_view_->GetWidget()->IsClosed()) {
    return;
  }

  // Check if the current browser is the last active browser in this profile.
  // TODO(crbug.com/323962334): This should also check whether the bubble is
  // open once the bubble is added.
  bool should_update_button_progress =
      browser_view_->browser() ==
      chrome::FindBrowserWithProfile(browser_view_->GetProfile());
  if (is_dormant_ == !should_update_button_progress) {
    return;
  }
  is_dormant_ = !should_update_button_progress;
  UpdateIcon();
}

DownloadDisplay::IconState DownloadToolbarUIController::GetIconState() const {
  return state_;
}

void DownloadToolbarUIController::OnAnyRowRemoved() {
  if (bubble_contents_->info().row_list_view_info().rows().empty()) {
    CloseDialog(views::Widget::ClosedReason::kUnspecified);
  }
}
