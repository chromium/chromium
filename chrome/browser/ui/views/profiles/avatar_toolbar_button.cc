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
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
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

// Value used to enlarge the AvatarIcon to accommodate for DIP scaling.
constexpr int kAvatarIconEnlargement = 1;

void UpdateProfileThemeColors(Browser* browser,
                              const ui::ColorProvider* color_provider) {
  if (!color_provider) {
    return;
  }
  CHECK(browser);
  Profile* profile = browser->profile();
  CHECK(profile);
  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    return;
  }
  if (web_app::AppBrowserController::IsWebApp(browser)) {
    return;
  }
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    return;
  }
  ThemeService* service = ThemeServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  // Use default profile colors only for extension and system themes.
  entry->SetProfileThemeColors(
      ShouldUseDefaultProfileColors(*service)
          ? GetDefaultProfileThemeColors(color_provider)
          : GetCurrentProfileThemeColors(*color_provider, *service));
}

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
  CHECK(browser_);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
#if BUILDFLAG(IS_CHROMEOS)
  // On CrOS this button should only show as badging for Incognito, Guest and
  // captivie portal signin. It's only enabled for non captive portal Incognito
  // where a menu is available for closing all Incognito windows.
  Profile* profile = browser_->profile();
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
}

AvatarToolbarButton::~AvatarToolbarButton() = default;

void AvatarToolbarButton::UpdateIcon() {
  // If the state manager isn't initialized, that means the widget is not set
  // yet and the button doesn't have access to the theme provider to set colors.
  // Defer updating until AddedToWidget(). This may get called as a result of
  // OnUserIdentityChanged() called from the constructor when the button is not
  // yet added to the ToolbarView's hierarchy.
  if (!state_manager_) {
    return;
  }

  const int icon_size = GetIconSize();
  const ui::ColorProvider* const color_provider = GetColorProvider();
  CHECK(color_provider);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  ui::ImageModel icon = state_provider->GetAvatarIcon(
      icon_size, GetForegroundColor(ButtonState::STATE_NORMAL),
      *color_provider);

  SetImageModel(ButtonState::STATE_NORMAL, icon);
  SetImageModel(ButtonState::STATE_DISABLED,
                ui::GetDefaultDisabledIconFromImageModel(icon));

  observer_list_.Notify(&Observer::OnIconUpdated);
}

void AvatarToolbarButton::AddedToWidget() {
  // `AddedToWidget()` can potentially be called more than once. E.g: on Mac
  // when entering/exiting fullscreen.
  if (!state_manager_) {
    state_manager_ =
        std::make_unique<AvatarToolbarButtonStateManager>(*this, browser_);
    state_manager_->InitializeStates();
  }

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

void AvatarToolbarButton::UpdateText() {
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);

  SetTooltipText(state_provider->GetAvatarTooltipText());
  SetHighlight(state_provider->GetText(),
               state_provider->GetHighlightColor(*color_provider));
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
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  std::optional<std::u16string> accessibility_label =
      state_provider->GetAccessibilityLabel();

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
  std::u16string button_content(GetText());
  if (accessibility_label.has_value()) {
    if (button_content.empty()) {
      name = accessibility_label.value();
      description = state_provider->GetAvatarTooltipText();
    } else {
      name = button_content;
      description = accessibility_label.value();
    }
  } else {
    if (button_content.empty()) {
      name = state_provider->GetAvatarTooltipText();
      description = std::u16string();
    } else {
      name = button_content;
      description = state_provider->GetAvatarTooltipText();
    }
  }

  GetViewAccessibility().SetName(name);
  GetViewAccessibility().SetDescription(description);
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightTextColor() const {
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return state_provider->GetHighlightTextColor(*color_provider);
}

std::optional<SkColor> AvatarToolbarButton::GetHighlightBorderColor() const {
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
    BrowserUserEducationInterface::From(browser_)->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHProfileSwitchFeature);
  } else {
    // Installable PasswordManager WebUI is the only web app that has an avatar
    // toolbar button.
    auto app_url = browser_->app_controller()->GetAppStartUrl();
    CHECK(
        content::HasWebUIScheme(app_url) &&
        (app_url.GetHost() == password_manager::kChromeUIPasswordManagerHost));
    BrowserUserEducationInterface::From(browser_)->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature);
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void AvatarToolbarButton::MaybeShowSupervisedUserSignInIPH() {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHSupervisedUserProfileSigninFeature)) {
    return;
  }
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(browser_->profile());
  CHECK(identity_manager);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  auto account_info = identity_manager->FindExtendedAccountInfoByAccountId(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  if (account_info.capabilities.is_subject_to_parental_controls() !=
      signin::Tribool::kTrue) {
    return;
  }
  if (account_info.IsEmpty()) {
    return;
  }

  // Prevent showing the promo right when the browser was created.
  // This is not just used for smoother animation, but it gives the anchor
  // element enough time to become visible and display the IPH.
  // TODO(crbug.com/372689164): investigate alternative rescheduling,
  // using `CanShowFeaturePromo`.
  base::TimeDelta time_since_creation = base::TimeTicks::Now() - creation_time_;
  if (time_since_creation < g_iph_min_delay_after_creation) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AvatarToolbarButton::MaybeShowSupervisedUserSignInIPH,
                       weak_ptr_factory_.GetWeakPtr()),
        g_iph_min_delay_after_creation - time_since_creation);
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHSupervisedUserProfileSigninFeature);
  params.title_params = base::UTF8ToUTF16(account_info.given_name);
  BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
      std::move(params));
}

void AvatarToolbarButton::MaybeShowSignInBenefitsIPH() {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos) ||
      !base::FeatureList::IsEnabled(
          feature_engagement::kIPHSignInBenefitsFeature)) {
    return;
  }

  // Prevent showing the IPH bubble right when the browser was created. Wait a
  // small delay for a smoother animation.
  base::TimeDelta time_since_creation = base::TimeTicks::Now() - creation_time_;
  if (time_since_creation < g_iph_min_delay_after_creation) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AvatarToolbarButton::MaybeShowSignInBenefitsIPH,
                       weak_ptr_factory_.GetWeakPtr()),
        g_iph_min_delay_after_creation - time_since_creation);
    return;
  }

  Profile* profile = browser_->profile();
  CHECK(profile);

  // The IPH only concerns signed-in, non-syncing profiles.
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs);

  // Users who sign in after the migration and users migrated from DICe will be
  // notified with other promos communicating sign-in benefits.
  if (prefs->GetBoolean(prefs::kPrimaryAccountSetAfterSigninMigration) ||
      prefs->GetBoolean(kDiceMigrationMigrated)) {
    return;
  }

  BrowserUserEducationInterface::From(browser_)->MaybeShowStartupFeaturePromo(
      feature_engagement::kIPHSignInBenefitsFeature);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void AvatarToolbarButton::MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(
    const AccountInfo& account_info) {
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHExplicitBrowserSigninPreferenceRememberedFeature,
      account_info.gaia.ToString());
  params.title_params = base::UTF8ToUTF16(account_info.given_name);
  BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
      std::move(params));
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
  if (!state_manager_) {
    return;
  }

  UpdateProfileThemeColors(browser_, GetColorProvider());
  UpdateText();
  UpdateInkdrop();
}

// static
base::AutoReset<base::TimeDelta>
AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
    base::TimeDelta delay) {
  return base::AutoReset<base::TimeDelta>(&g_iph_min_delay_after_creation,
                                          delay);
}

void AvatarToolbarButton::ButtonPressed(bool is_source_accelerator) {
  if (button_action_disabled_) {
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (BrowserUserEducationInterface::From(browser_)->IsFeaturePromoActive(
          feature_engagement::kIPHPasswordsSavePrimingPromoFeature)) {
    BrowserUserEducationInterface::From(browser_)
        ->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHPasswordsSavePrimingPromoFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }

  if (webauthn::PasskeyUnlockManager::IsPasskeyUnlockErrorUiEnabled()) {
    webauthn::PasskeyUnlockManager* passkey_unlock_manager =
        webauthn::PasskeyUnlockManagerFactory::GetForProfile(
            browser_->profile());
    if (passkey_unlock_manager &&
        passkey_unlock_manager->ShouldDisplayErrorUi()) {
      webauthn::PasskeyUnlockManager::RecordErrorUIEventType(
          webauthn::PasskeyUnlockManager::ErrorUIEventType::
              kAvatarButtonPressed);
    }
  }
#endif

  // Notify observers before the action is performed to allow them to close any
  // open dialogs.
  observer_list_.Notify(&Observer::OnButtonPressed);

  CHECK(state_manager_);
  StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  std::optional<base::RepeatingCallback<void(bool)>> action_override =
      active_state_provider->GetButtonActionOverride();
  if (action_override.has_value()) {
    action_override->Run(is_source_accelerator);
    return;
  }

  // By default, show the profile menu.
  browser_->GetFeatures().profile_menu_coordinator()->Show(
      is_source_accelerator);
}

void AvatarToolbarButton::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Try showing the IPH for signin preference remembered.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
          signin::PrimaryAccountChangeEvent::Type::kSet ||
      event_details.GetSetPrimaryAccountAccessPoint() !=
          signin_metrics::AccessPoint::kSigninChoiceRemembered) {
    return;
  }

  GaiaId gaia_id = event_details.GetCurrentState().primary_account.gaia;
  Profile* profile = browser_->profile();
  CHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs);
  const SigninPrefs signin_prefs(*prefs);
  std::optional<base::Time> last_signout_time =
      signin_prefs.GetChromeLastSignoutTime(gaia_id);
  if (last_signout_time &&
      base::Time::Now() - last_signout_time.value() < base::Days(14)) {
    // Less than two weeks since the last sign out event.
    return;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);

  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      event_details.GetCurrentState().primary_account);
  if (!account_info.given_name.empty()) {
    MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(account_info);
  } else {
    gaia_id_for_signin_choice_remembered_ = account_info.gaia;
  }
}

void AvatarToolbarButton::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.gaia == gaia_id_for_signin_choice_remembered_ &&
      !info.given_name.empty()) {
    gaia_id_for_signin_choice_remembered_ = GaiaId();
    MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(info);
  }
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
  if (IsLabelPresentAndVisible()) {
    return GetHighlightTextColor().value_or(GetColorProvider()->GetColor(
        kColorAvatarButtonHighlightDefaultForeground));
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

// static
base::AutoReset<std::optional<base::TimeDelta>>
AvatarToolbarButton::CreateScopedInfiniteDelayOverrideForTesting(
    AvatarDelayType delay_type) {
  return AvatarToolbarButtonStateManager::
      CreateScopedInfiniteDelayOverrideForTesting(delay_type);
}

void AvatarToolbarButton::ClearActiveStateForTesting() {
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  state_provider->ClearForTesting();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// static
base::AutoReset<std::optional<base::TimeDelta>> AvatarToolbarButton::
    CreateScopedZeroDelayOverrideSigninPendingTextForTesting() {
  return AvatarToolbarButtonStateManager::
      CreateScopedZeroDelayOverrideSigninPendingTextForTesting();
}

void AvatarToolbarButton::ForceShowingPromoForTesting() {
  CHECK(state_manager_);
  state_manager_->ForceShowingPromoForTesting();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

BEGIN_METADATA(AvatarToolbarButton)
END_METADATA
