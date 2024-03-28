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
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "content/public/common/url_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

// Note that the non-touchable icon size is larger than the default to make the
// avatar icon easier to read.
constexpr int kIconSizeForNonTouchUi = 22;
constexpr int kChromeRefreshImageLabelPadding = 6;

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

  if (features::IsChromeRefresh2023()) {
    SetImageLabelSpacing(kChromeRefreshImageLabelPadding);
    label()->SetPaintToLayer();
    label()->SetSkipSubpixelRenderingOpacityCheck(true);
    label()->layer()->SetFillsBoundsOpaquely(false);
    label()->SetSubpixelRenderingEnabled(false);
  }
}

AvatarToolbarButton::~AvatarToolbarButton() = default;

void AvatarToolbarButton::UpdateIcon() {
  if (!GetWidget()) {
    return;
  }

  UpdateIconWithoutObservers();

  for (auto& observer : observer_list_) {
    observer.OnIconUpdated();
  }
}

void AvatarToolbarButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);

  // TODO(crbug.com/1108671): this is a hack to avoid mismatch between avatar
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
  image_size.Enlarge(1, 1);
  image->SetSize(image_size);
}

void AvatarToolbarButton::UpdateIconWithoutObservers() {
  // If widget isn't set, the button doesn't have access to the theme provider
  // to set colors. Defer updating until AddedToWidget(). This may get called as
  // a result of OnUserIdentityChanged() called from the constructor when the
  // button is not yet added to the ToolbarView's hierarchy.
  if (!GetWidget()) {
    return;
  }

  const int icon_size = GetIconSize();
  for (auto state : kButtonStates) {
    SetImageModel(
        state, delegate_->GetAvatarIcon(icon_size, GetForegroundColor(state)));
  }

  SetInsets();
}

void AvatarToolbarButton::UpdateText() {
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);

  SetInsets();
  SetTooltipText(delegate_->GetAvatarTooltipText());
  auto [text, color] = delegate_->GetTextAndColor(color_provider);
  SetHighlight(text, color);
  // Update the layout insets after `SetHighlight()` since
  // text might be updated by setting the highlight.
  UpdateLayoutInsets();

  if (features::IsChromeRefresh2023()) {
    UpdateInkdrop();
    // Outset focus ring should be present for the chip but not when only
    // the icon is visible, when there is no text.
    views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(
        !IsLabelPresentAndVisible());
  }

  // TODO(crbug.com/1078221): this is a hack because toolbar buttons don't
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

std::optional<SkColor> AvatarToolbarButton::GetHighlightTextColor() const {
  if (!features::IsChromeRefresh2023()) {
    return std::nullopt;
  }

  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return delegate_->GetHighlightTextColor(color_provider);
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightBorderColor() const {
  if (features::IsChromeRefresh2023()) {
    const auto* const color_provider = GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(kColorToolbarButtonBorder);
  }

  return std::nullopt;
}

void AvatarToolbarButton::UpdateInkdrop() {
  CHECK(features::IsChromeRefresh2023());

  auto [hover_color_id, ripple_color_id] = delegate_->GetInkdropColors();
  ConfigureToolbarInkdropForRefresh2023(this, hover_color_id, ripple_color_id);
}

bool AvatarToolbarButton::ShouldPaintBorder() const {
  return (!features::IsChromeRefresh2023()) ||
         (IsLabelPresentAndVisible() && delegate_->ShouldPaintBorder());
}

bool AvatarToolbarButton::ShouldBlendHighlightColor() const {
  bool has_custom_theme =
      this->GetWidget() && this->GetWidget()->GetCustomTheme();

  return !features::IsChromeRefresh2023() || has_custom_theme;
}

base::ScopedClosureRunner AvatarToolbarButton::ShowExplicitText(
    const std::u16string& text) {
  return delegate_->ShowExplicitText(text);
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

void AvatarToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  for (auto& observer : observer_list_) {
    observer.OnMouseExited();
  }
  ToolbarButton::OnMouseExited(event);
}

void AvatarToolbarButton::OnBlur() {
  for (auto& observer : observer_list_) {
    observer.OnBlur();
  }
  ToolbarButton::OnBlur();
}

void AvatarToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  delegate_->OnThemeChanged(GetColorProvider());
  UpdateText();
  if (features::IsChromeRefresh2023()) {
    UpdateInkdrop();
  }
}

// static
void AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(
    base::TimeDelta delay) {
  g_iph_min_delay_after_creation = delay;
}

// static
void AvatarToolbarButton::SetTextDurationForTesting(base::TimeDelta duration) {
  AvatarToolbarButtonDelegate::SetTextDurationForTesting(duration);
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
    for (auto& observer : observer_list_) {
      observer.OnIPHPromoChanged(
          GetProperty(user_education::kHasInProductHelpPromoKey));
    }
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
  if (features::IsChromeRefresh2023() && !has_custom_theme &&
      IsLabelPresentAndVisible()) {
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

void AvatarToolbarButton::SetInsets() {
  // In non-touch mode we use a larger-than-normal icon size for avatars so we
  // need to compensate it by smaller insets.
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  gfx::Insets layout_insets((touch_ui || features::IsChromeRefresh2023())
                                ? 0
                                : (kDefaultIconSize - kIconSizeForNonTouchUi) /
                                      2);
  SetLayoutInsetDelta(layout_insets);
}

void AvatarToolbarButton::UpdateLayoutInsets() {
  if (!features::IsChromeRefresh2023()) {
    return;
  }

  if (IsLabelPresentAndVisible()) {
    SetLayoutInsets(::GetLayoutInsets(AVATAR_CHIP_PADDING));
  } else {
    SetLayoutInsets(::GetLayoutInsets(TOOLBAR_BUTTON));
  }
}

int AvatarToolbarButton::GetIconSize() const {
  if (ui::TouchUiController::Get()->touch_ui()) {
    return kDefaultTouchableIconSize;
  }

  return features::IsChromeRefresh2023() ? kDefaultIconSizeChromeRefresh
                                         : kIconSizeForNonTouchUi;
}

void AvatarToolbarButton::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AvatarToolbarButton::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AvatarToolbarButton::NotifyShowNameClearedForTesting() const {
  for (auto& observer : observer_list_) {
    observer.OnShowNameClearedForTesting();  // IN-TEST
  }
}

void AvatarToolbarButton::NotifyManagementTransientTextClearedForTesting()
    const {
  for (auto& observer : observer_list_) {
    observer.OnShowManagementTransientTextClearedForTesting();  // IN-TEST
  }
}

BEGIN_METADATA(AvatarToolbarButton)
END_METADATA
