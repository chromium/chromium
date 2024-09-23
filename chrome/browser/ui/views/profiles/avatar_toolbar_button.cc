// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include <vector>

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
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "content/public/common/url_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kChromeRefreshImageLabelPadding = 6;

// Value used to enlarge the AvatarIcon to accommodate for DIP scaling. This is
// used to adapt other related icon modifications, such as the dotted circle
// icon in SigninPending mode.
constexpr int kAvatarIconEnlargement = 1;

}  // namespace

// static
base::TimeDelta AvatarToolbarButton::g_iph_min_delay_after_creation =
    base::Seconds(2);

AvatarToolbarButton::AvatarToolbarButton(BrowserView* browser_view)
    : ToolbarButton(base::BindRepeating(&AvatarToolbarButton::ButtonPressed,
                                        base::Unretained(this),
                                        /*is_source_accelerator=*/false)),
      browser_(browser_view->browser()),
      creation_time_(base::TimeTicks::Now()) {
  delegate_ = std::make_unique<AvatarToolbarButtonDelegate>(this, browser_);

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
}

AvatarToolbarButton::~AvatarToolbarButton() = default;

void AvatarToolbarButton::UpdateIcon() {
  // If the delegate state manager isn't initialized, that means the widget is
  // not set yet and the button doesn't have access to the theme provider to set
  // colors. Defer updating until AddedToWidget(). This may get called as a
  // result of OnUserIdentityChanged() called from the constructor when the
  // button is not yet added to the ToolbarView's hierarchy.
  if (!delegate_->IsStateManagerInitialized()) {
    return;
  }

  const int icon_size = GetIconSize();
  ui::ImageModel icon = delegate_->GetAvatarIcon(
      icon_size, GetForegroundColor(ButtonState::STATE_NORMAL));

  SetImageModel(ButtonState::STATE_NORMAL, icon);
  SetImageModel(ButtonState::STATE_DISABLED,
                ui::GetDefaultDisabledIconFromImageModel(icon));

  observer_list_.Notify(&Observer::OnIconUpdated);
}

void AvatarToolbarButton::AddedToWidget() {
  // `AddedToWidget()` can potentially be called more than once. E.g: on Mac
  // when entering/exiting fullscreen.
  if (!delegate_->IsStateManagerInitialized()) {
    delegate_->InitializeStateManager();
  }

  ToolbarButton::AddedToWidget();

  // A call to `OnThemeChanged()` occurred before adding the widget, and could
  // not be processed since the delegate was not initialized yet.
  // This will also end up calling `UpdateIcon()`.
  OnThemeChanged();
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

void AvatarToolbarButton::UpdateText() {
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);

  SetTooltipText(delegate_->GetAvatarTooltipText());
  auto [text, color] = delegate_->GetTextAndColor(color_provider);
  SetHighlight(text, color);
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
  std::optional<std::u16string> accessibility_label =
      delegate_->GetAccessibilityLabel();

  std::u16string name;
  std::u16string description;

  // The button content text as well as the button action are modified
  // dynamically with very different contexts. The accessibility label is not
  // always present, but when it is, it is either used as the main text (through
  // name) or as the secondary text (through description) if the button content
  // exists. Adapt the description to match it's default when it is not the
  // accessibility label: the tooltip or no text if the button content has no
  // text initially. All the values needs to be overridden every time in order
  // clear the previous state effect.
  std::u16string button_content = GetText();
  if (accessibility_label.has_value()) {
    if (button_content.empty()) {
      name = accessibility_label.value();
      description = delegate_->GetAvatarTooltipText();
    } else {
      name = button_content;
      description = accessibility_label.value();
    }
  } else {
    if (button_content.empty()) {
      name = delegate_->GetAvatarTooltipText();
      description = std::u16string();
    } else {
      name = button_content;
      description = delegate_->GetAvatarTooltipText();
    }
  }

  GetViewAccessibility().SetName(name);
  GetViewAccessibility().SetDescription(description);
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightTextColor() const {
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return delegate_->GetHighlightTextColor(color_provider);
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightBorderColor() const {
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return color_provider->GetColor(kColorToolbarButtonBorder);
}

void AvatarToolbarButton::UpdateInkdrop() {
  auto [hover_color_id, ripple_color_id] = delegate_->GetInkdropColors();
  ConfigureToolbarInkdropForRefresh2023(this, hover_color_id, ripple_color_id);
}

bool AvatarToolbarButton::ShouldPaintBorder() const {
  return IsLabelPresentAndVisible() && delegate_->ShouldPaintBorder();
}

bool AvatarToolbarButton::ShouldBlendHighlightColor() const {
  return this->GetWidget() && this->GetWidget()->GetCustomTheme();
}

base::ScopedClosureRunner AvatarToolbarButton::ShowExplicitText(
    const std::u16string& text,
    std::optional<std::u16string> accessibility_label) {
  return delegate_->ShowExplicitText(text, accessibility_label);
}

void AvatarToolbarButton::ResetButtonAction() {
  explicit_button_pressed_action_.Reset();
  reset_button_action_button_closure_ptr_ = nullptr;
}

base::ScopedClosureRunner AvatarToolbarButton::SetExplicitButtonAction(
    base::RepeatingClosure explicit_closure) {
  // This logic is similar to the one in
  // `AvatarToolbarButtonDelegate::ShowExplicitText()`.
  // TODO(b/323516037): look into how to combine those into one struct for
  // consistency.

  // If an action was already set, enforce resetting it and invalidate the
  // existing reset closure internally.
  if (!explicit_button_pressed_action_.is_null()) {
    // It is safe to run the scoped closure multiple times. It is a no-op after
    // the first time.
    reset_button_action_button_closure_ptr_->RunAndReset();
  }

  explicit_button_pressed_action_ = std::move(explicit_closure);

  base::ScopedClosureRunner closure = base::ScopedClosureRunner(
      base::BindRepeating(&AvatarToolbarButton::ResetButtonAction,
                          weak_ptr_factory_.GetWeakPtr()));
  // Keep a pointer to the current active closure in case the current action was
  // reset from another call to `SetExplicitButtonAction()`.
  reset_button_action_button_closure_ptr_ = &closure;
  return closure;
}

bool AvatarToolbarButton::HasExplicitButtonAction() const {
  return !explicit_button_pressed_action_.is_null();
}

void AvatarToolbarButton::SetButtonActionDisabled(bool disabled) {
  button_action_disabled_ = disabled;
}

bool AvatarToolbarButton::IsButtonActionDisabled() const {
  return button_action_disabled_;
}

void AvatarToolbarButton::MaybeShowProfileSwitchIPH() {
  // Prevent showing the promo right when the browser was created. Wait a small
  // delay for a smoother animation.
  base::TimeDelta time_since_creation = base::TimeTicks::Now() - creation_time_;
  if (time_since_creation < g_iph_min_delay_after_creation) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AvatarToolbarButton::MaybeShowProfileSwitchIPH,
                       weak_ptr_factory_.GetWeakPtr()),
        g_iph_min_delay_after_creation - time_since_creation);
    return;
  }

  // This will show the promo only after the IPH system is properly initialized.
  if (!web_app::AppBrowserController::IsWebApp(browser_)) {
    browser_->window()->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHProfileSwitchFeature);
  } else {
    // Installable PasswordManager WebUI is the only web app that has an avatar
    // toolbar button.
    auto app_url = browser_->app_controller()->GetAppStartUrl();
    CHECK(content::HasWebUIScheme(app_url) &&
          (app_url.host() == password_manager::kChromeUIPasswordManagerHost));
    browser_->window()->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature);
  }
}

void AvatarToolbarButton::MaybeShowSupervisedUserSignInIPH(
    const AccountInfo& account_info) {
  // TODO(b/351333491): Likely to need a delaying mechanism similar to
  // `MaybeShowProfileSwitchIPH`. To be decided when implementing the
  // invocation.
  if (account_info.capabilities.is_subject_to_parental_controls() !=
      signin::Tribool::kTrue) {
    return;
  }
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHSupervisedUserProfileSigninFeature,
      account_info.gaia);
  params.title_params = base::UTF8ToUTF16(account_info.given_name);

  browser_->window()->MaybeShowFeaturePromo(std::move(params));
}

void AvatarToolbarButton::MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
    const AccountInfo& account_info) {
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHExplicitBrowserSigninPreferenceRememberedFeature,
      account_info.gaia);
  params.title_params = base::UTF8ToUTF16(account_info.given_name);
  browser_->window()->MaybeShowFeaturePromo(std::move(params));
}

void AvatarToolbarButton::MaybeShowWebSignoutIPH(const std::string& gaia_id) {
  CHECK(switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
  browser_->window()->MaybeShowFeaturePromo(user_education::FeaturePromoParams(
      feature_engagement::kIPHSignoutWebInterceptFeature, gaia_id));
}

void AvatarToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  observer_list_.Notify(&Observer::OnMouseExited);
  ToolbarButton::OnMouseExited(event);
}

void AvatarToolbarButton::OnBlur() {
  observer_list_.Notify(&Observer::OnBlur);
  ToolbarButton::OnBlur();
}

void AvatarToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  if (!delegate_->IsStateManagerInitialized()) {
    return;
  }

  delegate_->OnThemeChanged(GetColorProvider());
  UpdateText();
  UpdateInkdrop();
}

// static
void AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(
    base::TimeDelta delay) {
  g_iph_min_delay_after_creation = delay;
}

void AvatarToolbarButton::ButtonPressed(bool is_source_accelerator) {
  if (button_action_disabled_) {
    return;
  }

  if (!explicit_button_pressed_action_.is_null()) {
    explicit_button_pressed_action_.Run();
    return;
  }

  // Default behavior, shows the profile menu.
  ProfileMenuCoordinator::GetOrCreateForBrowser(browser_)->Show(
      is_source_accelerator);
}

void AvatarToolbarButton::AfterPropertyChange(const void* key,
                                              int64_t old_value) {
  if (key == user_education::kHasInProductHelpPromoKey) {
    observer_list_.Notify(
        &Observer::OnIPHPromoChanged,
        GetProperty(user_education::kHasInProductHelpPromoKey));
  }
  ToolbarButton::AfterPropertyChange(key, old_value);
}

SkColor AvatarToolbarButton::GetForegroundColor(ButtonState state) const {
  bool has_custom_theme =
      this->GetWidget() && this->GetWidget()->GetCustomTheme();

  // If there is a custom theme use the `ToolbarButton` version of
  // `GetForegroundColor()` This is to avoid creating new colorIds for icons for
  // all the different states. With chrome refresh and without any custom theme,
  // the color would be same as the label color.
  if (!has_custom_theme && IsLabelPresentAndVisible()) {
    const std::optional<SkColor> foreground_color = GetHighlightTextColor();
    const auto* const color_provider = GetColorProvider();
    return foreground_color.has_value()
               ? foreground_color.value()
               : color_provider->GetColor(
                     kColorAvatarButtonHighlightDefaultForeground);
  }

  return ToolbarButton::GetForegroundColor(state);
}

bool AvatarToolbarButton::IsLabelPresentAndVisible() const {
  if (!label()) {
    return false;
  }
  return label()->GetVisible() && !label()->GetText().empty();
}

void AvatarToolbarButton::UpdateLayoutInsets() {
  SetLayoutInsets(::GetLayoutInsets(
      IsLabelPresentAndVisible() ? AVATAR_CHIP_PADDING : TOOLBAR_BUTTON));
}

int AvatarToolbarButton::GetIconSize() const {
  return ui::TouchUiController::Get()->touch_ui()
             ? kDefaultTouchableIconSize
             : kDefaultIconSizeChromeRefresh;
}

void AvatarToolbarButton::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AvatarToolbarButton::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AvatarToolbarButton::PaintButtonContents(gfx::Canvas* canvas) {
  int icon_size = GetIconSize();
  // This ensures that the bounds get are mirror adapted, and will only return
  // the mirror values if RTL or mirror is enabled.
  gfx::Rect avatar_image_bounds = image_container_view()->GetMirroredBounds();

  // Override image bounds width and height to match the icon size used.
  avatar_image_bounds.set_width(icon_size);
  avatar_image_bounds.set_height(icon_size);
  // This is needed to adapt the changes done in `AvatarToolbarButton::Layout()`
  // where the internal image is enlarged. When enlarging an image, the
  // coordinates are not affected, but the image size is and therefore the
  // container of the image as well.
  // This is only needed for the mirrored version since in the regular version
  // the icon is placed at the beginning which does not take into consideration
  // the total width (the total width is considered when getting the mirrored
  // value).
  if (GetMirrored()) {
    avatar_image_bounds.set_x(avatar_image_bounds.x() + kAvatarIconEnlargement);
  }

  delegate_->PaintIcon(canvas, avatar_image_bounds);
}

// static
base::AutoReset<std::optional<base::TimeDelta>>
AvatarToolbarButton::CreateScopedInfiniteDelayOverrideForTesting(
    AvatarDelayType delay_type) {
  return AvatarToolbarButtonDelegate::
      CreateScopedInfiniteDelayOverrideForTesting(delay_type);
}

void AvatarToolbarButton::TriggerTimeoutForTesting(AvatarDelayType delay_type) {
  delegate_->TriggerTimeoutForTesting(delay_type);  // IN-TEST
}

// static
base::AutoReset<std::optional<base::TimeDelta>> AvatarToolbarButton::
    CreateScopedZeroDelayOverrideSigninPendingTextForTesting() {
  return AvatarToolbarButtonDelegate::
      CreateScopedZeroDelayOverrideSigninPendingTextForTesting();
}

BEGIN_METADATA(AvatarToolbarButton)
END_METADATA
