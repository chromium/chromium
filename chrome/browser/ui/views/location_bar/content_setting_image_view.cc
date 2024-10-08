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
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

std::optional<ViewID> GetViewID(
    ContentSettingImageModel::ImageType image_type) {
  using ImageType = ContentSettingImageModel::ImageType;
  switch (image_type) {
    case ImageType::JAVASCRIPT:
      return ViewID::VIEW_ID_CONTENT_SETTING_JAVASCRIPT;

    case ImageType::POPUPS:
      return ViewID::VIEW_ID_CONTENT_SETTING_POPUP;

    case ImageType::COOKIES:
    case ImageType::IMAGES:
    case ImageType::GEOLOCATION:
    case ImageType::MIXEDSCRIPT:
    case ImageType::PROTOCOL_HANDLERS:
    case ImageType::MEDIASTREAM:
    case ImageType::ADS:
    case ImageType::AUTOMATIC_DOWNLOADS:
    case ImageType::MIDI_SYSEX:
    case ImageType::SOUND:
    case ImageType::FRAMEBUST:
    case ImageType::CLIPBOARD_READ_WRITE:
    case ImageType::SENSORS:
    case ImageType::NOTIFICATIONS:
    case ImageType::STORAGE_ACCESS:
      return std::nullopt;

    case ImageType::NUM_IMAGE_TYPES:
      break;
  }
  NOTREACHED();
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContentSettingImageView,
                                      kMediaActivityIndicatorElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContentSettingImageView,
                                      kMidiSysexActivityIndicatorElementId);

ContentSettingImageView::ContentSettingImageView(
    std::unique_ptr<ContentSettingImageModel> image_model,
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list, parent_delegate),
      delegate_(delegate),
      content_setting_image_model_(std::move(image_model)),
      bubble_view_(nullptr) {
  DCHECK(delegate_);
  SetUpForInOutAnimation();
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);

  std::optional<ViewID> view_id =
      GetViewID(content_setting_image_model_->image_type());
  if (view_id)
    SetID(*view_id);

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
    SetVisible(false);
    GetViewAccessibility().SetIsIgnored(true);
    critical_promo_bubble_.reset();
    return;
  }
  DCHECK(web_contents);
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

  if (content_setting_image_model_->ShouldAutoOpenBubble(web_contents)) {
    ShowBubbleImpl();
    content_setting_image_model_->SetBubbleWasAutoOpened(web_contents);
  }

  // If the content usage or blockage should be indicated to the user, start the
  // animation and record that the icon has been shown.
  if (!can_animate_ ||
      !content_setting_image_model_->ShouldRunAnimation(web_contents)) {
    return;
  }

  // We just ignore this blockage if we're already showing some other string to
  // the user.  If this becomes a problem, we could design some sort of queueing
  // mechanism to show one after the other, but it doesn't seem important now.
  int string_id = content_setting_image_model_->explanatory_string_id();
  if (string_id) {
    // Reset the slide animation so that the label's show/hide animation runs.
    ResetSlideAnimation(false);
    AnimateIn(string_id);
  }

  content_setting_image_model_->SetAnimationHasRun(web_contents);

  std::optional<ui::ElementIdentifier> element_identifier;
  switch (content_setting_image_model_->image_type()) {
    case ContentSettingImageModel::ImageType::MEDIASTREAM:
      element_identifier = kMediaActivityIndicatorElementId;
      break;
    case ContentSettingImageModel::ImageType::MIDI_SYSEX:
      element_identifier = kMidiSysexActivityIndicatorElementId;
      break;
    default:
      break;
  }
  if (element_identifier) {
    SetProperty(views::kElementIdentifierKey, *element_identifier);
  }
}

void ContentSettingImageView::SetIconColor(std::optional<SkColor> color) {
  if (icon_color_ == color)
    return;
  icon_color_ = color;
  if (content_setting_image_model_->is_visible())
    UpdateImage();
  OnPropertyChanged(&icon_color_, views::kPropertyEffectsNone);
}

std::optional<SkColor> ContentSettingImageView::GetIconColor() const {
  return icon_color_;
}

bool ContentSettingImageView::OnMousePressed(const ui::MouseEvent& event) {
  // Pause animation so that the icon does not shrink and deselect while the
  // user is attempting to press it.
  PauseAnimation();
  return IconLabelBubbleView::OnMousePressed(event);
}

bool ContentSettingImageView::OnKeyPressed(const ui::KeyEvent& event) {
  // Pause animation so that the icon does not shrink and deselect while the
  // user is attempting to press it using key commands.
  if (GetKeyClickActionForEvent(event) == KeyClickAction::kOnKeyRelease) {
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

bool ContentSettingImageView::ShowBubble(const ui::Event& event) {
  return ShowBubbleImpl();
}

bool ContentSettingImageView::ShowBubbleImpl() {
  PauseAnimation();
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();
  if (web_contents && !bubble_view_) {
    views::View* const anchor = parent();
    bubble_view_ = new ContentSettingBubbleContents(
        content_setting_image_model_->CreateBubbleModel(
            delegate_->GetContentSettingBubbleModelDelegate(), web_contents),
        web_contents, anchor, views::BubbleBorder::TOP_RIGHT);
    bubble_view_->SetHighlightedButton(this);
    views::Widget* bubble_widget =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    observation_.Observe(bubble_widget);

    // Update popup bubble's title style.
    if (views::BubbleFrameView* const frame_view =
            bubble_view_->GetBubbleFrameView()) {
      if (views::Label* title_label = frame_view->default_title()) {
        title_label->SetTextStyle(views::style::STYLE_HEADLINE_4);
        title_label->SetEnabledColorId(kColorActivityIndicatorForeground);
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

views::Widget* ContentSettingImageView::GetBubbleWidgetForTesting() const {
  if (!bubble_view_)
    return nullptr;

  return bubble_view_->GetWidget();
}

void ContentSettingImageView::OnWidgetDestroying(views::Widget* widget) {
  if (!bubble_view_ || bubble_view_->GetWidget() != widget)
    return;

#if BUILDFLAG(IS_MAC)
  if (content_setting_image_model_->image_type() ==
          ContentSettingImageModel::ImageType::GEOLOCATION &&
      content_setting_image_model_->explanatory_string_id() ==
          IDS_GEOLOCATION_TURNED_OFF) {
    base::RecordAction(
        base::UserMetricsAction("ContentSettings.GeolocationDialog.Closed"));
  }
#endif  // BUILDFLAG(IS_MAC)

  DCHECK(observation_.IsObservingSource(widget));
  observation_.Reset();
  bubble_view_ = nullptr;
  UnpauseAnimation();
}

void ContentSettingImageView::UpdateImage() {
  gfx::Image icon = content_setting_image_model_->GetIcon(icon_color_.value_or(
      color_utils::DeriveDefaultIconColor(GetForegroundColor())));
  if (!icon.IsEmpty())
    SetImageModel(ui::ImageModel::FromImage(icon));
}

void ContentSettingImageView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();

  // The promo currently is only used for Notifications, and it is only shown
  // directly after the animation is shown.
  if (web_contents &&
      content_setting_image_model_->ShouldShowPromo(web_contents)) {
    // TODO(https://crbug.com/369386219): This isn't a true promo; it was a
    // "legacy critical promo" previously - a category that no longer exists.
    // This should either be changed to an actual promo, or removed.
    //
    // DO NOT COPY THIS PATTERN ELSEWHERE; DIRECTLY CREATING A HELP BUBBLE IS
    // GENERALLY PROHIBITED AND SHOULD BE AVOIDED IN ALL NEW CODE.
    auto* const service = UserEducationServiceFactory::GetForBrowserContext(
        web_contents->GetBrowserContext());
    auto* const anchor =
        views::ElementTrackerViews::GetInstance()->GetElementForView(this,
                                                                     true);
    user_education::HelpBubbleParams params;
    params.body_text = l10n_util::GetStringUTF16(
        IDS_NOTIFICATIONS_QUIET_PERMISSION_NEW_REQUEST_PROMO);
    params.close_button_alt_text = l10n_util::GetStringUTF16(IDS_CLOSE);
    critical_promo_bubble_ =
        service->help_bubble_factory_registry().CreateHelpBubble(
            anchor, std::move(params));
    if (critical_promo_bubble_) {
      content_setting_image_model_->SetPromoWasShown(web_contents);
    }
  }
}

BEGIN_METADATA(ContentSettingImageView)
ADD_PROPERTY_METADATA(std::optional<SkColor>, IconColor)
END_METADATA
