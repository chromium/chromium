// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"

#include <optional>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

// Timings used for testing purposes. Infinite time for the tests to confidently
// test the behaviors while a delay is ongoing.
constexpr base::TimeDelta kInfiniteTimeForTesting = base::TimeDelta::Max();

constexpr base::TimeDelta kShowNameDuration = base::Seconds(3);
static std::optional<base::TimeDelta> g_show_name_duration_for_testing;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
constexpr base::TimeDelta kShowSigninPendingTextDelay = base::Minutes(50);
static std::optional<base::TimeDelta>
    g_show_signin_pending_text_delay_for_testing;

constexpr base::TimeDelta kHistorySyncOptinDuration = base::Seconds(60);
static std::optional<base::TimeDelta> g_history_sync_optin_duration_for_testing;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

ProfileAttributesStorage& GetProfileAttributesStorage() {
  return g_browser_process->profile_manager()->GetProfileAttributesStorage();
}

ProfileAttributesEntry* GetProfileAttributesEntry(Profile& profile) {
  return GetProfileAttributesStorage().GetProfileAttributesWithPath(
      profile.GetPath());
}

gfx::Image GetGaiaAccountImage(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager
        ->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin))
        .account_image;
  }
  return gfx::Image();
}

std::u16string GetShortProfileName(Profile& profile) {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);
  // If the profile is being deleted, it doesn't matter what name is shown.
  if (!entry) {
    return std::u16string();
  }
  return signin_ui_util::GetShortProfileIdentityToDisplay(*entry, &profile);
}

gfx::Image GetProfileAvatarImage(Profile& profile,
                                 const ui::ColorProvider& color_provider,
                                 int preferred_size) {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);
  if (!entry) {  // This can happen if the user deletes the current profile.
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  // TODO(crbug.com/40102223): it should suffice to call entry->GetAvatarIcon().
  // For this to work well, this class needs to observe ProfileAttributesStorage
  // instead of (or on top of) IdentityManager. Only then we can rely on |entry|
  // being up to date (as the storage also observes IdentityManager so there's
  // no guarantee on the order of notifications).
  if (entry->IsUsingGAIAPicture() && entry->GetGAIAPicture()) {
    return *entry->GetGAIAPicture();
  }

  // Show |user_identity_image| when the following conditions are satisfied:
  //  - the user is migrated to Dice
  //  - the user isn't syncing
  //  - the profile icon wasn't explicitly changed
  gfx::Image gaia_account_image = GetGaiaAccountImage(&profile);
  if (!gaia_account_image.IsEmpty() &&
      AccountConsistencyModeManager::IsDiceEnabledForProfile(&profile) &&
      !IdentityManagerFactory::GetForProfile(&profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSync) &&
      entry->IsUsingDefaultAvatar()) {
    return gaia_account_image;
  }

  return entry->GetAvatarIcon(
      preferred_size, /*use_high_res_file=*/true,
      GetPlaceholderAvatarIconParamsDependingOnTheme(
          ThemeServiceFactory::GetForProfile(&profile),
          /*background_color_id=*/kColorToolbar, color_provider));
}

ui::ImageModel GetAvatarImageWithDottedRing(
    Profile& profile,
    const ui::ColorProvider& color_provider,
    int icon_size) {
  // Square image with a dotted ring.
  gfx::ImageSkia image_with_ring = profiles::GetAvatarWithDottedRing(
      ui::ImageModel::FromImage(
          GetProfileAvatarImage(profile, color_provider, icon_size)),
      icon_size,
      /*has_padding=*/false, /*has_background=*/false, color_provider);
  // Crop to a circle.
  return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
      gfx::Image(image_with_ring), image_with_ring.size().width(),
      image_with_ring.size().height(), profiles::AvatarShape::SHAPE_CIRCLE));
}

}  // namespace

namespace internal {

// States of the button ordered in priority of getting displayed.
// The order of those values is used with the `StateManager` to make sure the
// active state with the highest priority is shown.
// The lower the value of the enum, the higher the priority.
enum class ButtonState {
  kGuestSession,
  kIncognitoProfile,
  kExplicitTextShowing,
  kShowIdentityName,
  kSigninPending,
  kSyncPaused,
  kUpgradeClientError,
  kPassphraseError,
  // Catch-all for remaining errors in sync-the-feature or sync-the-transport.
  kSyncError,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  kHistorySyncOptin,
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Includes Work and School.
  kManagement,
  kNormal
};

namespace {

class StateProvider;
class ExplicitStateProvider;
class SyncErrorBaseStateProvider;
class SigninPendingStateProvider;
class ShowIdentityNameStateProvider;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class HistorySyncOptinStateProvider;
class ManagementStateProvider;
#endif

class StateObserver {
 public:
  virtual void OnStateProviderUpdateRequest(StateProvider* state_provider) = 0;

  virtual ~StateObserver() = default;
};

// StateManagerObserver is used to observe changes in the active button state.
//
// NOTE: This should only be used by `StateProvider`(s) if they really need to
// know when the active state changes. `StateProvider`(s) should be as
// independent as possible and in most cases this is not needed.
class StateManagerObserver {
 public:
  // Called by `StateManager` when the active button state changes.
  // `old_state` will be `std::nullopt` if there was no active state before
  // (i.e. initialization).
  virtual void OnButtonStateChanged(std::optional<ButtonState> old_state,
                                    ButtonState new_state) = 0;

  virtual ~StateManagerObserver() = default;
};

// Provides the information needed to display a specific button state.
// This class provides a default implementation for button appearance/behavior,
// the derived classes can override any of the `StateProvider` methods to
// provide a specific button appearance/behavior. The text shown on the button
// is state specific, therefore derived classes must override the `GetText()`
// method.
class StateProvider {
 public:
  // The constructor should not call any function that would end up calling
  // `RequestUpdate()` as it could end up trying to compute the active state,
  // which is not guaranteed to return a valid state at this point since all the
  // main states might not be created yet.
  // Consider overriding `Init()` if you need to add a potential code to
  // `RequestUpdate()`. The init method will be
  // called right after all the main states are created.
  explicit StateProvider(Profile* profile, StateObserver* state_observer)
      : profile_(*profile), state_observer_(*state_observer) {}

  // TODO(b/324018028): Consider changing `IsActive()` to be non-virtual and
  // return a member variable `is_active_` that can be controlled by the derived
  // classes that sets the active/inactive state when needed, also requesting
  // updates on state change. This way we would make sure not to miss updates
  // when a state activation changes.
  virtual bool IsActive() const = 0;

  // This method should be used to initialize anything that could potentially
  // call a `RequestUpdate()` which would end up computing the active state.
  // This method will be called after all main states are created, making sure
  // that an active state will be correctly computed.
  virtual void Init() {}

  // Returns the text to be shown on the button.
  virtual std::u16string GetText() const = 0;

  // Returns the highlight color of the button.
  virtual std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const {
    return color_provider.GetColor(kColorAvatarButtonHighlightDefault);
  }

  // Returns the text color of the button.
  virtual std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& color_provider) const {
    return color_provider.GetColor(
        kColorAvatarButtonHighlightDefaultForeground);
  }

  // Returns the avatar icon.
  virtual ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor /*icon_color*/,
      const ui::ColorProvider& color_provider) const {
    return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
        GetProfileAvatarImage(profile(), color_provider, icon_size), icon_size,
        icon_size, profiles::SHAPE_CIRCLE));
  }

  // Returns the tooltip text of the avatar button.
  virtual std::u16string GetAvatarTooltipText() const {
    return profiles::GetAvatarNameForProfile(profile().GetPath());
  }

  // Returns ink drop colors as a pair of hover and ripple colors of the
  // button.
  virtual std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const {
    return {kColorToolbarInkDropHover, kColorToolbarInkDropRipple};
  }

  // Returns whether the border should be painted for the button.
  virtual bool ShouldPaintBorder() const { return false; }

  // Returns the accessibility label of the button.
  virtual std::optional<std::u16string> GetAccessibilityLabel() const {
    return std::nullopt;
  }

  // Returns the action to be used when the button is pressed. This is used to
  // override the default action of the button (defined by
  // `AvatarToolbarButtonDelegate`) when it is pressed.
  virtual std::optional<base::RepeatingCallback<void(bool)>>
  GetButtonActionOverride() {
    return std::nullopt;
  }

  // This update request will attempt to update the text shown on the button.
  // The update will only go through if the requesting state was the main button
  // active one and is now inactive or if it is currently the main active one.
  // Therefore every time a `StateProvider` expects a change of internal state
  // it should call this method to attempt to propagate the changes.
  void RequestUpdate() { state_observer_->OnStateProviderUpdateRequest(this); }

  virtual ~StateProvider() = default;

 protected:
  Profile& profile() const { return profile_.get(); }

 private:
  const raw_ref<Profile> profile_;
  const raw_ref<StateObserver> state_observer_;
};

class PrivateBaseStateProvider : public StateProvider,
                                 public BrowserListObserver {
 public:
  explicit PrivateBaseStateProvider(Profile* profile,
                                    StateObserver* state_observer)
      : StateProvider(profile, state_observer) {
    scoped_browser_list_observation_.Observe(BrowserList::GetInstance());
  }
  ~PrivateBaseStateProvider() override = default;

  // StateProvider:
  bool IsActive() const final {
    // This state is always active when the Profile is in private mode, the
    // Profile type is not expected to change.
    return true;
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) final { RequestUpdate(); }
  void OnBrowserRemoved(Browser* browser) final { RequestUpdate(); }

 private:
  base::ScopedObservation<BrowserList, BrowserListObserver>
      scoped_browser_list_observation_{this};
};

class GuestStateProvider : public PrivateBaseStateProvider {
 public:
  explicit GuestStateProvider(Profile* profile, StateObserver* state_observer)
      : PrivateBaseStateProvider(profile, state_observer) {}

  ~GuestStateProvider() override = default;

  // StateProvider:
  std::u16string GetText() const override {
#if BUILDFLAG(IS_CHROMEOS)
    // On ChromeOS all windows are either Guest or not Guest and the Guest
    // avatar button is not actionable. Showing the number of open windows is
    // not as helpful as on other desktop platforms. Please see
    // crbug.com/1178520.
    const int guest_window_count = 1;
#else
    const int guest_window_count = BrowserList::GetGuestBrowserCount();
#endif
    return l10n_util::GetPluralStringFUTF16(IDS_AVATAR_BUTTON_GUEST,
                                            guest_window_count);
  }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(kColorAvatarButtonHighlightGuest);
  }

  std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(kColorAvatarButtonHighlightGuestForeground);
  }

  ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor /*icon_color*/,
      const ui::ColorProvider& /*color_provider*/) const override {
    return profiles::GetGuestAvatar(icon_size);
  }

  std::u16string GetAvatarTooltipText() const override {
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_GUEST_TOOLTIP);
  }

  bool ShouldPaintBorder() const override { return true; }
};

class IncognitoStateProvider : public PrivateBaseStateProvider {
 public:
  explicit IncognitoStateProvider(Profile* profile,
                                  StateObserver* state_observer)
      : PrivateBaseStateProvider(profile, state_observer) {}

  ~IncognitoStateProvider() override = default;

  // StateProvider:
  std::u16string GetText() const override {
    return l10n_util::GetPluralStringFUTF16(
        IDS_AVATAR_BUTTON_INCOGNITO,
        BrowserList::GetOffTheRecordBrowsersActiveForProfile(&profile()));
  }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(kColorAvatarButtonHighlightIncognito);
  }

  std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(
        kColorAvatarButtonHighlightIncognitoForeground);
  }

  ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor icon_color,
      const ui::ColorProvider& /*color_provider*/) const override {
    return ui::ImageModel::FromVectorIcon(kIncognitoRefreshMenuIcon, icon_color,
                                          icon_size);
  }

  std::u16string GetAvatarTooltipText() const override {
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);
  }

  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const override {
    return {kColorAvatarButtonIncognitoHover, kColorToolbarInkDropRipple};
  }
};

class ExplicitStateProvider : public StateProvider {
 public:
  explicit ExplicitStateProvider(
      Profile* profile,
      StateObserver* state_observer,
      std::u16string explicit_text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool)>> explicit_action)
      : StateProvider(profile, state_observer),
        explicit_text_(std::move(explicit_text)),
        accessibility_label_(std::move(accessibility_label)),
        explicit_action_(std::move(explicit_action)) {}
  ~ExplicitStateProvider() override = default;

  // StateProvider:
  bool IsActive() const override { return active_; }

  std::u16string GetText() const override { return explicit_text_; }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(kColorAvatarButtonHighlightExplicitText);
  }

  std::optional<std::u16string> GetAccessibilityLabel() const override {
    return accessibility_label_;
  }

  std::optional<base::RepeatingCallback<void(bool)>> GetButtonActionOverride()
      override {
    return explicit_action_;
  }

  // Used as the callback closure to the setter of the explicit state,
  // or when overriding the explicit state by another one.
  void Clear() {
    active_ = false;
    RequestUpdate();
  }

  base::WeakPtr<ExplicitStateProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool active_ = true;

  const std::u16string explicit_text_;
  const std::optional<std::u16string> accessibility_label_;

  // The explicit action to be used when the button is pressed.
  std::optional<base::RepeatingCallback<void(bool)>> explicit_action_;

  base::WeakPtrFactory<ExplicitStateProvider> weak_ptr_factory_{this};
};

class ShowIdentityNameStateProvider : public StateProvider,
                                      public signin::IdentityManager::Observer,
                                      public AvatarToolbarButton::Observer {
 public:
  explicit ShowIdentityNameStateProvider(
      Profile* profile,
      StateObserver* state_observer,
      AvatarToolbarButton* avatar_toolbar_button)
      : StateProvider(profile, state_observer),
        avatar_toolbar_button_(*avatar_toolbar_button) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    CHECK(identity_manager);
    identity_manager_observation_.Observe(identity_manager);
    avatar_button_observation_.Observe(avatar_toolbar_button);
  }

  ~ShowIdentityNameStateProvider() override {
    avatar_button_observation_.Reset();
  }

  // StateProvider:
  bool IsActive() const override { return show_identity_request_count_ > 0; }

  void Init() override {
    if (IdentityManagerFactory::GetForProfile(&profile())
            ->AreRefreshTokensLoaded()) {
      // Will potentially call a `RequestUpdate()`.
      OnRefreshTokensLoaded();
    }
  }

  std::u16string GetText() const override {
    return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_GREETING,
                                      GetShortProfileName(profile()));
  }

  std::u16string GetAvatarTooltipText() const override {
    return GetShortProfileName(profile());
  }

  // IdentityManager::Observer:
  // Needed if the first sync promo account should be displayed.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
        signin::PrimaryAccountChangeEvent::Type::kSet) {
      return;
    }
    OnUserIdentityChanged();
  }

  void OnRefreshTokensLoaded() override {
    // TODO(b/324018028): This check can be removed as `OnRefreshTokensLoaded()`
    // is called when first observing and not as a result of
    // `IdentityManager::OnRefreshTokensLoaded()`. So double call should not
    // happen anymore.
    if (refresh_tokens_loaded_) {
      // This is possible, if `AvatarToolbarButtonDelegate`  constructor  is
      // called within the loop in
      //  `IdentityManager::OnRefreshTokensLoaded()` to notify observers. In
      //  that case, |OnRefreshTokensLoaded| will be called twice, once from
      //  AvatarToolbarButtonDelegate` constructor and another time from the
      //  `IdentityManager`. This happens for new signed in profiles. See
      //  https://crbug.com/1035480
      return;
    }

    refresh_tokens_loaded_ = true;
    if (!signin_ui_util::ShouldShowAnimatedIdentityOnOpeningWindow(profile())) {
      return;
    }

    if (!IdentityManagerFactory::GetForProfile(&profile())
             ->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      return;
    }

    OnUserIdentityChanged();
  }

  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    identity_manager_observation_.Reset();
  }

  // AvatarToolbarButton::Observer
  void OnMouseExited() override { MaybeHideIdentityAnimation(); }

  void OnBlur() override { MaybeHideIdentityAnimation(); }

  void OnIPHPromoChanged(bool has_promo) override {
    if (has_in_product_help_promo_ == has_promo) {
      return;
    }

    has_in_product_help_promo_ = has_promo;
    // Trigger a new animation, even if the IPH is being removed. This keeps the
    // pill open a little more and avoids jankiness caused by the two animations
    // (IPH and identity pill) happening concurrently.
    // See https://crbug.com/1198907
    ShowIdentityName();
  }

  void OnIconUpdated() override {
    // Try to show the name if we were waiting for an image.
    MaybeShowIdentityName();
  }

  void ForceDelayTimeoutForTesting() { OnIdentityAnimationTimeout(); }

 private:
  // Initiates showing the identity.
  void OnUserIdentityChanged() {
    signin_ui_util::RecordAnimatedIdentityTriggered(&profile());
    // On any following icon update the name will be attempted to be shown when
    // the image is ready.
    waiting_for_image_ = true;
    MaybeShowIdentityName();
  }

  // Should be called when the icon is updated. This may trigger the showing of
  // the identity name.
  void MaybeShowIdentityName() {
    if (!waiting_for_image_ || GetGaiaAccountImage(&profile()).IsEmpty()) {
      return;
    }

    // Check that the user is still signed in. See https://crbug.com/1025674
    if (!IdentityManagerFactory::GetForProfile(&profile())
             ->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      Clear();
      return;
    }

    ShowIdentityName();
  }

  // Shows the name in the identity pill. If the name is already showing, this
  // extends the duration.
  void ShowIdentityName() {
    // Do not show the identity name if the enterprise badging is enabled for
    // the avatar.
    if (enterprise_util::CanShowEnterpriseBadgingForAvatar(&profile())) {
      return;
    }

    ++show_identity_request_count_;
    waiting_for_image_ = false;

    RequestUpdate();

    // Hide the pill after a while.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ShowIdentityNameStateProvider::OnIdentityAnimationTimeout,
            weak_ptr_factory_.GetWeakPtr()),
        g_show_name_duration_for_testing.value_or(kShowNameDuration));
  }

  void OnIdentityAnimationTimeout() {
    --show_identity_request_count_;
    MaybeHideIdentityAnimation();
  }

  // Called after the user interacted with the button or after some timeout.
  void MaybeHideIdentityAnimation() {
    if (show_identity_request_count_ > 0) {
      return;
    }

    // Keep identity visible if this button is in use (hovered or has focus) or
    // has an associated In-Product-Help promo. We should not move things around
    // when the user wants to click on `this` or another button in the parent.
    if (avatar_toolbar_button_->IsMouseHovered() ||
        avatar_toolbar_button_->HasFocus() || has_in_product_help_promo_) {
      return;
    }

    Clear();
  }

  // Clears the effects of the state being active.
  void Clear() {
    show_identity_request_count_ = 0;
    waiting_for_image_ = false;
    show_identity_request_count_ = false;
    has_in_product_help_promo_ = false;

    RequestUpdate();
  }

  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  // Count of the show identity pill name timeouts that are currently scheduled.
  // Multiple timeouts are scheduled when multiple show requests triggers happen
  // in a quick sequence (before the first timeout passes). The identity pill
  // tries to close when this reaches 0.
  int show_identity_request_count_ = 0;
  bool waiting_for_image_ = false;
  bool has_in_product_help_promo_ = false;
  bool refresh_tokens_loaded_ = false;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_button_observation_{this};

  base::WeakPtrFactory<ShowIdentityNameStateProvider> weak_ptr_factory_{this};
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class HistorySyncOptinCoordinator : public base::SupportsUserData::Data,
                                    public StateManagerObserver,
                                    public signin::IdentityManager::Observer {
 public:
  static HistorySyncOptinCoordinator& GetOrCreateForProfile(Profile& profile) {
    HistorySyncOptinCoordinator* coordinator =
        static_cast<HistorySyncOptinCoordinator*>(
            profile.GetUserData(kHistorySyncOptinCoordinatorKey));
    if (!coordinator) {
      coordinator = new HistorySyncOptinCoordinator(profile);
      profile.SetUserData(kHistorySyncOptinCoordinatorKey,
                          base::WrapUnique(coordinator));
    }
    return *coordinator;
  }

  bool triggered() const { return triggered_; }

  signin_metrics::AccessPoint access_point() const { return access_point_; }

  base::CallbackListSubscription AddStateChangedCallback(
      base::RepeatingClosure callback) {
    return state_changed_callbacks.Add(std::move(callback));
  }

  void PromoUsed() {
    CHECK(before_promo_used_elapsed_timer_.has_value());
    base::UmaHistogramMediumTimes(
        "Signin.SyncOptIn.IdentityPill.DurationBeforeClick",
        before_promo_used_elapsed_timer_->Elapsed());
    sync_promo_identity_pill_manager_.RecordPromoUsed();
    Collapse();
  }

  void ForceDelayTimeoutForTesting() { Collapse(); }

  // StateManagerObserver:
  void OnButtonStateChanged(std::optional<ButtonState> old_state,
                            ButtonState new_state) override {
    switch (new_state) {
      case ButtonState::kHistorySyncOptin:
        PromoShown();
        return;
      case ButtonState::kUpgradeClientError:
      case ButtonState::kPassphraseError:
      case ButtonState::kSyncError:
      case ButtonState::kSigninPending:
      case ButtonState::kSyncPaused:
      case ButtonState::kExplicitTextShowing:
        Collapse();
        return;
      case ButtonState::kShowIdentityName:
      case ButtonState::kIncognitoProfile:
      case ButtonState::kGuestSession:
        break;
      case ButtonState::kNormal:
      case ButtonState::kManagement:
        CHECK(!collapse_timer_.IsRunning());
        break;
    }
    if (!old_state.has_value()) {
      return;
    }
    switch (*old_state) {
      case ButtonState::kShowIdentityName:
        // `ShowIdentityName` state should be followed by `HistorySyncOptin`
        // state.
        Trigger(signin_metrics::AccessPoint::
                    kHistorySyncOptinExpansionPillOnStartup);
        break;
      case ButtonState::kIncognitoProfile:
      case ButtonState::kGuestSession:
      case ButtonState::kNormal:
      case ButtonState::kExplicitTextShowing:
      case ButtonState::kHistorySyncOptin:
      case ButtonState::kSyncError:
      case ButtonState::kManagement:
      case ButtonState::kSigninPending:
      case ButtonState::kSyncPaused:
      case ButtonState::kUpgradeClientError:
      case ButtonState::kPassphraseError:
        break;
    }
  }

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& /*event*/) override {
    if (!signin_util::ShouldShowHistorySyncOptinScreen(profile_.get())) {
      // Needed to prevent the promo from showing when it is already triggered
      // and the user sign out or turns on sync without dismissing the promo.
      Collapse();
    }
  }

  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    identity_manager_observation_.Reset();
  }

 private:
  constexpr static const void* const kHistorySyncOptinCoordinatorKey =
      &kHistorySyncOptinCoordinatorKey;

  explicit HistorySyncOptinCoordinator(Profile& profile)
      : profile_(profile), sync_promo_identity_pill_manager_(profile) {
    UserEducationService* user_education_service =
        UserEducationServiceFactory::GetForBrowserContext(&profile_.get());
    CHECK(user_education_service);
    new_session_callback_subscription_ =
        user_education_service->user_education_session_manager()
            .AddNewSessionCallback(base::BindRepeating(
                &HistorySyncOptinCoordinator::OnNewSession,
                // This is safe because `HistorySyncOptinCoordinator`
                // owns `CallbackListSubscription`.
                base::Unretained(this)));
    identity_manager_observation_.Observe(
        IdentityManagerFactory::GetForProfile(&profile));
  }

  void Trigger(signin_metrics::AccessPoint access_point) {
    if (triggered_) {
      return;
    }
    if (!sync_promo_identity_pill_manager_.ShouldShowPromo()) {
      return;
    }
    if (!signin_util::ShouldShowHistorySyncOptinScreen(profile_.get())) {
      return;
    }
    access_point_ = access_point;
    triggered_ = true;
    state_changed_callbacks.Notify();
  }

  void Collapse() {
    if (!triggered_) {
      return;
    }
    if (collapse_timer_.IsRunning()) {
      collapse_timer_.Stop();
    }
    triggered_ = false;
    before_promo_used_elapsed_timer_.reset();
    state_changed_callbacks.Notify();
  }

  void PromoShown() {
    if (collapse_timer_.IsRunning()) {
      // This prevents starting a new timer when the button state changes to
      // `HistorySyncOptin` in the next browser window(s).
      return;
    }
    before_promo_used_elapsed_timer_.emplace();
    has_been_shown_since_startup_ = true;
    sync_promo_identity_pill_manager_.RecordPromoShown();
    base::UmaHistogramEnumeration("Signin.SyncOptIn.IdentityPill.Shown",
                                  access_point_);
    collapse_timer_.Start(FROM_HERE,
                          g_history_sync_optin_duration_for_testing.value_or(
                              kHistorySyncOptinDuration),
                          base::BindOnce(&HistorySyncOptinCoordinator::Collapse,
                                         // This is safe because
                                         // `HistorySyncOptinStateProvider`
                                         // owns `clear_timer_`.
                                         base::Unretained(this)));
  }

  void OnNewSession() {
    // NOTE: All history sync opt-in triggers for enterprise badging are
    // considered "on inactivity" (`kHistorySyncOptinExpansionPillOnInactivity`
    // access point).
    if (!enterprise_util::CanShowEnterpriseBadgingForAvatar(&profile_.get())) {
      if (!has_been_shown_since_startup_) {
        // If the history sync opt-in has not been shown since startup,
        // do NOT trigger it. This avoids a subtle race condition on startup
        // when the greetings are about to show roughly at the same time as the
        // new session is detected (greetings are followed by the history sync
        // opt-in anyway).
        //
        // NOTE: We assume that we are notified about the new session before the
        // first history sync opt-in collapses (~60 seconds).
        return;
      }
    }
    Trigger(signin_metrics::AccessPoint::
                kHistorySyncOptinExpansionPillOnInactivity);
  }

  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::kUnknown;
  bool triggered_ = false;
  bool has_been_shown_since_startup_ = false;
  base::OneShotTimer collapse_timer_;

  // Timer to measure the time between the promo being shown and used (clicked).
  std::optional<base::ElapsedTimer> before_promo_used_elapsed_timer_;

  const raw_ref<Profile> profile_;

  signin::SyncPromoIdentityPillManager sync_promo_identity_pill_manager_;

  // New (user education) session callback subscription. The callback is
  // triggered whenever a new user education session starts (i.e. after a
  // 'certain' period of inactivity, see
  // `user_education::features::GetIdleTimeBetweenSessions()`).
  base::CallbackListSubscription new_session_callback_subscription_;

  // Callbacks to be triggered when the history sync opt-in state (`triggered_`)
  // changes.
  base::RepeatingCallbackList<void()> state_changed_callbacks;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

class HistorySyncOptinStateProvider : public StateProvider {
 public:
  explicit HistorySyncOptinStateProvider(Browser* browser,
                                         StateObserver* state_observer)
      : StateProvider(browser->profile(), state_observer),
        coordinator_(HistorySyncOptinCoordinator::GetOrCreateForProfile(
            *browser->profile())),
        browser_(*browser) {}
  ~HistorySyncOptinStateProvider() override = default;

  // StateProvider:
  bool IsActive() const override { return coordinator_->triggered(); }

  std::u16string GetText() const override {
    switch (switches::kHistorySyncOptinExpansionPillOption.Get()) {
      case switches::HistorySyncOptinExpansionPillOption::kBrowseAcrossDevices:
      case switches::HistorySyncOptinExpansionPillOption::
          kBrowseAcrossDevicesNewProfileMenuPromoVariant:
        return l10n_util::GetStringUTF16(
            IDS_AVATAR_BUTTON_BROWSE_ACROSS_DEVICES);
      case switches::HistorySyncOptinExpansionPillOption::kSyncHistory:
        return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_HISTORY);
      case switches::HistorySyncOptinExpansionPillOption::
          kSeeTabsFromOtherDevices:
        return l10n_util::GetStringUTF16(
            IDS_AVATAR_BUTTON_SEE_TABS_FROM_OTHER_DEVICES);
    }
  }

  void Init() override {
    state_changed_callback_subscription_ =
        coordinator_->AddStateChangedCallback(
            base::BindRepeating(&HistorySyncOptinStateProvider::RequestUpdate,
                                base::Unretained(this)));
    if (coordinator_->triggered()) {
      RequestUpdate();
    }
  }

  std::optional<base::RepeatingCallback<void(bool)>> GetButtonActionOverride()
      override {
    return base::BindRepeating(
        &HistorySyncOptinStateProvider::OnButtonClick,
        // This is safe because `AvatarToolbarButtonDelegate`
        // owning all the providers owns the callback.
        base::Unretained(this));
  }

  void ForceDelayTimeoutForTesting() {
    coordinator_->ForceDelayTimeoutForTesting();
  }

 private:
  void OnButtonClick(bool is_source_accelerator) {
    browser_->GetFeatures().profile_menu_coordinator()->Show(
        is_source_accelerator, coordinator_->access_point());
    coordinator_->PromoUsed();
  }

  // History sync opt-in coordinator state change callback subscription.
  // The callbacks are used to notify the state provider(s) when the history
  // sync opt-in state changes.
  base::CallbackListSubscription state_changed_callback_subscription_;

  raw_ref<HistorySyncOptinCoordinator> coordinator_;

  // This is needed to delay the creation of `ProfileMenuCoordinator`.
  const raw_ref<Browser> browser_;
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// This provider observes sync errors (including transport mode). It can be
// configured to listen to a specific error with `sync_error_type`, or to all
// errors by passing nullopt. That way specific implementations of
// `SyncErrorBaseStateProvider`s can handle a specific sync error, while an
// implementation passing `std::nullopt` with lower priority can handle the
// remaining errors.
class SyncErrorBaseStateProvider : public StateProvider,
                                   public syncer::SyncServiceObserver {
 public:
  struct AvatarError {
    AvatarSyncErrorType avatar_error = AvatarSyncErrorType::kUpgradeClientError;
    std::string email;

    friend bool operator==(const AvatarError&, const AvatarError&) = default;
  };

  explicit SyncErrorBaseStateProvider(
      Profile* profile,
      StateObserver* state_observer,
      std::optional<AvatarSyncErrorType> sync_error_type)
      : StateProvider(profile, state_observer),
        sync_error_type_(sync_error_type),
        last_avatar_error_(GetAvatarError(profile)) {
    if (auto* sync_service = SyncServiceFactory::GetForProfile(profile)) {
      sync_service_observation_.Observe(sync_service);
    }
  }

  // StateProvider:
  bool IsActive() const final {
    return SyncServiceFactory::IsSyncAllowed(&profile()) &&
           HasError(last_avatar_error_);
  }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(kColorAvatarButtonHighlightSyncPaused);
  }

  ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor /*icon_color*/,
      const ui::ColorProvider& color_provider) const override {
    return GetAvatarImageWithDottedRing(profile(), color_provider, icon_size);
  }

  std::u16string GetAvatarTooltipText() const final {
    const std::optional<AvatarError> error = GetLastAvatarSyncError();
    CHECK(error.has_value());
    return l10n_util::GetStringFUTF16(
        IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP, GetShortProfileName(profile()),
        GetAvatarSyncErrorDescription(
            error->avatar_error,
            IdentityManagerFactory::GetForProfile(&profile())
                ->HasPrimaryAccount(signin::ConsentLevel::kSync),
            error->email));
  }

  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const override {
    return {kColorToolbarInkDropHover, kColorAvatarButtonNormalRipple};
  }

  std::optional<AvatarError> GetLastAvatarSyncError() const {
    return HasError(last_avatar_error_) ? last_avatar_error_ : std::nullopt;
  }

 private:
  // Computes the current avatar error.
  static std::optional<AvatarError> GetAvatarError(Profile* profile) {
    std::optional<AvatarSyncErrorType> error_type =
        ::GetAvatarSyncErrorType(profile);
    if (!error_type) {
      return std::nullopt;
    }

    const syncer::SyncService* service =
        SyncServiceFactory::GetForProfile(profile);
    CHECK(service);

    return AvatarError{error_type.value(), service->GetAccountInfo().email};
  }

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService*) final {
    const std::optional<AvatarError> error = GetAvatarError(&profile());
    if (last_avatar_error_ == error) {
      return;
    }

    bool previous_error_state = HasError(last_avatar_error_);
    bool new_error_state = HasError(error);
    last_avatar_error_ = error;

    if (previous_error_state == new_error_state) {
      return;
    }

    RequestUpdate();
  }

  void OnSyncShutdown(syncer::SyncService*) final {
    sync_service_observation_.Reset();
  }

  // Returns true if `avatar_sync_error` has a value and the value matches
  // `sync_error_type_`. If `sync_error_type_` is std::nullopt then any
  // non-nullopt `avatar_sync_error` is a match.
  bool HasError(const std::optional<AvatarError>& avatar_sync_error) const {
    if (!avatar_sync_error) {
      return false;  // No sync error.
    }

    if (sync_error_type_.has_value() &&
        avatar_sync_error->avatar_error != sync_error_type_) {
      return false;  // Error has the wrong type.
    }

    return true;
  }

  // std::nullopt to be active on all errors.
  const std::optional<AvatarSyncErrorType> sync_error_type_;

  // Caches the value of the last error so the class can detect when it
  // changes and notify changes.
  std::optional<AvatarError> last_avatar_error_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

class SyncPausedStateProvider : public SyncErrorBaseStateProvider {
 public:
  explicit SyncPausedStateProvider(Profile* profile,
                                   StateObserver* state_observer)
      : SyncErrorBaseStateProvider(profile,
                                   state_observer,
                                   AvatarSyncErrorType::kSyncPaused) {}

  ~SyncPausedStateProvider() override = default;

  // StateProvider:
  std::u16string GetText() const override {
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
  }

  ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor icon_color,
      const ui::ColorProvider& color_provider) const override {
    // TODO(crbug.com/40756583): If sync-the-feature is disabled, the icon
    // should be different.
    return StateProvider::GetAvatarIcon(icon_size, icon_color, color_provider);
  }
};

class UpgradeClientErrorStateProvider : public SyncErrorBaseStateProvider {
 public:
  explicit UpgradeClientErrorStateProvider(Profile* profile,
                                           StateObserver* state_observer)
      : SyncErrorBaseStateProvider(profile,
                                   state_observer,
                                   AvatarSyncErrorType::kUpgradeClientError) {}

  ~UpgradeClientErrorStateProvider() override = default;

  // StateProvider:
  std::u16string GetText() const override {
    return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON);
  }
};

class PassphraseErrorStateProvider : public SyncErrorBaseStateProvider {
 public:
  explicit PassphraseErrorStateProvider(Profile* profile,
                                        StateObserver* state_observer)
      : SyncErrorBaseStateProvider(profile,
                                   state_observer,
                                   AvatarSyncErrorType::kPassphraseError) {}

  ~PassphraseErrorStateProvider() override = default;

  // StateProvider:
  std::u16string GetText() const override {
    return l10n_util::GetStringUTF16(
        IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON);
  }
};

class GenericSyncErrorStateProvider : public SyncErrorBaseStateProvider {
 public:
  explicit GenericSyncErrorStateProvider(Profile* profile,
                                         StateObserver* state_observer)
      : SyncErrorBaseStateProvider(profile,
                                   state_observer,
                                   /*sync_error_type=*/std::nullopt) {}

  ~GenericSyncErrorStateProvider() override = default;

  // StateProvider:
  std::u16string GetText() const override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile());
    CHECK(identity_manager);
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
    }
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED);
  }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile());
    CHECK(identity_manager);
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      return color_provider.GetColor(kColorAvatarButtonHighlightSyncError);
    }
    return color_provider.GetColor(kColorAvatarButtonHighlightSigninPaused);
  }

  std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& color_provider) const override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile());
    CHECK(identity_manager);
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      return color_provider.GetColor(
          kColorAvatarButtonHighlightSyncErrorForeground);
    }
    return SyncErrorBaseStateProvider::GetHighlightTextColor(color_provider);
  }

  ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor icon_color,
      const ui::ColorProvider& color_provider) const override {
    if (IdentityManagerFactory::GetForProfile(&profile())
            ->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      return StateProvider::GetAvatarIcon(icon_size, icon_color,
                                          color_provider);
    }
    return SyncErrorBaseStateProvider::GetAvatarIcon(icon_size, icon_color,
                                                     color_provider);
  }

  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile());
    CHECK(identity_manager);
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
      return StateProvider::GetInkdropColors();
    }
    return SyncErrorBaseStateProvider::GetInkdropColors();
  }
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const void* const kSigninPendingTimestampStartKey =
    &kSigninPendingTimestampStartKey;

// Helper struct to store a `base::TimeTicks` as a Profile user data.
struct TimeStampData : public base::SupportsUserData::Data {
  explicit TimeStampData(base::Time time) : time_(time) {}
  base::Time time_;
};

// This state has two modes when active; extended and collapsed. This states is
// active when the Signed in account is in error. Based on the source of the
// error, a mode is active:
// - collapsed: error originates from a web signout action from the user, the
// avatar button will not show a text.
// - extended version: any other error or after 50 minutes past a web signout or
// on Chrome restart, the button will extend to show a "Verify it's you" text.
//
// In both modes, the avatar icon is shrunk slightly and surrounded by a dotted
// circle to show the pending state.
class SigninPendingStateProvider : public StateProvider,
                                   public signin::IdentityManager::Observer {
 public:
  explicit SigninPendingStateProvider(
      Profile* profile,
      StateObserver* state_observer,
      const AvatarToolbarButton* avatar_toolbar_button)
      : StateProvider(profile, state_observer),
        identity_manager_(*IdentityManagerFactory::GetForProfile(profile)),
        avatar_toolbar_button_(*avatar_toolbar_button) {
    identity_manager_observation_.Observe(&identity_manager_.get());

    TimeStampData* signed_in_pending_delay_start = static_cast<TimeStampData*>(
        profile->GetUserData(kSigninPendingTimestampStartKey));
    // If a delay to show the pending state text was already started by another
    // browser, start one with the remaining time.
    if (signed_in_pending_delay_start) {
      base::TimeDelta elapsed_delay_time =
          base::Time::Now() - signed_in_pending_delay_start->time_;
      const base::TimeDelta delay =
          g_show_signin_pending_text_delay_for_testing.value_or(
              kShowSigninPendingTextDelay);
      if (elapsed_delay_time < delay) {
        StartTimerDelay(delay - elapsed_delay_time);
      } else {
        // This can happen if all browsers were closed when the delay expired,
        // and the cleanup task could not be run. Remove the user data now.
        profile->RemoveUserData(kSigninPendingTimestampStartKey);
      }
    }
  }

  // StateProvider:
  bool IsActive() const override {
    CoreAccountId primary_account_id =
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    if (primary_account_id.empty()) {
      return false;
    }

    return identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
        primary_account_id);
  }

  std::u16string GetText() const override {
    if (!ShouldShowText()) {
      return std::u16string();
    }
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_PAUSED);
  }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    if (!ShouldShowText()) {
      return StateProvider::GetHighlightColor(color_provider);
    }
    return color_provider.GetColor(kColorAvatarButtonHighlightSigninPaused);
  }

  ui::ImageModel GetAvatarIcon(
      int icon_size,
      SkColor /*icon_color*/,
      const ui::ColorProvider& color_provider) const override {
    return GetAvatarImageWithDottedRing(profile(), color_provider, icon_size);
  }

  std::optional<std::u16string> GetAccessibilityLabel() const override {
    return l10n_util::GetStringUTF16(
        IDS_AVATAR_BUTTON_SIGNIN_PENDING_ACCESSIBILITY_LABEL);
  }

  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const override {
    return {kColorToolbarInkDropHover, kColorAvatarButtonNormalRipple};
  }

  // Only show the text when the delay timer is not running.
  bool ShouldShowText() const { return !display_text_delay_timer_.IsRunning(); }

  void ForceTimerTimeoutForTesting() {
    display_text_delay_timer_.FireNow();
    display_text_delay_timer_.Stop();
  }

 private:
  // signin::IdentityManager::Observer:
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override {
    if (account_info != identity_manager_->GetPrimaryAccountInfo(
                            signin::ConsentLevel::kSignin)) {
      return;
    }

    if (!error.IsPersistentError() && display_text_delay_timer_.IsRunning()) {
      // Clear timer and make it reaches the end. Next update should make the
      // state inactive.
      display_text_delay_timer_.Reset();
      OnTimerDelayReached();
      return;
    }

    if (error.state() ==
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS &&
        token_operation_source ==
            signin_metrics::SourceForRefreshTokenOperation::
                kDiceResponseHandler_Signout) {
      profile().SetUserData(kSigninPendingTimestampStartKey,
                            std::make_unique<TimeStampData>(base::Time::Now()));
      StartTimerDelay(g_show_signin_pending_text_delay_for_testing.value_or(
          kShowSigninPendingTextDelay));
    }

    RequestUpdate();
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    RequestUpdate();
  }

  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    identity_manager_observation_.Reset();
  }

  void StartTimerDelay(base::TimeDelta delay) {
    display_text_delay_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&SigninPendingStateProvider::OnTimerDelayReached,
                       // Unretained is fine here since the object owns the
                       // timer which will not fire if destroyed.
                       base::Unretained(this)));
  }

  void OnTimerDelayReached() {
    profile().RemoveUserData(kSigninPendingTimestampStartKey);
    RequestUpdate();
  }

  raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::OneShotTimer display_text_delay_timer_;
};

class ManagementStateProvider : public StateProvider,
                                public ProfileAttributesStorage::Observer,
                                public policy::ManagementService::Observer,
                                public BrowserListObserver {
 public:
  explicit ManagementStateProvider(
      Profile* profile,
      StateObserver* state_observer,
      const AvatarToolbarButton* avatar_toolbar_button)
      : StateProvider(profile, state_observer),
        avatar_toolbar_button_(*avatar_toolbar_button) {
    BrowserList::AddObserver(this);
    profile_observation_.Observe(&GetProfileAttributesStorage());
    management_observation_.Observe(
        policy::ManagementServiceFactory::GetForProfile(profile));
  }

  ~ManagementStateProvider() override { BrowserList::RemoveObserver(this); }

  // StateProvider:
  bool IsActive() const override {
    return enterprise_util::CanShowEnterpriseBadgingForAvatar(&profile());
  }

  std::u16string GetText() const override {
    return enterprise_util::GetEnterpriseLabel(&profile(), /*truncated=*/true);
  }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(kColorAvatarButtonHighlightManagement);
  }

  std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& color_provider) const override {
    return color_provider.GetColor(
        kColorAvatarButtonHighlightManagementForeground);
  }

  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const override {
    return {kColorToolbarInkDropHover, kColorAvatarButtonNormalRipple};
  }

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser*) override {
    // This is required so that the enterprise text is shown when a profile is
    // opened.
    RequestUpdate();
  }

  // ProfileAttributesStorage::Observer:
  void OnProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) override {
    RequestUpdate();
  }

  // ManagementService::Observer
  void OnEnterpriseLabelUpdated() override { RequestUpdate(); }

  const raw_ref<const AvatarToolbarButton> avatar_toolbar_button_;

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};

  base::ScopedObservation<policy::ManagementService,
                          policy::ManagementService::Observer>
      management_observation_{this};

  base::WeakPtrFactory<ManagementStateProvider> weak_ptr_factory_{this};
};
#endif

// Regular State, should always have the lowest priority.
class NormalStateProvider : public StateProvider {
 public:
  explicit NormalStateProvider(Profile* profile, StateObserver* state_observer)
      : StateProvider(profile, state_observer) {}

  // StateProvider:
  bool IsActive() const override {
    // Normal state is always active.
    return true;
  }

  std::u16string GetText() const override { return std::u16string(); }

  std::optional<SkColor> GetHighlightColor(
      const ui::ColorProvider& /*color_provider*/) const override {
    return std::nullopt;
  }

  std::optional<SkColor> GetHighlightTextColor(
      const ui::ColorProvider& /*color_provider*/) const override {
    return std::nullopt;
  }
};

}  // namespace

// Container of all the states and returns the active state with the highest
// priority.
// All states are initialized at construction based on the Profile type.
// Exception for `ButtonState::kExplicitTextShowing` with
// `ExplicitStateProvider`  which is the only state that can be added
// dynamically and controlled externally. It has to be part of the
// `StateManager` however to properly compute the current active state.
// This class also listens to Profile changes that should affect the global
// state of the button, for chanhges that should occur regardless of the current
// active state for Regular Profiles.
class StateManager : public StateObserver,
                     public signin::IdentityManager::Observer,
                     public ProfileAttributesStorage::Observer {
 public:
  explicit StateManager(AvatarToolbarButton& avatar_toolbar_button,
                        Browser* browser)
      : avatar_toolbar_button_(avatar_toolbar_button) {
    // Creates the main states and listeners.
    CreateStatesAndListeners(browser);
    ComputeButtonActiveState();
  }
  ~StateManager() override = default;

  // This needs to be separated from the constructor since it might call
  // updates, which will try to access the `StateManager`.
  void InitializeStates() {
    // States should initialize here, making sure that this should happen after
    // all main states are created. This would allow the `Init()` functions of
    // state to call `ComputeButtonActiveState()`. If this was done in their
    // constructor there could be a chance that no active state exist yet.
    for (auto& state : states_) {
      state.second->Init();
    }
    ComputeButtonActiveState();
  }

  ButtonState GetButtonActiveState() const {
    return current_active_state_pair_->first;
  }

  StateProvider* GetActiveStateProvider() const {
    return current_active_state_pair_->second.get();
  }

  // Special setter for the explicit state as it is controlled externally.
  void SetExplicitStateProvider(
      std::unique_ptr<ExplicitStateProvider> explicit_state_provider) {
    if (auto it = states_.find(ButtonState::kExplicitTextShowing);
        it != states_.end()) {
      // Attempt to clear existing states if not already done.
      static_cast<ExplicitStateProvider*>(it->second.get())->Clear();
    }

    // Invalidate the pointer as the map will reorder it's element when adding a
    // new state and the pointer will not be valid anymore. The value will be
    // set later again with `ComputeButtonActiveState()`.
    current_active_state_pair_ = nullptr;
    // Add the new state.
    states_[ButtonState::kExplicitTextShowing] =
        std::move(explicit_state_provider);

    // Recompute the button active state after adding a new state.
    ComputeButtonActiveState();
    UpdateAvatarButton();
  }

 private:
  // Creates all main states and attach listeners.
  void CreateStatesAndListeners(Browser* browser) {
    // Add each possible state for each Profile type or browser configuration,
    // since this structure is tied to Browser, in which a Profile cannot
    // change, it is correct to initialize the possible fixed states once.

    Profile* profile = browser->profile();

    // Web app has limited toolbar space, thus always show kNormal state.
    if (web_app::AppBrowserController::IsWebApp(browser)) {
      // This state is always active.
      states_[ButtonState::kNormal] = std::make_unique<NormalStateProvider>(
          profile, /*state_observer=*/this);
      return;
    }

    if (profile->IsRegularProfile()) {
      states_[ButtonState::kShowIdentityName] =
          std::make_unique<ShowIdentityNameStateProvider>(
              profile,
              /*state_observer=*/this, &avatar_toolbar_button_.get());

      states_[ButtonState::kUpgradeClientError] =
          std::make_unique<UpgradeClientErrorStateProvider>(
              profile,
              /*state_observer=*/this);
      states_[ButtonState::kPassphraseError] =
          std::make_unique<PassphraseErrorStateProvider>(
              profile,
              /*state_observer=*/this);

      if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
        states_[ButtonState::kSyncPaused] =
            std::make_unique<SyncPausedStateProvider>(profile,
                                                      /*state_observer=*/this);
      }

      // Generic catch-all providers for sync errors not handled by higher
      // priority providers.
      states_[ButtonState::kSyncError] =
          std::make_unique<GenericSyncErrorStateProvider>(
              profile,
              /*state_observer=*/this);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (base::FeatureList::IsEnabled(
              switches::kEnableHistorySyncOptinExpansionPill)) {
        auto history_sync_optin_state_provider =
            std::make_unique<HistorySyncOptinStateProvider>(
                browser,
                /*state_observer=*/this);
        state_manager_observers_.emplace_back(
            HistorySyncOptinCoordinator::GetOrCreateForProfile(*profile));
        states_[ButtonState::kHistorySyncOptin] =
            std::move(history_sync_optin_state_provider);
      }

      if (base::FeatureList::IsEnabled(
              features::kEnterpriseProfileBadgingForAvatar) ||
          base::FeatureList::IsEnabled(
              features::kEnterpriseProfileBadgingPolicies)) {
        // Contains both Work and School.
        states_[ButtonState::kManagement] =
            std::make_unique<ManagementStateProvider>(
                profile,
                /*state_observer=*/this, &avatar_toolbar_button_.get());
      }

      states_[ButtonState::kSigninPending] =
          std::make_unique<SigninPendingStateProvider>(
              profile,
              /*state_observer=*/this, &avatar_toolbar_button_.get());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile);
      scoped_identity_manager_observation_.Observe(identity_manager);
      if (identity_manager->AreRefreshTokensLoaded()) {
        OnRefreshTokensLoaded();
      }
      profile_observation_.Observe(&GetProfileAttributesStorage());

    } else if (profile->IsGuestSession()) {
      // This state is always active.
      states_[ButtonState::kGuestSession] =
          std::make_unique<GuestStateProvider>(profile,
                                               /*state_observer=*/this);
    } else if (profile->IsIncognitoProfile()) {
      // This state is always active.
      states_[ButtonState::kIncognitoProfile] =
          std::make_unique<IncognitoStateProvider>(profile,
                                                   /*state_observer=*/this);
    }

    // This state is always active.
    states_[ButtonState::kNormal] =
        std::make_unique<NormalStateProvider>(profile, /*state_observer=*/this);
  }

  // StateObserver:
  void OnStateProviderUpdateRequest(StateProvider* requesting_state) override {
    if (!requesting_state->IsActive()) {
      // Updates goes through if the requesting state was the current button
      // active state, since we are now clearing it, otherwise we just ignore
      // the request.
      if (current_active_state_pair_->second.get() == requesting_state) {
        // Recompute the new button active state as we are clearing the
        // requesting state effects.
        ComputeButtonActiveState();
        // Always update the button since we do not know exactly which state
        // should now be active.
        UpdateAvatarButton();
      }
      return;
    }

    // Updates `current_active_state_`, and does not alter the states active
    // status. In that case, `requesting_state` remains active at this point but
    // is not necessarily the one with the highest priority.
    ComputeButtonActiveState();
    // Ignore the request if the requested state is not the button active one
    // because the requesting state despite being active, does not have the
    // highest current active priority, meaning that it's update request should
    // not have any effect.
    if (current_active_state_pair_->second.get() != requesting_state) {
      return;
    }
    UpdateAvatarButton();
  }

  // Computes the current active state with the highest priority.
  // Multiple states could be active at the same time.
  void ComputeButtonActiveState() {
    // Traverse the map of states sorted by their priority set in `ButtonState`.
    for (auto& state_pair : states_) {
      // Sets first state that is active.
      if (state_pair.second->IsActive()) {
        std::optional<ButtonState> old_state;
        if (current_active_state_pair_) {
          if (current_active_state_pair_->first == state_pair.first) {
            return;
          }
          old_state = current_active_state_pair_->first;
        }
        current_active_state_pair_ = &state_pair;
        for (auto observer : state_manager_observers_) {
          observer->OnButtonStateChanged(old_state,
                                         current_active_state_pair_->first);
        }
        return;
      }
    }

    NOTREACHED() << "There should at least be one active state in the map.";
  }

  // `AvatarToolbarButton::UpdateIcon()` will notify observers, the
  // `ShowIdentityNameStateProvider` being one of the observers.
  void UpdateButtonIcon() { avatar_toolbar_button_->UpdateIcon(); }

  void UpdateButtonText() { avatar_toolbar_button_->UpdateText(); }

  // This is mainly used `OnStateProviderUpdateRequest()` where not all of the
  // state transitions update all of the button properties. Consider adding a
  // filter if this is impacting performance.
  void UpdateAvatarButton() {
    UpdateButtonText();
    UpdateButtonIcon();
  }

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(signin::IdentityManager*) override {
    scoped_identity_manager_observation_.Reset();
  }

  void OnRefreshTokensLoaded() override { UpdateButtonIcon(); }

  void OnAccountsInCookieUpdated(const signin::AccountsInCookieJarInfo&,
                                 const GoogleServiceAuthError&) override {
    UpdateButtonIcon();
  }

  void OnExtendedAccountInfoUpdated(const AccountInfo&) override {
    UpdateButtonIcon();
  }

  void OnExtendedAccountInfoRemoved(const AccountInfo&) override {
    UpdateButtonIcon();
  }

  //  ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath&) override {
    UpdateButtonIcon();
  }

  void OnProfileHighResAvatarLoaded(const base::FilePath&) override {
    UpdateButtonIcon();
  }

  void OnProfileNameChanged(const base::FilePath&,
                            const std::u16string&) override {
    UpdateButtonText();
  }

  base::flat_map<ButtonState, std::unique_ptr<StateProvider>> states_;
  raw_ref<AvatarToolbarButton> avatar_toolbar_button_;

  // Active state per the last request to `ComputeButtonActiveState()`.
  // Pointer to the active element of `states_` with the highest priority.
  raw_ptr<std::pair<ButtonState, std::unique_ptr<StateProvider>>>
      current_active_state_pair_ = nullptr;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};

  std::vector<raw_ref<StateManagerObserver>> state_manager_observers_;
};

}  // namespace internal

using ButtonState = internal::ButtonState;
using ExplicitStateProvider = internal::ExplicitStateProvider;

AvatarToolbarButtonDelegate::AvatarToolbarButtonDelegate(
    AvatarToolbarButton* button,
    Browser* browser)
    : avatar_toolbar_button_(button),
      browser_(browser),
      profile_(browser->profile()),
      identity_manager_(
          IdentityManagerFactory::GetForProfile(browser->profile())) {
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
#if BUILDFLAG(IS_CHROMEOS)
  // On CrOS this button should only show as badging for Incognito, Guest and
  // captivie portal signin. It's only enabled for non captive portal Incognito
  // where a menu is available for closing all Incognito windows.
  avatar_toolbar_button_->SetEnabled(
      profile_->IsOffTheRecord() && !profile_->IsGuestSession() &&
      !profile_->GetOTRProfileID().IsCaptivePortal());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

AvatarToolbarButtonDelegate::~AvatarToolbarButtonDelegate() = default;

void AvatarToolbarButtonDelegate::InitializeStateManager() {
  CHECK(!state_manager_);
  state_manager_ = std::make_unique<internal::StateManager>(
      *avatar_toolbar_button_, browser_);
  state_manager_->InitializeStates();
}

bool AvatarToolbarButtonDelegate::IsStateManagerInitialized() const {
  return state_manager_.get() != nullptr;
}

void AvatarToolbarButtonDelegate::OnButtonPressed(bool is_source_accelerator) {
  internal::StateProvider* active_state_provider =
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

bool AvatarToolbarButtonDelegate::HasExplicitButtonState() const {
  return state_manager_->GetButtonActiveState() ==
         ButtonState::kExplicitTextShowing;
}

void AvatarToolbarButtonDelegate::OnThemeChanged(
    const ui::ColorProvider* color_provider) {
  // Update avatar color information in profile attributes.
  if (profile_->IsOffTheRecord() || profile_->IsGuestSession()) {
    return;
  }

  // Do not update the profile theme colors if the current browser window is a
  // web app.
  if (web_app::AppBrowserController::IsWebApp(browser_)) {
    return;
  }

  ProfileAttributesEntry* entry = GetProfileAttributesEntry(*profile_);
  if (!entry) {
    return;
  }

  ThemeService* service = ThemeServiceFactory::GetForProfile(profile_);
  if (!service || !color_provider) {
    return;
  }

  // Use default profile colors only for extension and system themes.
  entry->SetProfileThemeColors(
      ShouldUseDefaultProfileColors(*service)
          ? GetDefaultProfileThemeColors(color_provider)
          : GetCurrentProfileThemeColors(*color_provider, *service));
}

base::ScopedClosureRunner AvatarToolbarButtonDelegate::SetExplicitButtonState(
    const std::u16string& text,
    std::optional<std::u16string> accessibility_label,
    std::optional<base::RepeatingCallback<void(bool)>> action) {
  CHECK(!text.empty());

  // Create the new explicit state with the clear text callback.
  std::unique_ptr<ExplicitStateProvider> explicit_state_provider =
      std::make_unique<ExplicitStateProvider>(
          profile_,
          /*state_observer=*/state_manager_.get(), text,
          std::move(accessibility_label), std::move(action));

  ExplicitStateProvider* explicit_state_provider_ptr =
      explicit_state_provider.get();
  // Activate the state.
  state_manager_->SetExplicitStateProvider(std::move(explicit_state_provider));

  return base::ScopedClosureRunner(
      base::BindOnce(&ExplicitStateProvider::Clear,
                     // WeakPtr is needed here since this state could be
                     // replaced before the call to the closure.
                     explicit_state_provider_ptr->GetWeakPtr()));
}

std::pair<std::u16string, std::optional<SkColor>>
AvatarToolbarButtonDelegate::GetTextAndColor(
    const ui::ColorProvider& color_provider) const {
  internal::StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  return {active_state_provider->GetText(),
          active_state_provider->GetHighlightColor(color_provider)};
}

std::optional<std::u16string>
AvatarToolbarButtonDelegate::GetAccessibilityLabel() const {
  internal::StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  return active_state_provider->GetAccessibilityLabel();
}

std::optional<SkColor> AvatarToolbarButtonDelegate::GetHighlightTextColor(
    const ui::ColorProvider& color_provider) const {
  internal::StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  return active_state_provider->GetHighlightTextColor(color_provider);
}

std::u16string AvatarToolbarButtonDelegate::GetAvatarTooltipText() const {
  internal::StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  return active_state_provider->GetAvatarTooltipText();
}

std::pair<ChromeColorIds, ChromeColorIds>
AvatarToolbarButtonDelegate::GetInkdropColors() const {
  if (avatar_toolbar_button_->IsLabelPresentAndVisible()) {
    internal::StateProvider* active_state_provider =
        state_manager_->GetActiveStateProvider();
    CHECK(active_state_provider);
    return active_state_provider->GetInkdropColors();
  }

  return {kColorToolbarInkDropHover, kColorToolbarInkDropRipple};
}

ui::ImageModel AvatarToolbarButtonDelegate::GetAvatarIcon(
    int icon_size,
    SkColor icon_color,
    const ui::ColorProvider& color_provider) const {
  internal::StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  return active_state_provider->GetAvatarIcon(icon_size, icon_color,
                                              color_provider);
}

bool AvatarToolbarButtonDelegate::ShouldPaintBorder() const {
  internal::StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  CHECK(active_state_provider);
  return active_state_provider->ShouldPaintBorder();
}

void AvatarToolbarButtonDelegate::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Try showing the IPH for signin preference remembered.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
          signin::PrimaryAccountChangeEvent::Type::kSet ||
      event_details.GetSetPrimaryAccountAccessPoint() !=
          signin_metrics::AccessPoint::kSigninChoiceRemembered) {
    return;
  }

  GaiaId gaia_id = event_details.GetCurrentState().primary_account.gaia;
  const SigninPrefs signin_prefs(*profile_->GetPrefs());
  std::optional<base::Time> last_signout_time =
      signin_prefs.GetChromeLastSignoutTime(gaia_id);
  if (last_signout_time &&
      base::Time::Now() - last_signout_time.value() < base::Days(14)) {
    // Less than two weeks since the last sign out event.
    return;
  }

  AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
      event_details.GetCurrentState().primary_account);
  if (!account_info.given_name.empty()) {
    avatar_toolbar_button_
        ->MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(account_info);
  } else {
    gaia_id_for_signin_choice_remembered_ = account_info.gaia;
  }
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.gaia == gaia_id_for_signin_choice_remembered_ &&
      !info.given_name.empty()) {
    gaia_id_for_signin_choice_remembered_ = GaiaId();
    avatar_toolbar_button_
        ->MaybeShowExplicitBrowserSigninPreferenceRememberedIPH(info);
  }
}

void AvatarToolbarButtonDelegate::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos) &&
      profile_->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin) &&
      account_info == identity_manager_->GetPrimaryAccountInfo(
                          signin::ConsentLevel::kSignin) &&
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      error.state() ==
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS &&
      token_operation_source == signin_metrics::SourceForRefreshTokenOperation::
                                    kDiceResponseHandler_Signout) {
    avatar_toolbar_button_->MaybeShowWebSignoutIPH(account_info.gaia);
  }
}

// static
base::AutoReset<std::optional<base::TimeDelta>>
AvatarToolbarButtonDelegate::CreateScopedInfiniteDelayOverrideForTesting(
    AvatarDelayType delay_type) {
  switch (delay_type) {
    case AvatarDelayType::kNameGreeting:
      return base::AutoReset<std::optional<base::TimeDelta>>(
          &g_show_name_duration_for_testing, kInfiniteTimeForTesting);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case AvatarDelayType::kSigninPendingText:
      return base::AutoReset<std::optional<base::TimeDelta>>(
          &g_show_signin_pending_text_delay_for_testing,
          kInfiniteTimeForTesting);
    case AvatarDelayType::kHistorySyncOptin:
      return base::AutoReset<std::optional<base::TimeDelta>>(
          &g_history_sync_optin_duration_for_testing, kInfiniteTimeForTesting);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  }
}

void AvatarToolbarButtonDelegate::TriggerTimeoutForTesting(
    AvatarDelayType delay_type) {
  switch (delay_type) {
    case AvatarDelayType::kNameGreeting:
      if (state_manager_->GetButtonActiveState() ==
          ButtonState::kShowIdentityName) {
        auto* show_identity_state =
            static_cast<internal::ShowIdentityNameStateProvider*>(
                state_manager_->GetActiveStateProvider());
        CHECK(show_identity_state);
        show_identity_state->ForceDelayTimeoutForTesting();  // IN-TEST
      }
      break;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case AvatarDelayType::kSigninPendingText:
      if (state_manager_->GetButtonActiveState() ==
          ButtonState::kSigninPending) {
        auto* signin_pending_state =
            static_cast<internal::SigninPendingStateProvider*>(
                state_manager_->GetActiveStateProvider());
        CHECK(signin_pending_state);
        signin_pending_state->ForceTimerTimeoutForTesting();  // IN-TEST
      }
      break;
    case AvatarDelayType::kHistorySyncOptin:
      if (state_manager_->GetButtonActiveState() ==
          ButtonState::kHistorySyncOptin) {
        auto* history_sync_optin_state =
            static_cast<internal::HistorySyncOptinStateProvider*>(
                state_manager_->GetActiveStateProvider());
        CHECK(history_sync_optin_state);
        history_sync_optin_state->ForceDelayTimeoutForTesting();  // IN-TEST
      }
      break;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  }
}

// static
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
base::AutoReset<std::optional<base::TimeDelta>> AvatarToolbarButtonDelegate::
    CreateScopedZeroDelayOverrideSigninPendingTextForTesting() {
  return base::AutoReset<std::optional<base::TimeDelta>>(
      &g_show_signin_pending_text_delay_for_testing, base::Seconds(0));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
