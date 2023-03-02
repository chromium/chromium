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
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_started_animation_views.h"
#include "chrome/browser/ui/views/download/bubble/download_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
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
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

constexpr int kProgressRingRadius = 9;
constexpr int kProgressRingRadiusTouchMode = 12;
constexpr float kProgressRingStrokeWidth = 1.7f;
// 7.5 rows * 60 px per row = 450;
constexpr int kMaxHeightForRowList = 450;

// Close the partial bubble after 5 seconds if the user doesn't interact with
// it.
constexpr base::TimeDelta kAutoClosePartialViewDelay = base::Seconds(5);

// Helper class to draw a circular badge with text.
class CircleBadgeImageSource : public gfx::CanvasImageSource {
 public:
  CircleBadgeImageSource(const gfx::Size& size,
                         gfx::RenderText& render_text,
                         SkColor text_color,
                         SkColor background_color)
      : gfx::CanvasImageSource(size),
        render_text_(&render_text),
        text_color_(text_color),
        background_color_(background_color) {}

  CircleBadgeImageSource(const CircleBadgeImageSource&) = delete;
  CircleBadgeImageSource& operator=(const CircleBadgeImageSource&) = delete;

  ~CircleBadgeImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(background_color_);

    const gfx::Rect& badge_rect = render_text_->display_rect();
    // Set the corner radius to make the rectangle appear like a circle.
    const int corner_radius = badge_rect.height() / 2;
    canvas->DrawRoundRect(badge_rect, corner_radius, flags);

    render_text_->SetColor(text_color_);
    render_text_->Draw(canvas);
  }

 private:
  // Pointee may be modified to change the text color upon painting.
  const raw_ptr<gfx::RenderText> render_text_ = nullptr;
  const SkColor text_color_;
  const SkColor background_color_;
};

gfx::Insets GetPrimaryViewMargin() {
  return gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_VERTICAL),
                         0);
}

gfx::Insets GetSecurityViewMargin() {
  return gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL));
}
}  // namespace

DownloadToolbarButtonView::DownloadToolbarButtonView(BrowserView* browser_view)
    : ToolbarButton(
          base::BindRepeating(&DownloadToolbarButtonView::ButtonPressed,
                              base::Unretained(this))),
      browser_(browser_view->browser()) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetVectorIcons(kDownloadToolbarButtonIcon, kDownloadToolbarButtonIcon);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON));
  SetVisible(false);

  badge_image_view_ = AddChildView(std::make_unique<views::ImageView>());
  badge_image_view_->SetPaintToLayer();
  badge_image_view_->layer()->SetFillsBoundsOpaquely(false);
  badge_image_view_->SetCanProcessEventsWithinSubtree(false);

  scanning_animation_.SetSlideDuration(base::Milliseconds(2500));
  scanning_animation_.SetTweenType(gfx::Tween::LINEAR);

  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(browser_);
  // Wait until we're done with everything else before creating `controller_`
  // since it can call `Show()` synchronously.
  controller_ = std::make_unique<DownloadDisplayController>(
      this, browser_, bubble_controller_.get());
}

DownloadToolbarButtonView::~DownloadToolbarButtonView() {
  controller_.reset();
  bubble_controller_.reset();
}

gfx::ImageSkia DownloadToolbarButtonView::GetBadgeImage(
    bool is_active,
    int progress_download_count,
    SkColor badge_text_color,
    SkColor badge_background_color) {
  // Only display the badge if there are multiple downloads.
  if (!is_active || progress_download_count < 2) {
    return gfx::ImageSkia();
  }

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
    // Color is set by the CircleBadgeImageSource when drawing.

    render_text = new_render_text.get();
    render_texts_[index] = std::move(new_render_text);
  }

  return gfx::CanvasImageSource::MakeImageSkia<CircleBadgeImageSource>(
      gfx::Size(badge_height, badge_height), *render_text, badge_text_color,
      badge_background_color);
}

void DownloadToolbarButtonView::PaintButtonContents(gfx::Canvas* canvas) {
  DownloadDisplayController::ProgressInfo progress_info =
      controller_->GetProgress();
  DownloadDisplayController::IconInfo icon_info = controller_->GetIconInfo();
  // Do not show the progress ring when there is no in progress download.
  if (progress_info.download_count == 0) {
    if (scanning_animation_.is_animating()) {
      scanning_animation_.End();
    }
    return;
  }

  bool is_disabled = GetVisualState() == Button::STATE_DISABLED;
  bool is_active = icon_info.is_active;
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

  if (icon_info.icon_state == download::DownloadIconState::kDeepScanning ||
      !progress_info.progress_certain) {
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
      /*sweep_angle=*/360 * progress_info.progress_percentage / 100.0);
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

bool DownloadToolbarButtonView::IsShowing() {
  return GetVisible();
}

void DownloadToolbarButtonView::Enable() {
  SetEnabled(true);
}

void DownloadToolbarButtonView::Disable() {
  SetEnabled(false);
}

void DownloadToolbarButtonView::UpdateDownloadIcon(bool show_animation) {
  if (show_animation && gfx::Animation::ShouldRenderRichAnimation()) {
    has_pending_download_started_animation_ = true;
    if (!needs_layout()) {
      ShowPendingDownloadStartedAnimation();
    }
  }
  UpdateIcon();
}

bool DownloadToolbarButtonView::IsFullscreenWithParentViewHidden() {
  return browser_->window()->IsFullscreen() &&
         !browser_->window()->IsToolbarVisible();
}

// This function shows the partial view. If the main view is already showing,
// we do not show the partial view. If the partial view is already showing,
// there is nothing to do here, the controller should update the partial view.
void DownloadToolbarButtonView::ShowDetails() {
  if (!bubble_delegate_) {
    is_primary_partial_view_ = true;
    if (!auto_close_bubble_timer_) {
      CreateAutoCloseTimer();
    }
    CreateBubbleDialogDelegate(GetPrimaryView());
  }
  if (auto_close_bubble_timer_) {
    auto_close_bubble_timer_->Reset();
  }
}

void DownloadToolbarButtonView::HideDetails() {
  CloseDialog(views::Widget::ClosedReason::kUnspecified);
}

bool DownloadToolbarButtonView::IsShowingDetails() {
  return bubble_delegate_ != nullptr;
}

void DownloadToolbarButtonView::UpdateIcon() {
  if (!GetWidget())
    return;

  // Schedule paint to update the progress ring.
  SchedulePaint();

  DownloadDisplayController::IconInfo icon_info = controller_->GetIconInfo();
  const gfx::VectorIcon* new_icon;
  SkColor icon_color = GetIconColor();
  bool is_touch_mode = ui::TouchUiController::Get()->touch_ui();
  if (icon_info.icon_state == download::DownloadIconState::kProgress ||
      icon_info.icon_state == download::DownloadIconState::kDeepScanning) {
    new_icon = is_touch_mode ? &kDownloadInProgressTouchIcon
                             : &kDownloadInProgressIcon;
  } else {
    new_icon = is_touch_mode ? &kDownloadToolbarButtonTouchIcon
                             : &kDownloadToolbarButtonIcon;
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

  badge_image_view_->SetImage(GetBadgeImage(
      icon_info.is_active, controller_->GetProgress().download_count,
      GetProgressColor(GetVisualState() == Button::STATE_DISABLED,
                       icon_info.is_active),
      GetColorProvider()->GetColor(kColorToolbar)));
}

void DownloadToolbarButtonView::Layout() {
  ToolbarButton::Layout();
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

std::unique_ptr<views::View> DownloadToolbarButtonView::GetPrimaryView() {
  if (is_primary_partial_view_) {
    return CreateRowListView(bubble_controller_->GetPartialView());
  } else {
    // raw ptr is safe as the toolbar view owns the bubble.
    return std::make_unique<DownloadDialogView>(
        browser_, CreateRowListView(bubble_controller_->GetMainView()), this);
  }
}

void DownloadToolbarButtonView::OpenPrimaryDialog() {
  primary_view_->SetVisible(true);
  security_view_->SetVisible(false);
  bubble_delegate_->SetButtons(ui::DIALOG_BUTTON_NONE);
  bubble_delegate_->set_margins(GetPrimaryViewMargin());
  ResizeDialog();
}

void DownloadToolbarButtonView::OpenSecurityDialog(
    DownloadBubbleRowView* download_row_view) {
  security_view_->UpdateSecurityView(download_row_view);
  primary_view_->SetVisible(false);
  security_view_->SetVisible(true);
  bubble_delegate_->set_margins(GetSecurityViewMargin());
  security_view_->UpdateAccessibilityTextAndFocus();
  ResizeDialog();
}

void DownloadToolbarButtonView::CloseDialog(
    views::Widget::ClosedReason reason) {
  if (bubble_delegate_)
    bubble_delegate_->GetWidget()->CloseWithReason(reason);
}

void DownloadToolbarButtonView::ResizeDialog() {
  // Resize may be called when there is no delegate, e.g. during bubble
  // construction.
  if (bubble_delegate_)
    bubble_delegate_->SizeToContents();
}

void DownloadToolbarButtonView::OnBubbleDelegateDeleted() {
  bubble_delegate_ = nullptr;
  primary_view_ = nullptr;
  security_view_ = nullptr;
}

void DownloadToolbarButtonView::CreateBubbleDialogDelegate(
    std::unique_ptr<View> bubble_contents_view) {
  if (!bubble_contents_view)
    return;
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      this, views::BubbleBorder::TOP_RIGHT);
  bubble_delegate->SetTitle(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_HEADER_TEXT));
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  bubble_delegate->RegisterDeleteDelegateCallback(
      base::BindOnce(&DownloadToolbarButtonView::OnBubbleDelegateDeleted,
                     weak_factory_.GetWeakPtr()));
  auto* switcher_view =
      bubble_delegate->SetContentsView(std::make_unique<views::View>());
  switcher_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  primary_view_ = switcher_view->AddChildView(std::move(bubble_contents_view));
  // raw ptr for this and member fields are safe as Toolbar Button view owns the
  // Bubble.
  security_view_ =
      switcher_view->AddChildView(std::make_unique<DownloadBubbleSecurityView>(
          bubble_controller_.get(), this, bubble_delegate.get()));
  security_view_->SetVisible(false);
  bubble_delegate->set_fixed_width(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble_delegate->set_margins(GetPrimaryViewMargin());
  bubble_delegate_ = bubble_delegate.get();
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
  bubble_delegate_->GetWidget()->Show();
}

void DownloadToolbarButtonView::CreateAutoCloseTimer() {
  auto_close_bubble_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, kAutoClosePartialViewDelay,
      base::BindRepeating(&DownloadToolbarButtonView::AutoClosePartialView,
                          base::Unretained(this)));
}

void DownloadToolbarButtonView::DeactivateAutoClose() {
  auto_close_bubble_timer_.reset();
}

void DownloadToolbarButtonView::AutoClosePartialView() {
  if (!is_primary_partial_view_ || !auto_close_bubble_timer_) {
    return;
  }
  if (primary_view_ && primary_view_->IsMouseHovered()) {
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
    CreateBubbleDialogDelegate(GetPrimaryView());
  }
  controller_->OnButtonPressed();
}

void DownloadToolbarButtonView::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIcon();
}

std::unique_ptr<views::View> DownloadToolbarButtonView::CreateRowListView(
    std::vector<DownloadUIModel::DownloadUIModelPtr> model_list) {
  // Do not create empty partial view.
  if (is_primary_partial_view_ && model_list.empty())
    return nullptr;

  auto row_list_view = std::make_unique<DownloadBubbleRowListView>(
      is_primary_partial_view_, browser_,
      base::BindOnce(&DownloadToolbarButtonView::DeactivateAutoClose,
                     base::Unretained(this)));
  for (DownloadUIModel::DownloadUIModelPtr& model : model_list) {
    // raw pointer is safe as the toolbar owns the bubble, which owns an
    // individual row view.
    row_list_view->AddChildView(std::make_unique<DownloadBubbleRowView>(
        std::move(model), row_list_view.get(), bubble_controller_.get(), this,
        browser_));
  }

  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(std::move(row_list_view));
  scroll_view->ClipHeightTo(0, kMaxHeightForRowList);
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);
  return std::move(scroll_view);
}

void DownloadToolbarButtonView::ShowPendingDownloadStartedAnimation() {
  if (!has_pending_download_started_animation_) {
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
      web_contents, image()->GetBoundsInScreen(),
      color_provider->GetColor(kColorDownloadToolbarButtonAnimationForeground),
      color_provider->GetColor(kColorDownloadToolbarButtonAnimationBackground));
  has_pending_download_started_animation_ = false;
}

SkColor DownloadToolbarButtonView::GetIconColor() const {
  return icon_color_.value_or(
      controller_->GetIconInfo().is_active
          ? GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive)
          : GetColorProvider()->GetColor(kColorDownloadToolbarButtonInactive));
}

void DownloadToolbarButtonView::SetIconColor(SkColor color) {
  if (icon_color_ == color)
    return;
  icon_color_ = color;
  UpdateIcon();
}

SkColor DownloadToolbarButtonView::GetProgressColor(bool is_disabled,
                                                    bool is_active) const {
  if (is_disabled) {
    return icon_color_.value_or(
        GetForegroundColor(ButtonState::STATE_DISABLED));
  } else if (is_active) {
    return icon_color_.value_or(
        GetColorProvider()->GetColor(kColorDownloadToolbarButtonActive));
  }
  return icon_color_.value_or(
      GetColorProvider()->GetColor(kColorDownloadToolbarButtonInactive));
}

BEGIN_METADATA(DownloadToolbarButtonView, ToolbarButton)
END_METADATA
