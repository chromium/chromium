// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.subscription_eligibility.SubscriptionEligibilityService;
import org.chromium.chrome.browser.subscription_eligibility.SubscriptionEligibilityServiceFactory;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/**
 * The mediator for a signin button on the NTP toolbar. Listens for sign in state changes and drives
 * corresponding changes to the property model values which back the SigninButton view
 */
@NullMarked
final class SigninButtonMediator
        implements ProfileDataCache.Observer,
                IdentityManager.Observer,
                SyncService.SyncStateChangedListener,
                SigninManager.SignInStateObserver,
                BottomSheetSigninAndHistorySyncCoordinator.Delegate,
                TintObserver {
    // TODO(crbug.com/475816843): Make this non-static and non-final when re-introducing the sign-in
    // text button.
    private static final boolean SHOW_AVATAR_WHEN_SIGNED_OUT = true;

    private final Context mContext;
    private final PropertyModel mModel;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;
    private final WindowAndroid mWindowAndroid;
    private final ActivityResultTracker mActivityResultTracker;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final SnackbarManager mSnackbarManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private final Runnable mOnSigninTapped;

    private @Nullable Profile mProfile;
    private @Nullable BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    private @Nullable SigninManager mSigninManager;
    private @Nullable AccountMenuCoordinator mAccountMenuCoordinator;

    // We observe IdentityManager to receive primary account state change notifications.
    private @Nullable IdentityManager mIdentityManager;

    // ProfileDataCache facilitates retrieving the profile picture.
    private @Nullable ProfileDataCache mProfileDataCache;

    // SyncService is observed to update mIdentityError.
    private @Nullable SyncService mSyncService;

    private @UserActionableError int mIdentityError = UserActionableError.NONE;

    private @Nullable ColorStateList mActivityFocusTint;

    private boolean mHasSpaceToShow = true;

    private boolean mShouldShowOnPage;

    /**
     * @param context The {@link Context} to retrieve resources.
     * @param windowAndroid The {@link WindowAndroid} for the current window.
     * @param model The {@link PropertyModel} for the sign-in button.
     * @param profileSupplier The supplier of the current profile.
     * @param signinAndHistorySyncActivityLauncher The {@link SigninAndHistorySyncActivityLauncher}
     *     to launch sign-in and history sync activity.
     * @param activityResultTracker The {@link ActivityResultTracker} for launching new activities
     *     and watching for their result.
     * @param deviceLockActivityLauncher The launcher for the device lock challenge.
     * @param bottomSheetController The {@link BottomSheetController} to show the sign-in bottom
     *     sheet.
     * @param modalDialogManager The {@link ModalDialogManager} to manage the dialog.
     * @param snackbarManager The {@link SnackbarManager} to show sign-in/sign-out snackbars.
     * @param themeColorProvider The {@link ThemeColorProvider} to get toolbar tint changes.
     * @param onSigninTapped Runnable to be called when the sign-in button is tapped.
     */
    public SigninButtonMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            MonotonicObservableSupplier<Profile> profileSupplier,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            ActivityResultTracker activityResultTracker,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            ThemeColorProvider themeColorProvider,
            Runnable onSigninTapped) {
        mContext = context;
        mModel = model;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileSupplierObserver);
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        mThemeColorProvider = themeColorProvider;
        mOnSigninTapped = onSigninTapped;
        mActivityFocusTint = mThemeColorProvider.getActivityFocusTint();
        mThemeColorProvider.addTintObserver(this);
        mModel.set(
                SigninButtonProperties.IS_ENABLED,
                Objects.equals(mThemeColorProvider.getTint(), mActivityFocusTint));
    }

    /**
     * {@link SyncService.SyncStateChangedListener} implementation which updates the signin button
     * in case a profile badge is needed due to an identity error.
     */
    @Override
    public void syncStateChanged() {
        updateButtonState();
    }

    /**
     * {@link SigninManager.SignInStateObserver} implementation which updates the signin button when
     * signin allowed state changes.
     */
    @Override
    public void onSignInAllowedChanged() {
        updateButtonState();
    }

    /**
     * {@link IdentityManager.Observer} implementation which updates the signin button when primary
     * account changes.
     */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        switch (eventDetails.getEventTypeFor()) {
            case PrimaryAccountChangeEvent.Type.SET:
                updateButtonState();
                break;
            case PrimaryAccountChangeEvent.Type.CLEARED:
                mIdentityError = UserActionableError.NONE;
                updateButtonState();
                break;
            case PrimaryAccountChangeEvent.Type.NONE:
                break;
        }
    }

    /**
     * {@link ProfileDataCache.Observer} implementation which updates the image on the toolbar
     * button once the profile image becomes available.
     */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        String primaryEmail =
                CoreAccountInfo.getEmailFrom(
                        assumeNonNull(mIdentityManager).getPrimaryAccountInfo());
        if (profileData.getAccountEmail().equals(primaryEmail)) {
            updateButtonState();
        }
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {

        // If activityFocusTint is different from tint, the toolbar thinks we are inactive.
        boolean isWindowActive = Objects.equals(tint, activityFocusTint);
        mModel.set(SigninButtonProperties.IS_ENABLED, isWindowActive);

        // The avatar icon tint needs to be manually set.
        mActivityFocusTint = activityFocusTint;
        if (mProfile != null && !mProfile.isOffTheRecord()) {
            updateButtonState();
        }
    }

    boolean hasSpaceToShow() {
        return mHasSpaceToShow;
    }

    void setHasSpaceToShow(boolean hasSpaceToShow) {
        if (mHasSpaceToShow == hasSpaceToShow) return;
        mHasSpaceToShow = hasSpaceToShow;
        updateButtonVisibility(mShouldShowOnPage);
    }

    void updateButtonVisibility(boolean shouldShowOnPage) {
        mShouldShowOnPage = shouldShowOnPage;

        // INFLATE_BUTTON is true as long as the button should exist on the current page.
        mModel.set(SigninButtonProperties.SHOULD_SHOW_ON_PAGE, mShouldShowOnPage);

        // SHOW_BUTTON depends on both page state and available space.
        mModel.set(SigninButtonProperties.IS_VISIBLE, mShouldShowOnPage && mHasSpaceToShow);
    }

    private void updateButtonState() {
        if (mProfile == null || mProfile.isOffTheRecord()) {
            assert !mModel.get(SigninButtonProperties.IS_VISIBLE);
            return;
        }

        mIdentityError =
                mSyncService == null
                        ? UserActionableError.NONE
                        : mSyncService.getUserActionableError();

        @Nullable AccountInfo accountInfo = assumeNonNull(mIdentityManager).getPrimaryAccountInfo();
        if (accountInfo != null) {
            assumeNonNull(mProfileDataCache)
                    .setBadge(
                            accountInfo.getId(),
                            mIdentityError == UserActionableError.NONE
                                    ? null
                                    : BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                                            .withToolbarIdentityDiscConfig()
                                            .build(mContext));
        }

        @Nullable CoreAccountId id = AccountInfo.getIdFrom(accountInfo);
        DisplayableProfileData profileData =
                id == null ? null : assumeNonNull(mProfileDataCache).getById(id);
        setButton(profileData);
    }

    private void setButton(@Nullable DisplayableProfileData profileData) {
        boolean showSigninText =
                profileData == null
                        && assumeNonNull(mSigninManager).isSigninAllowed()
                        && !SHOW_AVATAR_WHEN_SIGNED_OUT;

        if (!showSigninText) {
            Drawable avatar = profileData != null ? profileData.getImage() : getPlaceholderImage();
            if (profileData != null) {
                avatar = maybeApplyErrorBadgeInset(avatar);
            }

            mModel.set(SigninButtonProperties.BUTTON_AVATAR, avatar);
            mModel.set(
                    SigninButtonProperties.AVATAR_TINT,
                    profileData != null ? null : mActivityFocusTint);
            mModel.set(
                    SigninButtonProperties.AVATAR_CONTENT_DESCRIPTION,
                    SigninUtils.getContentDescriptionForIdentityDisc(
                            mContext, profileData, mIdentityError));
        }

        mModel.set(SigninButtonProperties.USE_SIGNIN_TEXT_BUTTON, showSigninText);
        mModel.set(SigninButtonProperties.ON_CLICK, this::onClick);
    }

    /**
     * Triggered by mProfileSupplierObserver when profile is changed in mProfileSupplier.
     * mIdentityManager is updated with the profile, or set to null if profile is off-the-record.
     */
    private void setProfile(@Nullable Profile profile) {
        mProfile = profile;
        resetProfileDataCache();
        if (mAccountMenuCoordinator != null) {
            mAccountMenuCoordinator.destroy();
            mAccountMenuCoordinator = null;
        }
        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
            mIdentityManager = null;
        }
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
            mSigninManager = null;
        }
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
            mSyncService = null;
        }
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }
        if (profile == null || profile.isOffTheRecord()) {
            return;
        }
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(mIdentityManager).addObserver(this);
        mSigninManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(mSigninManager).addSignInStateObserver(this);

        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_AI_SUBSCRIPTION_AVATAR_RING)) {
            SubscriptionEligibilityService subscriptionEligibilityService =
                    SubscriptionEligibilityServiceFactory.getForProfile(profile);

            int aiTierRingThicknessPx =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.ai_tier_ring_thickness_identity_disc);
            int aiTierImageSizePx =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.toolbar_identity_disc_size_with_ring);
            mProfileDataCache =
                    ProfileDataCache.createWithAiTierRing(
                            mContext,
                            mIdentityManager,
                            subscriptionEligibilityService,
                            aiTierImageSizePx,
                            aiTierRingThicknessPx);
        } else {
            mProfileDataCache =
                    ProfileDataCache.createWithoutBadge(
                            mContext, mIdentityManager, R.dimen.toolbar_identity_disc_size);
        }
        mProfileDataCache.addObserver(this);
        mSyncService = SyncServiceFactory.getForProfile(profile);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }
        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            initializeSigninCoordinator();
        }
        updateButtonState();
    }

    private void onClick(View view) {
        if (mProfile == null || mProfile.isOffTheRecord()) {
            return;
        }

        mOnSigninTapped.run();
        recordSigninButtonUsed(mProfile);

        // TODO(crbug.com/551756560): Use a delegate pattern to handle form-factor-based behavior
        // instead of branching here.
        if (DeviceInfo.isDesktop()
                && SigninFeatureMap.isEnabled(SigninFeatures.SIGNIN_BUTTON_PROFILE_MENU)) {
            if (mAccountMenuCoordinator == null) {
                mAccountMenuCoordinator = new AccountMenuCoordinator(mContext);
            }
            mAccountMenuCoordinator.show(view);
            return;
        }

        Profile originalProfile = mProfile.getOriginalProfile();
        if (assumeNonNull(mSigninManager).isSigninAllowed()) {
            AccountPickerBottomSheetStrings bottomSheetStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    mContext.getString(
                                            R.string.signin_account_picker_bottom_sheet_title))
                            .setSubtitleString(
                                    mContext.getString(
                                            R.string
                                                    .signin_account_picker_bottom_sheet_benefits_subtitle))
                            .build();
            BottomSheetSigninAndHistorySyncConfig config =
                    new BottomSheetSigninAndHistorySyncConfig.Builder(
                                    bottomSheetStrings,
                                    NoAccountSigninMode.BOTTOM_SHEET,
                                    WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    HistorySyncConfig.OptInMode.OPTIONAL,
                                    mContext.getString(R.string.history_sync_title),
                                    mContext.getString(R.string.history_sync_subtitle))
                            .signinSurveyType(
                                    SigninSurveyController.SigninSurveyType.NTP_SIGNIN_BUTTON)
                            .build();
            if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
                assumeNonNull(mSigninCoordinator).startSigninFlow(config);
            } else {
                @Nullable Intent intent =
                        mSigninAndHistorySyncActivityLauncher
                                .createBottomSheetSigninIntentOrShowError(
                                        mContext,
                                        originalProfile,
                                        config,
                                        SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                if (intent != null) {
                    mContext.startActivity(intent);
                }
            }
        } else {
            SettingsNavigation settingsNavigation =
                    SettingsNavigationFactory.createSettingsNavigation();
            settingsNavigation.startSettings(mContext);
            SigninSurveyController.registerTrigger(
                    originalProfile,
                    SigninSurveyController.SigninSurveyType.NTP_ACCOUNT_AVATAR_TAP);
        }
    }

    /**
     * Records SigninButton usage with feature engagement tracker. This signal can be used to decide
     * whether to show in-product help. We also record the clicking actions on the profile icon in
     * histograms.
     */
    private void recordSigninButtonUsed(Profile profile) {
        BrowserUiUtils.recordIdentityDiscClicked(true);
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.IDENTITY_DISC_USED);
        RecordUserAction.record("MobileToolbarIdentityDiscTap");
    }

    private void initializeSigninCoordinator() {
        if (mSigninCoordinator == null) {
            OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
            profileSupplier.set(assumeNonNull(mProfile));

            mSigninCoordinator =
                    mSigninAndHistorySyncActivityLauncher
                            .createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                    mWindowAndroid,
                                    assertNonNull(mWindowAndroid.getActivity().get()),
                                    mActivityResultTracker,
                                    this,
                                    mDeviceLockActivityLauncher,
                                    profileSupplier,
                                    () -> mBottomSheetController,
                                    SupplierUtils.of(mModalDialogManager),
                                    SupplierUtils.of(mSnackbarManager),
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
        }
    }

    private void resetProfileDataCache() {
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
    }

    /** Call to tear down dependencies. */
    public void destroy() {
        setProfile(null);
        mProfileSupplier.removeObserver(mProfileSupplierObserver);
        mThemeColorProvider.removeTintObserver(this);
    }

    // TODO(crbug.com/522145863): Remove when AI tier is launched.
    private Drawable maybeApplyErrorBadgeInset(Drawable avatar) {
        if (mIdentityError == UserActionableError.NONE
                || SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_AI_SUBSCRIPTION_AVATAR_RING)) {
            return avatar;
        }

        // The error badge makes the avatar bigger by adding to the bottom and right. Since the
        // button is centered, this pushes it up and left. Adding an inset makes it symmetric again.
        int badgePositionX =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_identity_disc_badge_position_x);
        int badgeSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_identity_disc_badge_size);
        int avatarSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_identity_disc_size);
        int insetPx = badgePositionX + badgeSize - avatarSize;
        if (insetPx <= 0) {
            return avatar;
        }
        return new InsetDrawable(avatar, /* insetLeft= */ insetPx, /* insetTop= */ insetPx, 0, 0);
    }

    private Drawable getPlaceholderImage() {
        return assumeNonNull(mProfileDataCache).getPlaceholderImage();
    }
}
