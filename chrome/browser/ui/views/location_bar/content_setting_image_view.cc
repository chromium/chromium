// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/property_effects.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Duration of the label slide-in animation when the label is kept expanded
// until the bubble closes. Matches the slide-in portion of the default in-out
// animation.
constexpr base::TimeDelta kLabelSlideInDuration = base::Milliseconds(600);

std::optional<ViewID> GetViewID(
    ContentSettingImageModel::ImageType image_type) {
  using ImageType = ContentSettingImageModel::ImageType;
  switch (image_type) {
    case ImageType::kJavaScript:
      return ViewID::VIEW_ID_CONTENT_SETTING_JAVASCRIPT;

    case ImageType::kPopups:
      return ViewID::VIEW_ID_CONTENT_SETTING_POPUP;

    case ImageType::kCookies:
    case ImageType::kImages:
    case ImageType::kGeolocation:
    case ImageType::kMixedScript:
    case ImageType::kProtocolHandlers:
    case ImageType::kMediaStream:
    case ImageType::kAds:
    case ImageType::kAutomaticDownloads:
    case ImageType::kMidiSysex:
    case ImageType::kSound:
    case ImageType::kFramebust:
    case ImageType::kClipboardReadWrite:
    case ImageType::kSensors:
    case ImageType::kNotifications:
    case ImageType::kStorageAccess:
#if BUILDFLAG(IS_CHROMEOS)
    case ImageType::kSmartCard:
#endif
#if BUILDFLAG(IS_WIN)
    case ImageType::kProtectedMediaIdentifier:
#endif
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace

ContentSettingImageView::ContentSettingImageView(
    std::unique_ptr<ContentSettingImageModel> image_model,
    IconLabelBubbleView::Delegate* parent_delegate,
    ContentSettingImageViewDelegate* delegate,
    BrowserWindowInterface* browser,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list, parent_delegate),
      delegate_(delegate),
      content_setting_image_model_(std::move(image_model)),
      bubble_view_(nullptr),
      browser_(browser) {
  CHECK(delegate_, base::NotFatalUntil::M162);
  SetUpForInOutAnimation();
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);

  UpdateElementIdentifier();

  std::optional<ViewID> view_id =
      GetViewID(content_setting_image_model_->image_type());
  if (view_id) {
    SetID(*view_id);
  }

  // Because this view is focusable, it should always have an accessible name,
  // even if an announcement is not to be made.
  // TODO(crbug.com/40890218): `IconLabelBubbleView::GetAccessibleNodeData`
  // would set the name to explicitly empty when the name was missing.
  // That function no longer exists. As a result we need to handle that here.
  // There appear to be cases in which `Update` is never called and we lack
  // an announcement string ID during construction. As a result, this view
  // will lack an accessible name and the paint checks will fail. Shouldn't
  // this view always have an accessible name? If not, should it be pruned
  // from the accessibility tree when it lacks one?
  const std::u16string& accessible_name =
      content_setting_image_model_->AccessibilityAnnouncementStringId()
          ? l10n_util::GetStringUTF16(content_setting_image_model_
                                          ->AccessibilityAnnouncementStringId())
          : std::u16string();

  GetViewAccessibility().SetName(
      accessible_name, accessible_name.empty()
                           ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                           : ax::mojom::NameFrom::kAttribute);

  // The chrome refresh version of this view has a ripple effect which is
  // configured by the background.
  UpdateBackground();
}

ContentSettingImageView::~ContentSettingImageView() = default;

void ContentSettingImageView::Update() {
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();

  bool force_hide = delegate_->ShouldHideContentSettingImage();
  content_setting_image_model_->Update(force_hide ? nullptr : web_contents);
  SetTooltipText(content_setting_image_model_->get_tooltip());

  if (!content_setting_image_model_->is_visible()) {
    keep_label_expanded_ = false;
    label_expanded_web_contents_ = nullptr;
    ResetSlideAnimation(false);
    SetVisible(false);
    GetViewAccessibility().SetIsIgnored(true);
    return;
  }
  CHECK(web_contents, base::NotFatalUntil::M162);
  UpdateImage();
  SetVisible(true);
  GetViewAccessibility().SetIsIgnored(false);
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);

  if (content_setting_image_model_->ShouldNotifyAccessibility(web_contents)) {
    auto name = l10n_util::GetStringUTF16(
        content_setting_image_model_->AccessibilityAnnouncementStringId());
    GetViewAccessibility().SetName(name);

    GetViewAccessibility().AnnounceAlert(l10n_util::GetStringFUTF16(
        IDS_A11Y_INDICATORS_ANNOUNCEMENT, name,
        l10n_util::GetStringUTF16(IDS_A11Y_OMNIBOX_CHIP_HINT)));

    content_setting_image_model_->AccessibilityWasNotified(web_contents);
  }

  const bool should_auto_open_bubble =
      content_setting_image_model_->ShouldAutoOpenBubble(web_contents);
  const bool should_run_animation =
      content_setting_image_model_->ShouldRunAnimation(web_contents);
  const int string_id = content_setting_image_model_->explanatory_string_id();
  const bool should_keep_label_expanded =
      string_id && content_setting_image_model_->should_keep_label_expanded();

  // Collapse a label that was kept expanded if it is about to be re-animated,
  // if it should no longer be kept expanded, or if it was expanded for a
  // different tab.
  if (keep_label_expanded_ &&
      (should_run_animation || !should_keep_label_expanded ||
       label_expanded_web_contents_.get() != web_contents)) {
    CollapseKeptExpandedLabel(/*animate=*/false);
  }
  if (should_run_animation && should_keep_label_expanded) {
    keep_label_expanded_ = true;
    label_expanded_web_contents_ = web_contents->GetWeakPtr();
  }

  if (should_auto_open_bubble) {
    content_setting_image_model_->SetBubbleWasAutoOpened(web_contents);
    ShowBubbleImpl();
  }

  // If the content usage or blockage should be indicated to the user, start the
  // animation and record that the icon has been shown. `can_animate_` only
  // suppresses transient in-out animations; labels with `keep_label_expanded_`
  // still animate in and stay expanded until the bubble is closed.
  if ((!can_animate_ && !keep_label_expanded_) || !should_run_animation) {
    return;
  }

  // We just ignore this blockage if we're already showing some other string to
  // the user.  If this becomes a problem, we could design some sort of queueing
  // mechanism to show one after the other, but it doesn't seem important now.
  if (string_id) {
    if (keep_label_expanded_) {
      // Animate the label in, but don't automatically animate it out again.
      SetUpForAnimation(kLabelSlideInDuration);
    } else {
      SetUpForInOutAnimation();
    }
    // Reset the slide animation so that the label's show/hide animation runs.
    ResetSlideAnimation(false);
    AnimateIn(string_id);
  }

  content_setting_image_model_->SetAnimationHasRun(web_contents);

  UpdateElementIdentifier();
}

void ContentSettingImageView::UpdateElementIdentifier() {
  SetProperty(views::kElementIdentifierKey,
              content_setting_image_model_->GetElementIdentifier());
}

void ContentSettingImageView::SetIconColor(std::optional<SkColor> color) {
  if (icon_color_ == color) {
    return;
  }
  icon_color_ = color;
  if (content_setting_image_model_->is_visible()) {
    UpdateImage();
  }
  OnPropertyChanged(&icon_color_, views::PropertyEffects::kNone);
}

std::optional<SkColor> ContentSettingImageView::GetIconColor() const {
  return icon_color_;
}

bool ContentSettingImageView::OnMousePressed(const ui::MouseEvent& event) {
  // Pause animation so that the icon does not shrink and deselect while the
  // user is attempting to press it. Not needed if the label is kept expanded.
  if (!keep_label_expanded_) {
    PauseAnimation();
  }
  return IconLabelBubbleView::OnMousePressed(event);
}

bool ContentSettingImageView::OnKeyPressed(const ui::KeyEvent& event) {
  // Pause animation so that the icon does not shrink and deselect while the
  // user is attempting to press it using key commands. Not needed if the label
  // is kept expanded.
  if (!keep_label_expanded_ &&
      GetKeyClickActionForEvent(event) == KeyClickAction::kOnKeyRelease) {
    PauseAnimation();
  }
  return Button::OnKeyPressed(event);
}

void ContentSettingImageView::OnThemeChanged() {
  UpdateImage();
  IconLabelBubbleView::OnThemeChanged();
}

bool ContentSettingImageView::ShouldShowSeparator() const {
  return false;
}

bool ContentSettingImageView::ShouldShowLabelAfterAnimation() const {
  return keep_label_expanded_;
}

bool ContentSettingImageView::ShowBubble(const ui::Event& event) {
  return ShowBubbleImpl();
}

bool ContentSettingImageView::ShowBubbleImpl() {
  // If the label is kept expanded, let it finish animating in rather than
  // pausing it; it will collapse once the bubble is closed.
  if (!keep_label_expanded_) {
    PauseAnimation();
  }
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();
  if (web_contents && !bubble_view_) {
    std::unique_ptr<ContentSettingBubbleModel> bubble_model =
        content_setting_image_model_->CreateBubbleModel(
            delegate_->GetContentSettingBubbleModelDelegate(), web_contents);
    if (!bubble_model) {
      // The state backing this icon is gone; refresh to hide the icon rather
      // than showing an empty bubble.
      UnpauseAnimation();
      Update();
      return false;
    }
    views::View* const anchor = parent();
    bubble_view_ = new ContentSettingBubbleContents(
        std::move(bubble_model), web_contents, views::BubbleAnchor(anchor),
        views::BubbleBorder::TOP_RIGHT);
    bubble_view_->SetHighlightedElement(
        content_setting_image_model_->GetElementIdentifier());
    views::Widget* bubble_widget =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    observation_.Observe(bubble_widget);

    // Update popup bubble's title style.
    if (views::BubbleFrameView* const frame_view =
            bubble_view_->GetBubbleFrameView()) {
      if (views::Label* title_label = frame_view->default_title()) {
        title_label->SetTextStyle(views::style::STYLE_HEADLINE_4);
        title_label->SetEnabledColor(kColorActivityIndicatorForeground);
      }
    }

    bubble_widget->Show();
    delegate_->OnContentSettingImageBubbleShown(
        content_setting_image_model_->image_type());
  }

  return true;
}

bool ContentSettingImageView::IsBubbleShowing() const {
  return bubble_view_ != nullptr;
}

ContentSettingImageModel::ImageType ContentSettingImageView::GetType() const {
  return content_setting_image_model_->image_type();
}

views::Widget* ContentSettingImageView::GetBubbleWidget() const {
  if (!bubble_view_) {
    return nullptr;
  }

  return bubble_view_->GetWidget();
}

views::BubbleDialogDelegateView*
ContentSettingImageView::GetBubbleViewForTesting() const {
  return bubble_view_.get();
}

void ContentSettingImageView::OnWidgetDestroying(views::Widget* widget) {
  if (!bubble_view_ || bubble_view_->GetWidget() != widget) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  if (content_setting_image_model_->image_type() ==
          ContentSettingImageModel::ImageType::kGeolocation &&
      content_setting_image_model_->explanatory_string_id() ==
          IDS_GEOLOCATION_TURNED_OFF) {
    base::RecordAction(
        base::UserMetricsAction("ContentSettings.GeolocationDialog.Closed"));
  }
#endif  // BUILDFLAG(IS_MAC)

  CHECK(observation_.IsObservingSource(widget), base::NotFatalUntil::M162);
  observation_.Reset();
  bubble_view_ = nullptr;

  if (keep_label_expanded_) {
    // Now that the bubble is closed, collapse the label again.
    CollapseKeptExpandedLabel(/*animate=*/true);
    return;
  }
  UnpauseAnimation();
}

void ContentSettingImageView::CollapseKeptExpandedLabel(bool animate) {
  keep_label_expanded_ = false;
  label_expanded_web_contents_ = nullptr;
  if (animate) {
    AnimateOut();
  } else {
    ResetSlideAnimation(false);
    PreferredSizeChanged();
  }
}

void ContentSettingImageView::UpdateImage() {
  gfx::Image icon = content_setting_image_model_->GetIcon(icon_color_.value_or(
      color_utils::DeriveDefaultIconColor(GetForegroundColor())));
  if (!icon.IsEmpty()) {
    SetImageModel(ui::ImageModel::FromImage(icon));
  }
}

void ContentSettingImageView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();

  // The promo currently is only used for Notifications, and it is only shown
  // directly after the animation is shown.
  if (web_contents &&
      !content_setting_image_model_->blocked_on_system_level()) {
    const webapps::AppId* app_id =
        web_app::WebAppTabHelper::GetAppId(web_contents);
    if (app_id) {
      user_education::FeaturePromoParams params(
          feature_engagement::kIPHPwaQuietNotificationFeature, *app_id);
      BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
          std::move(params));
    }
  }
}

BEGIN_METADATA(ContentSettingImageView)
ADD_PROPERTY_METADATA(std::optional<SkColor>, IconColor)
END_METADATA
