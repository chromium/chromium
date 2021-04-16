// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/read_later/read_later_button.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "url/gurl.h"

namespace {

// Enumeration of all bookmark bar prefs and states when a user can access the
// ReadLaterButton. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class BookmarkBarPrefAndState {
  kVisibleAndOnNTP = 0,
  kHiddenAndOnNTP = 1,
  kVisibleAndNotOnNTP = 2,
  kMaxValue = kVisibleAndNotOnNTP,
};

void RecordBookmarkBarState(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  BookmarkBarPrefAndState state = BookmarkBarPrefAndState::kVisibleAndNotOnNTP;
  if (web_contents) {
    const GURL site_origin = web_contents->GetLastCommittedURL().GetOrigin();
    // These are also the NTP urls checked for showing the bookmark bar on the
    // NTP.
    if (site_origin == GURL(chrome::kChromeUINewTabURL).GetOrigin() ||
        site_origin == GURL(chrome::kChromeUINewTabPageURL).GetOrigin() ||
        site_origin ==
            GURL(chrome::kChromeUINewTabPageThirdPartyURL).GetOrigin()) {
      if (browser->profile()->GetPrefs()->GetBoolean(
              bookmarks::prefs::kShowBookmarkBar)) {
        state = BookmarkBarPrefAndState::kVisibleAndOnNTP;
      } else {
        state = BookmarkBarPrefAndState::kHiddenAndOnNTP;
      }
    }
  }
  base::UmaHistogramEnumeration(
      "Bookmarks.BookmarksBarStatus.OnReadingListOpened", state);
}

// Note this matches the background base layer alpha used in ToolbarButton.
constexpr SkAlpha kBackgroundBaseLayerAlpha = 204;
constexpr base::TimeDelta kHighlightShowDuration =
    base::TimeDelta::FromMilliseconds(150);
constexpr base::TimeDelta kHighlightHideDuration =
    base::TimeDelta::FromMilliseconds(650);
constexpr base::TimeDelta kHighlightDuration =
    base::TimeDelta::FromMilliseconds(2250);

}  // namespace

ReadLaterButton::ReadLaterButton(Browser* browser)
    : LabelButton(base::BindRepeating(&ReadLaterButton::ButtonPressed,
                                      base::Unretained(this)),
                  l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE)),
      browser_(browser),
      webui_bubble_manager_(std::make_unique<WebUIBubbleManagerT<ReadLaterUI>>(
          this,
          browser->profile(),
          GURL(chrome::kChromeUIReadLaterURL),
          IDS_READ_LATER_TITLE,
          true)),
      widget_open_timer_(base::BindRepeating([](base::TimeDelta time_elapsed) {
        base::UmaHistogramMediumTimes("ReadingList.WindowDisplayedDuration",
                                      time_elapsed);
      })),
      highlight_color_animation_(
          std::make_unique<HighlightColorAnimation>(this)) {
  // Note: BrowserView may not exist during tests.
  if (BrowserView::GetBrowserViewForBrowser(browser_))
    DCHECK(!BrowserView::GetBrowserViewForBrowser(browser_)->side_panel());

  dot_indicator_ = views::DotIndicator::Install(image());

  reading_list_model_ =
      ReadingListModelFactory::GetForBrowserContext(browser_->profile());
  if (reading_list_model_)
    reading_list_model_scoped_observation_.Observe(reading_list_model_);

  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));

  views::InstallPillHighlightPathGenerator(this);
  SetInkDropMode(InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  SetInkDropVisibleOpacity(kToolbarInkDropVisibleOpacity);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);

  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
}

ReadLaterButton::~ReadLaterButton() = default;

void ReadLaterButton::CloseBubble() {
  if (webui_bubble_manager_->GetBubbleWidget())
    webui_bubble_manager_->CloseBubble();
}

std::unique_ptr<views::InkDrop> ReadLaterButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnFocus(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropHighlight>
ReadLaterButton::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

SkColor ReadLaterButton::GetInkDropBaseColor() const {
  return GetToolbarInkDropBaseColor(this);
}

void ReadLaterButton::OnThemeChanged() {
  LabelButton::OnThemeChanged();

  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  highlight_color_animation_->SetColor(
      ToolbarButton::AdjustHighlightColorForContrast(
          theme_provider, gfx::kGoogleBlue300, gfx::kGoogleBlue600,
          gfx::kGoogleBlue050, gfx::kGoogleBlue900));

  dot_indicator_->SetColor(
      /*dot_color=*/GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityHigh),
      /*border_color=*/theme_provider->GetColor(
          ThemeProperties::COLOR_TOOLBAR));
}

void ReadLaterButton::Layout() {
  LabelButton::Layout();

  // Set |dot_indicator_| bounds.
  constexpr int kDotIndicatorSize = 8;
  gfx::Rect bounds = gfx::Rect(0, 0, kDotIndicatorSize, kDotIndicatorSize);
  bounds.Offset(-2, -2);
  dot_indicator_->SetBoundsRect(bounds);
}

void ReadLaterButton::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(webui_bubble_manager_->GetBubbleWidget(), widget);
  DCHECK(bubble_widget_observation_.IsObservingSource(
      webui_bubble_manager_->GetBubbleWidget()));
  bubble_widget_observation_.Reset();
}

void ReadLaterButton::ReadingListModelLoaded(const ReadingListModel* model) {
  if (model->unseen_size())
    dot_indicator_->Show();
}

void ReadLaterButton::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  DCHECK(reading_list_model_scoped_observation_.IsObservingSource(
      reading_list_model_));
  reading_list_model_scoped_observation_.Reset();
  reading_list_model_ = nullptr;
}

void ReadLaterButton::ReadingListDidAddEntry(const ReadingListModel* model,
                                             const GURL& url,
                                             reading_list::EntrySource source) {
  if (source == reading_list::EntrySource::ADDED_VIA_CURRENT_APP &&
      BrowserView::GetBrowserViewForBrowser(browser_)->IsActive()) {
    highlight_color_animation_->Show();
  }
  dot_indicator_->Show();
}

void ReadLaterButton::ButtonPressed() {
  highlight_color_animation_->Hide();

  if (webui_bubble_manager_->GetBubbleWidget()) {
    webui_bubble_manager_->CloseBubble();
  } else {
    base::RecordAction(
        base::UserMetricsAction("DesktopReadingList.OpenReadingList"));
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser_->profile());
    tracker->NotifyEvent(feature_engagement::events::kReadingListMenuOpened);
    RecordBookmarkBarState(browser_);
    webui_bubble_manager_->ShowBubble();
    reading_list_model_->MarkAllSeen();
    dot_indicator_->Hide();
    // There should only ever be a single bubble widget active for the
    // ReadLaterButton.
    DCHECK(!bubble_widget_observation_.IsObserving());
    bubble_widget_observation_.Observe(
        webui_bubble_manager_->GetBubbleWidget());
    widget_open_timer_.Reset(webui_bubble_manager_->GetBubbleWidget());
  }
}

void ReadLaterButton::UpdateColors() {
  if (!GetThemeProvider())
    return;

  const int highlight_radius =
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMaximum, size());
  SetEnabledTextColors(highlight_color_animation_->GetTextColor());
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kReadLaterIcon, highlight_color_animation_->GetIconColor()));
  base::Optional<SkColor> background_color =
      highlight_color_animation_->GetBackgroundColor();
  if (background_color) {
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(
            *background_color, highlight_radius, gfx::Insets(0))));
  } else {
    SetBackground(nullptr);
  }
}

ReadLaterButton::HighlightColorAnimation::HighlightColorAnimation(
    ReadLaterButton* parent)
    : parent_(parent),
      highlight_color_animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kHighlightShowDuration,
                                    gfx::Tween::FAST_OUT_SLOW_IN,
                                    0.0,
                                    1.0),
          gfx::MultiAnimation::Part(kHighlightDuration,
                                    gfx::Tween::Type::LINEAR,
                                    1.0,
                                    1.0),
          gfx::MultiAnimation::Part(kHighlightHideDuration,
                                    gfx::Tween::FAST_OUT_SLOW_IN,
                                    1.0,
                                    0.0)}) {
  highlight_color_animation_.set_delegate(this);
  highlight_color_animation_.set_continuous(false);
}

ReadLaterButton::HighlightColorAnimation::~HighlightColorAnimation() = default;

void ReadLaterButton::HighlightColorAnimation::Show() {
  // Do nothing if an animation is already showing.
  if (highlight_color_animation_.is_animating())
    return;

  highlight_color_animation_.Start();
  parent_->UpdateColors();
}

void ReadLaterButton::HighlightColorAnimation::Hide() {
  ClearHighlightColor();
}

void ReadLaterButton::HighlightColorAnimation::SetColor(SkColor color) {
  highlight_color_ = color;
  parent_->UpdateColors();
}

SkColor ReadLaterButton::HighlightColorAnimation::GetTextColor() const {
  SkColor original_text_color = color_utils::GetColorWithMaxContrast(
      parent_->GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR));
  return FadeWithAnimation(highlight_color_, original_text_color);
}

base::Optional<SkColor>
ReadLaterButton::HighlightColorAnimation::GetBackgroundColor() const {
  if (!highlight_color_animation_.is_animating())
    return base::nullopt;
  SkColor original_bg_color = SkColorSetA(
      ToolbarButton::GetDefaultBackgroundColor(parent_->GetThemeProvider()),
      kBackgroundBaseLayerAlpha);
  SkColor highlight_bg_color = color_utils::GetResultingPaintColor(
      SkColorSetA(highlight_color_, SkColorGetA(highlight_color_) *
                                        kToolbarInkDropHighlightVisibleOpacity),
      original_bg_color);
  return FadeWithAnimation(highlight_bg_color, original_bg_color);
}

SkColor ReadLaterButton::HighlightColorAnimation::GetIconColor() const {
  SkColor original_icon_color = parent_->GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  return FadeWithAnimation(highlight_color_, original_icon_color);
}

void ReadLaterButton::HighlightColorAnimation::AnimationEnded(
    const gfx::Animation* animation) {
  ClearHighlightColor();
}

void ReadLaterButton::HighlightColorAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  parent_->UpdateColors();
}

SkColor ReadLaterButton::HighlightColorAnimation::FadeWithAnimation(
    SkColor target_color,
    SkColor original_color) const {
  if (!highlight_color_animation_.is_animating())
    return original_color;

  return gfx::Tween::ColorValueBetween(
      highlight_color_animation_.GetCurrentValue(), original_color,
      target_color);
}

void ReadLaterButton::HighlightColorAnimation::ClearHighlightColor() {
  highlight_color_animation_.Stop();
  parent_->UpdateColors();
}

BEGIN_METADATA(ReadLaterButton, views::LabelButton)
END_METADATA
