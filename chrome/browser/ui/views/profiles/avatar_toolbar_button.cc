// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/webauthn/passkey_unlock_manager.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/features.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "content/public/common/url_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kChromeRefreshImageLabelPadding = 6;

// Value used to enlarge the AvatarIcon to accommodate for DIP scaling.
constexpr int kAvatarIconEnlargement = 1;

}  // namespace

AvatarToolbarButton::AvatarToolbarButton(BrowserView* browser_view)
    : ToolbarButton(base::BindRepeating(&AvatarToolbarButton::ButtonPressed,
                                        base::Unretained(this),
                                        /*is_source_accelerator=*/false)),
      state_manager_(std::make_unique<AvatarToolbarButtonStateManager>(
          *this,
          browser_view->browser())),
      slide_animation_(this) {
  state_manager_->InitializeStates();
#if BUILDFLAG(IS_CHROMEOS)
  // On CrOS this button should only show as badging for Incognito, Guest and
  // captivie portal signin. It's only enabled for non captive portal Incognito
  // where a menu is available for closing all Incognito windows.
  Profile* profile = browser_view->browser()->profile();
  CHECK(profile);
  SetEnabled(profile->IsOffTheRecord() && !profile->IsGuestSession() &&
             !profile->GetOTRProfileID().IsCaptivePortal());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Activate on press for left-mouse-button only to mimic other MenuButtons
  // without drag-drop actions (specifically the adjacent browser menu).
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON);

  SetID(VIEW_ID_AVATAR_BUTTON);
  SetProperty(views::kElementIdentifierKey, kToolbarAvatarButtonElementId);

  // The avatar should not flip with RTL UI. This does not affect text rendering
  // and LabelButton image/label placement is still flipped like usual.
  SetFlipCanvasOnPaintForRTLUI(false);

  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);

  // For consistency with identity representation, we need to have the avatar on
  // the left and the (potential) user name on the right.
  SetHorizontalAlignment(gfx::ALIGN_LEFT);

  SetImageLabelSpacing(kChromeRefreshImageLabelPadding);
  label()->SetPaintToLayer();
  label()->SetSkipSubpixelRenderingOpacityCheck(true);
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->SetSubpixelRenderingEnabled(false);

  // With default (EASE_OUT) tween type.
  slide_animation_.SetSlideDuration(base::Milliseconds(200));
}

AvatarToolbarButton::~AvatarToolbarButton() = default;

void AvatarToolbarButton::UpdateIcon() {
  // If the state manager isn't initialized, that means the widget is not set
  // yet and the button doesn't have access to the theme provider to set colors.
  // Defer updating until AddedToWidget(). This may get called as a result of
  // OnUserIdentityChanged() called from the constructor when the button is not
  // yet added to the ToolbarView's hierarchy.
  if (!state_manager_ || !GetWidget()) {
    return;
  }

  const int icon_size = GetIconSize();
  const ui::ColorProvider* const color_provider = GetColorProvider();
  CHECK(color_provider);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  auto [icon, icon_type] = state_provider->GetAvatarIcon(
      icon_size, GetForegroundColor(ButtonState::STATE_NORMAL),
      *color_provider);

  SetImageModel(ButtonState::STATE_NORMAL, icon);
  SetImageModel(ButtonState::STATE_DISABLED,
                ui::GetDefaultDisabledIconFromImageModel(icon));

  // In forced-colors mode, re-color the placeholder avatar for
  // hover/pressed/highlighted states so it remains visible against the
  // opaque ink drop background. Cache both icons so
  // OnInkDropHighlightedChanged() can swap them cheaply.
  const ui::NativeTheme* theme = GetNativeTheme();
  if (theme &&
      theme->forced_colors() != ui::ColorProviderKey::ForcedColors::kNone &&
      icon_type == AvatarIconType::kPlaceholder) {
    forced_colors_normal_icon_ = icon;
    const SkColor hovered_color =
        color_provider->GetColor(ui::kColorIconHovered);
    forced_colors_hovered_icon_ =
        ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
            profiles::GetPlaceholderAvatarIconWithColors(
                hovered_color, hovered_color, icon_size,
                profiles::PlaceholderAvatarIconParams{.has_padding = false,
                                                      .has_background = false}),
            icon_size, icon_size, profiles::SHAPE_CIRCLE));
    SetImageModel(ButtonState::STATE_HOVERED, forced_colors_hovered_icon_);
    SetImageModel(ButtonState::STATE_PRESSED, forced_colors_hovered_icon_);

    // Also override STATE_NORMAL when the ink drop is highlighted
    // (e.g. profile menu bubble is open).
    OnInkDropHighlightedChanged();
  } else {
    forced_colors_normal_icon_ = ui::ImageModel();
    forced_colors_hovered_icon_ = ui::ImageModel();
    SetImageModel(ButtonState::STATE_HOVERED, std::nullopt);
    SetImageModel(ButtonState::STATE_PRESSED, std::nullopt);
  }

  state_manager_->NotifyIconUpdated();
}

void AvatarToolbarButton::AddedToWidget() {
  // `AddedToWidget()` can potentially be called more than once. E.g: on Mac
  // when entering/exiting fullscreen.

  ToolbarButton::AddedToWidget();

  // A call to `OnThemeChanged()` occurred before adding the widget, and could
  // not be processed since the state manager was not initialized yet.
  // This will also end up calling `UpdateIcon()`.
  OnThemeChanged();
}

void AvatarToolbarButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ToolbarButton::OnBoundsChanged(previous_bounds);
  // This is needed to update the layout insets when the button is resized.
  // `ToolbarButton::SetHighlight` may NOT clear the text immediately when the
  // text is empty (clearing is delayed until the bounds are changed).
  UpdateLayoutInsets();
}

void AvatarToolbarButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);

  // TODO(crbug.com/40707582): this is a hack to avoid mismatch between avatar
  // bitmap scaling and DIP->canvas pixel scaling in fractional DIP scaling
  // modes (125%, 133%, etc.) that can cause the right-hand or bottom pixel row
  // of the avatar image to be sliced off at certain specific browser sizes and
  // configurations.
  //
  // In order to solve this, we increase the width and height of the image by 1
  // after layout, so the rest of the layout is before. Since the profile image
  // uses transparency, visually this does not cause any change in cases where
  // the bug doesn't manifest.
  auto* image = views::AsViewClass<views::ImageView>(image_container_view());
  CHECK(image);
  image->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  image->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  gfx::Size image_size = image->GetImage().size();
  image_size.Enlarge(kAvatarIconEnlargement, kAvatarIconEnlargement);
  image->SetSize(image_size);
}

void AvatarToolbarButton::AnimateTextChange(
    StateProvider* state_provider,
    const ui::ColorProvider* color_provider) {
  const std::u16string new_text = state_provider->GetText();
  const std::u16string_view current_text = GetText();

  if (new_text == current_text ||
      gfx::ScopedAnimationDurationScaleMode::is_zero()) {
    SetHighlight(new_text, state_provider->GetHighlightColor(*color_provider));
    return;
  }

  label()->SetElideBehavior(gfx::NO_ELIDE);

  if (!current_text.empty() && new_text.empty()) {
    // Defer SetHighlight() to AnimationEnded() to avoid text disappearing and
    // collapsing the animation immediately.
    slide_animation_.Hide();
    return;
  }

  SetHighlight(new_text, state_provider->GetHighlightColor(*color_provider));

  if (current_text.empty()) {
    slide_animation_.Show();
    return;
  }

  // Animate resizing between two non-empty texts.
  UpdateLayoutInsets();
  const int icon_width =
      ::GetLayoutInsets(TOOLBAR_BUTTON).width() + GetIconSize();
  const int target_width =
      ToolbarButton::CalculatePreferredSize(views::SizeBounds(width(), {}))
          .width();
  double start_value = 1.0;
  if (target_width > icon_width) {
    start_value =
        static_cast<double>(width() - icon_width) / (target_width - icon_width);
  }

  slide_animation_.Reset(std::clamp(start_value, 0.0, 1.0));
  slide_animation_.Show();
}

void AvatarToolbarButton::UpdateText() {
  if (!state_manager_ || !GetWidget()) {
    return;
  }

  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);

  AnimateTextChange(state_provider, color_provider);

  SetTooltipText(state_provider->GetAvatarTooltipText());
  UpdateAccessibilityLabel();
  // Update the layout insets after `SetHighlight()` since
  // text might be updated by setting the highlight.
  UpdateLayoutInsets();

  UpdateInkdrop();
  // Outset focus ring should be present for the chip but not when only
  // the icon is visible, when there is no text.
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(
      !IsLabelPresentAndVisible());

  // TODO(crbug.com/40689215): this is a hack because toolbar buttons don't
  // correctly calculate their preferred size until they've been laid out once
  // or twice, because they modify their own borders and insets in response to
  // their size and have their own preferred size caching mechanic. These should
  // both ideally be handled with a modern layout manager instead.
  //
  // In the meantime, to ensure that correct (or nearly correct) bounds are set,
  // we will force a resize then invalidate layout to let the layout manager
  // take over.
  SizeToPreferredSize();
  InvalidateLayout();
}

void AvatarToolbarButton::UpdateAccessibilityLabel() {
  auto [name, description] = state_manager_->GetAccessibilityLabels(GetText());

  GetViewAccessibility().SetName(name);
  GetViewAccessibility().SetDescription(description);
}

gfx::Size AvatarToolbarButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = ToolbarButton::CalculatePreferredSize(available_size);
  if (slide_animation_.is_animating()) {
    int icon_width = ::GetLayoutInsets(TOOLBAR_BUTTON).width() + GetIconSize();
    size.set_width(icon_width + (size.width() - icon_width) *
                                    slide_animation_.GetCurrentValue());
  }
  return size;
}

void AvatarToolbarButton::AnimationProgressed(const gfx::Animation* animation) {
  CHECK_EQ(animation, &slide_animation_);
  PreferredSizeChanged();
}

void AvatarToolbarButton::AnimationEnded(const gfx::Animation* animation) {
  CHECK_EQ(animation, &slide_animation_);
  label()->SetElideBehavior(gfx::ELIDE_TAIL);
  if (slide_animation_.GetCurrentValue() == 0.0) {
    SetHighlight(std::u16string(), std::nullopt);
    // When animation finishes hiding the pill update the layout.
    UpdateText();
  }
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightTextColor() const {
  if (!state_manager_ || !GetWidget()) {
    return std::nullopt;
  }

  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);

  // For the identity pill hiding animation, text color is default foreground
  // color to avoid defaulting to background color and text disappearing
  // immediately.
  std::optional<SkColor> color =
      state_provider->GetHighlightTextColor(*color_provider);
  if (color.has_value()) {
    return color;
  }

  if (!GetText().empty()) {
    return color_provider->GetColor(
        kColorAvatarButtonHighlightDefaultForeground);
  }

  return std::nullopt;
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightBorderColor() const {
  if (!GetWidget()) {
    return std::nullopt;
  }

  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return color_provider->GetColor(kColorToolbarButtonBorder);
}

void AvatarToolbarButton::UpdateInkdrop() {
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  auto [hover_color_id, ripple_color_id] = state_provider->GetInkdropColors();
  ConfigureToolbarInkdropForRefresh2023(this, hover_color_id, ripple_color_id);
}

bool AvatarToolbarButton::ShouldPaintBorder() const {
  if (!IsLabelPresentAndVisible()) {
    return false;
  }
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  return state_provider->ShouldPaintBorder();
}

bool AvatarToolbarButton::ShouldBlendHighlightColor() const {
  return false;
}

base::ScopedClosureRunner AvatarToolbarButton::SetExplicitButtonState(
    const std::u16string& text,
    std::optional<std::u16string> accessibility_label,
    std::optional<base::RepeatingCallback<void(bool)>> explicit_action) {
  CHECK(state_manager_);
  return state_manager_->SetExplicitState(text, std::move(accessibility_label),
                                          std::move(explicit_action));
}

bool AvatarToolbarButton::HasExplicitButtonState() const {
  return state_manager_->HasExplicitButtonState();
}

void AvatarToolbarButton::MaybeShowProfileSwitchIPH() {
  state_manager_->MaybeShowProfileSwitchIPH();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void AvatarToolbarButton::MaybeShowSupervisedUserSignInIPH() {
  state_manager_->MaybeShowSupervisedUserSignInIPH();
}

void AvatarToolbarButton::MaybeShowSignInBenefitsIPH() {
  state_manager_->MaybeShowSignInBenefitsIPH();
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void AvatarToolbarButton::MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
    const AccountInfo& account_info) {
  state_manager_->MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
      account_info);
}

void AvatarToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  state_manager_->NotifyMouseExited();
  ToolbarButton::OnMouseExited(event);
}

void AvatarToolbarButton::OnBlur() {
  state_manager_->NotifyBlur();
  ToolbarButton::OnBlur();
}

void AvatarToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  if (!state_manager_) {
    return;
  }

  UpdateProfileThemeColors(state_manager_->browser(), GetColorProvider());
  UpdateText();
  UpdateInkdrop();

  // Update icon when ink drop highlight changes (for forced-colors mode).
  if (auto* ink_drop_host = views::InkDrop::Get(this)) {
    ink_drop_highlight_subscription_ =
        ink_drop_host->AddHighlightedChangedCallback(base::BindRepeating(
            &AvatarToolbarButton::OnInkDropHighlightedChanged,
            base::Unretained(this)));
  }
}

void AvatarToolbarButton::ButtonPressed(bool is_source_accelerator) {
  state_manager_->HandleButtonPressed(is_source_accelerator);
}

void AvatarToolbarButton::AfterPropertyChange(const void* key,
                                              int64_t old_value) {
  if (key == user_education::kHasInProductHelpPromoKey) {
    state_manager_->NotifyIPHPromoChanged(
        GetProperty(user_education::kHasInProductHelpPromoKey));
  }
  ToolbarButton::AfterPropertyChange(key, old_value);
}

SkColor AvatarToolbarButton::GetForegroundColor(ButtonState state) const {
  const ui::ColorProvider* const color_provider = GetColorProvider();
  if (IsLabelPresentAndVisible() && color_provider) {
    return GetHighlightTextColor().value_or(
        color_provider->GetColor(kColorAvatarButtonHighlightDefaultForeground));
  }
  return ToolbarButton::GetForegroundColor(state);
}

bool AvatarToolbarButton::IsLabelPresentAndVisible() const {
  if (!label()) {
    return false;
  }
  return label()->GetVisible() && !label()->GetText().empty();
}

bool AvatarToolbarButton::IsMouseHovered() const {
  return views::View::IsMouseHovered();
}

bool AvatarToolbarButton::HasFocus() const {
  return views::View::HasFocus();
}

views::DialogDelegate* AvatarToolbarButton::GetDialogDelegate() {
  return GetProperty(views::kAnchoredDialogKey);
}

void AvatarToolbarButton::UpdateLayoutInsets() {
  SetLayoutInsets(::GetLayoutInsets(
      IsLabelPresentAndVisible() ? AVATAR_CHIP_PADDING : TOOLBAR_BUTTON));
}

void AvatarToolbarButton::OnInkDropHighlightedChanged() {
  // In forced-colors mode, swap STATE_NORMAL between the cached normal and
  // hovered icons based on the ink drop highlight state.
  if (forced_colors_hovered_icon_.IsEmpty()) {
    return;
  }
  CHECK(!forced_colors_normal_icon_.IsEmpty());
  const auto* ink_drop_host = views::InkDrop::Get(this);
  CHECK(ink_drop_host);
  if (ink_drop_host->GetHighlighted()) {
    SetImageModel(ButtonState::STATE_NORMAL, forced_colors_hovered_icon_);
  } else {
    SetImageModel(ButtonState::STATE_NORMAL, forced_colors_normal_icon_);
  }
}

void AvatarToolbarButton::AddObserver(Observer* observer) {
  state_manager_->AddObserver(observer);
}

void AvatarToolbarButton::RemoveObserver(Observer* observer) {
  state_manager_->RemoveObserver(observer);
}

void AvatarToolbarButton::ClearActiveStateForTesting() {
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  state_provider->ClearForTesting();  // IN-TEST
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void AvatarToolbarButton::ForceShowingPromoForTesting() {
  CHECK(state_manager_);
  state_manager_->ForceShowingPromoForTesting();
}

bool AvatarToolbarButton::
    GetStateAndFireSignedOutTriggerDelayTimerForTesting() {
  CHECK(state_manager_);
  return state_manager_->GetStateAndFireSignedOutTriggerDelayTimerForTesting();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

BEGIN_METADATA(AvatarToolbarButton)
END_METADATA
