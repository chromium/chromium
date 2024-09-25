// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.browser_ui.widget.impression.OneShotImpressionListener;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Set;

/**
 * A controller for configuring the sync promo. It sets up the sync promo depending on the context:
 * whether there are any Google accounts on the device which have been previously signed in or not.
 * The controller also takes care of counting impressions, recording signin related user actions and
 * histograms.
 */
public class SyncPromoController {
    public interface Delegate {
        /**
         * Returns the string to apply to the sync promo primary button.
         *
         * @param context the Android context.
         * @param profileData the user's profile data used to create the "continue as..." label.
         */
        String getTextForPrimaryButton(
                Context context, @Nullable DisplayableProfileData profileData);
    }

    /** Specifies the various states of sync promo. */
    // TODO(crbug.com/343908771): Revise SyncPromoState after launching
    //     ReplaceSyncPromosWithSignInPromos.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        SyncPromoState.NO_PROMO,
        SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE,
        SyncPromoState.PROMO_FOR_SIGNED_IN_STATE,
        SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE,
    })
    public @interface SyncPromoState {
        /** Promo is hidden. */
        int NO_PROMO = 0;

        /** Promo is shown when a user is signed out. */
        int PROMO_FOR_SIGNED_OUT_STATE = 1;

        /** Promo is shown when a user is signed in without sync consent. */
        int PROMO_FOR_SIGNED_IN_STATE = 2;

        /** Promo is shown when a user is signed in with sync consent but has turned off sync. */
        int PROMO_FOR_SYNC_TURNED_OFF_STATE = 3;
    }

    /** Receives notifications when user clicks close button in the promo. */
    public interface OnDismissListener {
        /** Action to be performed when the promo is being dismissed. */
        void onDismiss();
    }

    private static final int MAX_TOTAL_PROMO_SHOW_COUNT = 100;
    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private static final int MAX_IMPRESSIONS_SETTINGS = 20;
    private static final int NTP_SYNC_PROMO_INCREASE_SHOW_COUNT_AFTER_MINUTE = 30;

    @VisibleForTesting static final int NTP_SYNC_PROMO_RESET_AFTER_DAYS = 30;

    @VisibleForTesting static final int SYNC_ANDROID_NTP_PROMO_MAX_IMPRESSIONS = 5;

    @VisibleForTesting
    static final int NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS =
            336; // 14 days in hours.

    @VisibleForTesting static final String GMAIL_DOMAIN = "gmail.com";

    /** Strings used for promo shown count histograms. */
    @StringDef({UserAction.CONTINUED, UserAction.DISMISSED, UserAction.SHOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface UserAction {
        String CONTINUED = "Continued";
        String DISMISSED = "Dismissed";
        String SHOWN = "Shown";
    }

    private final Profile mProfile;
    private final AccountPickerBottomSheetStrings mBottomSheetStrings;
    private final @AccessPoint int mAccessPoint;
    private final String mImpressionUserActionName;
    // TODO(b/332704829): Move the declaration of most of these access-point specific fields to the
    // Delegate.
    private final @Nullable String mSyncPromoDismissedPreferenceTracker;
    private final @StringRes int mTitleStringId;
    private final @StringRes int mDescriptionStringId;
    private final boolean mShouldSuppressSecondaryButton;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private final SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private final @SigninAndHistorySyncCoordinator.HistoryOptInMode int mHistoryOptInMode;
    private final Delegate mDelegate;

    private @Nullable DisplayableProfileData mProfileData;
    private @Nullable ImpressionTracker mImpressionTracker;

    private static long getNTPSyncPromoResetAfterMillis() {
        return NTP_SYNC_PROMO_RESET_AFTER_DAYS * DateUtils.DAY_IN_MILLIS;
    }

    /**
     * If the signin promo card has been hidden for longer than the {@link
     * NTP_SYNC_PROMO_RESET_AFTER_DAYS}, resets the impression counts, {@link
     * ChromePreferenceKeys#SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME} and {@link
     * ChromePreferenceKeys#SIGNIN_PROMO_NTP_LAST_SHOWN_TIME} to allow the promo card to show again.
     */
    public static void resetNtpSyncPromoLimitsIfHiddenForTooLong() {
        final long currentTime = System.currentTimeMillis();
        final long resetAfterMs = getNTPSyncPromoResetAfterMillis();
        final long lastShownTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
        if (resetAfterMs <= 0 || lastShownTime <= 0) return;

        if (currentTime - lastShownTime >= resetAfterMs) {
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            getPromoShowCountPreferenceName(SigninAccessPoint.NTP_FEED_TOP_PROMO),
                            0);
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME);
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME);
        }
    }

    private static boolean timeElapsedSinceFirstShownExceedsLimit() {
        final long timeSinceFirstShownLimitMs =
                NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS * DateUtils.HOUR_IN_MILLIS;
        if (timeSinceFirstShownLimitMs <= 0) return false;

        final long currentTime = System.currentTimeMillis();
        final long firstShownTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, 0L);
        return firstShownTime > 0 && currentTime - firstShownTime >= timeSinceFirstShownLimitMs;
    }

    private static boolean canShowSettingsPromo() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return false;
        }
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        boolean isPromoDismissed =
                preferencesManager.readBoolean(
                        ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, false);
        return preferencesManager.readInt(
                                getPromoShowCountPreferenceName(SigninAccessPoint.SETTINGS))
                        < MAX_IMPRESSIONS_SETTINGS
                && !isPromoDismissed;
    }

    @VisibleForTesting
    public static String getPromoShowCountPreferenceName(@AccessPoint int accessPoint) {
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SyncPromoAccessPointId.BOOKMARKS);
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                // This preference may get reset while the other ones are never reset unless device
                // data is wiped.
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SyncPromoAccessPointId.NTP);
            case SigninAccessPoint.SETTINGS:
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SyncPromoAccessPointId.SETTINGS);
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + accessPoint);
        }
    }

    /**
     * Creates a new SyncPromoController.
     *
     * @param profile The Profile associated with the sync promo.
     * @param bottomSheetStrings Contains the string resource IDs for the sign-in bottom sheet.
     * @param accessPoint Specifies the AccessPoint from which the promo is to be shown.
     * @param syncConsentActivityLauncher Launcher of {@link SyncConsentActivity}.
     * @param signinAndHistorySyncActivityLauncher Launcher of {@link SigninAndHistorySyncActivity}.
     */
    public SyncPromoController(
            Profile profile,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @AccessPoint int accessPoint,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher) {
        mProfile = profile;
        mBottomSheetStrings = bottomSheetStrings;
        mAccessPoint = accessPoint;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED;
                if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                    mTitleStringId = R.string.signin_promo_title_bookmarks;
                    mDescriptionStringId = R.string.signin_promo_description_bookmarks;
                } else {
                    mTitleStringId = R.string.sync_promo_title_bookmarks;
                    mDescriptionStringId = R.string.sync_promo_description_bookmarks;
                }
                mShouldSuppressSecondaryButton = false;
                mHistoryOptInMode = SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate = this::getPromoPrimaryButtonText;
                break;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                mImpressionUserActionName = "Signin_Impression_FromNTPFeedTopPromo";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED;
                if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                    mTitleStringId = R.string.signin_promo_title_ntp_feed_top_promo;
                    mDescriptionStringId = R.string.signin_promo_description_ntp_feed_top_promo;
                } else {
                    mTitleStringId = R.string.sync_promo_title_ntp_content_suggestions;
                    mDescriptionStringId = R.string.sync_promo_description_ntp_content_suggestions;
                }
                mShouldSuppressSecondaryButton = false;
                mHistoryOptInMode = SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate = this::getPromoPrimaryButtonText;
                break;
            case SigninAccessPoint.RECENT_TABS:
                mImpressionUserActionName = "Signin_Impression_FromRecentTabs";
                mSyncPromoDismissedPreferenceTracker = null;
                if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                    mTitleStringId = R.string.signin_promo_title_recent_tabs;
                    mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                    mShouldSuppressSecondaryButton = true;
                } else {
                    mTitleStringId = R.string.sync_promo_title_recent_tabs;
                    mDescriptionStringId = R.string.sync_promo_description_recent_tabs;
                    mShouldSuppressSecondaryButton = false;
                }
                mHistoryOptInMode = SigninAndHistorySyncCoordinator.HistoryOptInMode.REQUIRED;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate =
                        (context, profileData) -> {
                            if (ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                                return context.getResources()
                                        .getString(R.string.signin_promo_turn_on);
                            }
                            return context.getResources()
                                    .getString(R.string.sync_promo_turn_on_sync);
                        };
                break;
            case SigninAccessPoint.SETTINGS:
                mImpressionUserActionName = "Signin_Impression_FromSettings";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED;
                mTitleStringId = R.string.sync_promo_title_settings;
                boolean isAccountStorageEnabled =
                        ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS);
                mDescriptionStringId =
                        isAccountStorageEnabled
                                ? R.string.sync_promo_description_settings_without_passwords
                                : R.string.sync_promo_description_settings;
                mShouldSuppressSecondaryButton = false;
                mHistoryOptInMode = SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate =
                        (context, profileData) -> {
                            assert !ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);

                            IdentityManager identityManager =
                                    IdentityServicesProvider.get().getIdentityManager(mProfile);
                            if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                                    || profileData == null) {
                                return context.getResources()
                                        .getString(R.string.sync_promo_turn_on_sync);
                            }
                            return SigninUtils.getContinueAsButtonText(context, profileData);
                        };
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + mAccessPoint);
        }
    }

    /** Determines whether the Sync promo can be shown. */
    public boolean canShowSyncPromo() {
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return canShowBookmarkPromo();
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                return canShowNTPPromo();
            case SigninAccessPoint.RECENT_TABS:
                return canShowRecentTabsPromo();
            case SigninAccessPoint.SETTINGS:
                return canShowSettingsPromo();
            default:
                assert false : "Unexpected value for access point: " + mAccessPoint;
                return false;
        }
    }

    private boolean canShowNTPPromo() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            if (IdentityServicesProvider.get()
                    .getIdentityManager(mProfile)
                    .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                return false;
            }
        }

        int promoShowCount =
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                getPromoShowCountPreferenceName(
                                        SigninAccessPoint.NTP_FEED_TOP_PROMO));
        if (promoShowCount >= SYNC_ANDROID_NTP_PROMO_MAX_IMPRESSIONS) {
            return false;
        }

        if (timeElapsedSinceFirstShownExceedsLimit()) {
            return false;
        }

        if (ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false)) {
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS)) {
            return false;
        }
        final @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        if (visibleAccount == null) {
            return true;
        }
        final Promise<AccountInfo> visibleAccountPromise =
                AccountInfoServiceProvider.get().getAccountInfoByEmail(visibleAccount.getEmail());

        AccountInfo accountInfo =
                visibleAccountPromise.isFulfilled() ? visibleAccountPromise.getResult() : null;
        if (accountInfo == null) return false;

        return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                || accountInfo
                                .getAccountCapabilities()
                                .canShowHistorySyncOptInsWithoutMinorModeRestrictions()
                        == Tribool.TRUE;
    }

    private boolean canShowBookmarkPromo() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            assert SyncFeatureMap.isEnabled(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE);
            if (IdentityServicesProvider.get()
                    .getIdentityManager(mProfile)
                    .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                return false;
            }
        }

        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (SyncFeatureMap.isEnabled(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
                && syncService
                        .getSelectedTypes()
                        .containsAll(
                                Set.of(
                                        UserSelectableType.BOOKMARKS,
                                        UserSelectableType.READING_LIST))) {
            return false;
        }

        boolean isTypeManagedByPolicy =
                syncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        boolean isMaxImpressionCountReached =
                ChromeSharedPreferences.getInstance()
                                .readInt(
                                        getPromoShowCountPreferenceName(
                                                SigninAccessPoint.BOOKMARK_MANAGER))
                        >= MAX_IMPRESSIONS_BOOKMARKS;
        boolean isPromoDismissed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);

        return !isTypeManagedByPolicy && !isMaxImpressionCountReached && !isPromoDismissed;
    }

    private boolean canShowRecentTabsPromo() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            final HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(mProfile);
            final SigninManager signinManager =
                    IdentityServicesProvider.get().getSigninManager(mProfile);
            final IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(mProfile);
            if (!signinManager.isSigninAllowed()
                    && !identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                // If sign-in is not possible, then history sync isn't possible either.
                return false;
            }
            return !historySyncHelper.shouldSuppressHistorySync();
        }
        return !SyncServiceFactory.getForProfile(mProfile)
                .isTypeManagedByPolicy(UserSelectableType.TABS);
    }

    // Find the visible account for sync promos
    private @Nullable CoreAccountInfo getVisibleAccount() {
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        @Nullable
        CoreAccountInfo visibleAccount = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultCoreAccountInfoIfFulfilled(
                            accountManagerFacade.getCoreAccountInfos());
        }
        return visibleAccount;
    }

    /**
     * Sets up the sync promo view.
     * @param profileDataCache The {@link ProfileDataCache} that stores profile data.
     * @param view The {@link PersonalizedSigninPromoView} that should be set up.
     * @param listener The {@link SyncPromoController.OnDismissListener} to be set to the view.
     */
    public void setUpSyncPromoView(
            ProfileDataCache profileDataCache,
            PersonalizedSigninPromoView view,
            SyncPromoController.OnDismissListener listener) {
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assert !identityManager.hasPrimaryAccount(ConsentLevel.SYNC) : "Sync is already enabled!";

        final @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        // Set up the sync promo
        if (visibleAccount == null) {
            setupPromoView(view, /* profileData= */ null, listener);
            return;
        }
        setupPromoView(
                view,
                profileDataCache.getProfileDataOrDefault(visibleAccount.getEmail()),
                listener);
    }

    /**
     * Configures the signin promo view and resets the impression tracker. If this controller has
     * been previously set up.
     * @param view The view in which the promo will be added.
     * @param profileData If not null, the promo will be configured to be in the hot state, using
     *         the account image, email and full name of the user to set the picture and the text of
     *         the promo appropriately. Otherwise, the promo will be in the cold state.
     * @param onDismissListener Listener which handles the action of dismissing the promo. A null
     *         onDismissListener marks that the promo is not dismissible and as a result the close
     *         button is hidden.
     */
    private void setupPromoView(
            PersonalizedSigninPromoView view,
            final @Nullable DisplayableProfileData profileData,
            final @Nullable OnDismissListener onDismissListener) {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
        mImpressionTracker = new ImpressionTracker(view);
        mImpressionTracker.setListener(
                new OneShotImpressionListener(this::recordSigninPromoImpression));

        mProfileData = profileData;
        if (mProfileData == null) {
            setupColdState(view);
        } else {
            setupHotState(view);
        }

        if (onDismissListener != null) {
            // Recent Tabs promos can't be dismissed.
            assert mAccessPoint != SigninAccessPoint.RECENT_TABS;
            view.getDismissButton().setVisibility(View.VISIBLE);
            view.getDismissButton()
                    .setOnClickListener(
                            promoView -> {
                                assert mSyncPromoDismissedPreferenceTracker != null;
                                ChromeSharedPreferences.getInstance()
                                        .writeBoolean(mSyncPromoDismissedPreferenceTracker, true);
                                recordShowCountHistogram(UserAction.DISMISSED);
                                onDismissListener.onDismiss();
                            });
        } else {
            view.getDismissButton().setVisibility(View.GONE);
        }
    }

    /** Should be called when the view is not in use anymore (e.g. it's being recycled). */
    public void detach() {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    /** Increases promo show count by one. */
    public void increasePromoShowCount() {
        if (mAccessPoint == SigninAccessPoint.NTP_FEED_TOP_PROMO) {
            final long currentTime = System.currentTimeMillis();
            final long lastShownTime =
                    ChromeSharedPreferences.getInstance()
                            .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
            if (currentTime - lastShownTime
                    < NTP_SYNC_PROMO_INCREASE_SHOW_COUNT_AFTER_MINUTE
                            * DateUtils.MINUTE_IN_MILLIS) {
                return;
            }
            if (ChromeSharedPreferences.getInstance()
                            .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME)
                    == 0) {
                ChromeSharedPreferences.getInstance()
                        .writeLong(
                                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME,
                                currentTime);
            }
            ChromeSharedPreferences.getInstance()
                    .writeLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, currentTime);
        }
        if (mAccessPoint != SigninAccessPoint.RECENT_TABS) {
            ChromeSharedPreferences.getInstance()
                    .incrementInt(getPromoShowCountPreferenceName(mAccessPoint));
        }
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        recordShowCountHistogram(UserAction.SHOWN);
    }

    // TODO(crbug.com/40838474): we can share more code between setupColdState() and
    // setupHotState().
    // The difference between the 2 will just be the avatar and the behavior of the primary button.
    private void setupColdState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        view.getImage().setImageResource(R.drawable.chrome_sync_logo);
        setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);

        view.getTitle().setText(mTitleStringId);
        view.getDescription().setText(mDescriptionStringId);

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);
        boolean shouldLaunchSigninFlow =
                shouldLaunchSigninFlow(
                        mAccessPoint, identityManager, signinManager, null, prefService);
        view.getPrimaryButton().setText(mDelegate.getTextForPrimaryButton(context, null));
        view.getPrimaryButton()
                .setOnClickListener(v -> signinWithNewAccount(context, shouldLaunchSigninFlow));

        view.getSecondaryButton().setVisibility(View.GONE);
    }

    private void setupHotState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        Drawable accountImage = mProfileData.getImage();
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.sync_promo_account_image_size);

        view.getTitle().setText(mTitleStringId);
        view.getDescription().setText(mDescriptionStringId);

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        List<CoreAccountInfo> accounts =
                AccountManagerFacadeProvider.getInstance().getCoreAccountInfos().getResult();
        PrefService prefService = UserPrefs.get(mProfile);
        boolean shouldLaunchSigninFlow =
                shouldLaunchSigninFlow(
                        mAccessPoint, identityManager, signinManager, accounts, prefService);
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                && shouldLaunchSigninFlow
                && mAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            // The bookmarks manager has different conditions for displaying the new flow.
            view.getDescription().setText(R.string.signin_promo_description_bookmarks);
        }
        view.getPrimaryButton()
                .setOnClickListener(v -> signinWithDefaultAccount(context, shouldLaunchSigninFlow));
        view.getPrimaryButton().setText(mDelegate.getTextForPrimaryButton(context, mProfileData));
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                || mShouldSuppressSecondaryButton) {
            view.getSecondaryButton().setVisibility(View.GONE);
            return;
        }

        // Hide secondary button on automotive devices, as they only support one account per device
        if (BuildInfo.getInstance().isAutomotive) {
            view.getSecondaryButton().setVisibility(View.GONE);
        } else {
            view.getSecondaryButton().setText(R.string.signin_promo_choose_another_account);
            view.getSecondaryButton()
                    .setOnClickListener(
                            v -> signinWithNotDefaultAccount(context, shouldLaunchSigninFlow));
            view.getSecondaryButton().setVisibility(View.VISIBLE);
        }
    }

    private void signinWithNewAccount(Context context, boolean launchSigninFlow) {
        recordShowCountHistogram(UserAction.CONTINUED);
        if (launchSigninFlow) {
            if (mAccessPoint == SigninAccessPoint.RECENT_TABS) {
                mSigninAndHistorySyncActivityLauncher.launchActivityForHistorySyncDedicatedFlow(
                        context,
                        mProfile,
                        mBottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        mAccessPoint);
            } else {
                mSigninAndHistorySyncActivityLauncher.launchActivityIfAllowed(
                        context,
                        mProfile,
                        mBottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        mHistoryOptInMode,
                        mAccessPoint);
            }
        } else {
            mSyncConsentActivityLauncher.launchActivityForPromoAddAccountFlow(
                    context, mAccessPoint);
        }
    }

    private void signinWithDefaultAccount(Context context, boolean launchSigninFlow) {
        recordShowCountHistogram(UserAction.CONTINUED);
        if (launchSigninFlow) {
            if (mAccessPoint == SigninAccessPoint.RECENT_TABS) {
                mSigninAndHistorySyncActivityLauncher.launchActivityForHistorySyncDedicatedFlow(
                        context,
                        mProfile,
                        mBottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        mAccessPoint);
            } else {
                mSigninAndHistorySyncActivityLauncher.launchActivityIfAllowed(
                        context,
                        mProfile,
                        mBottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        mHistoryOptInMode,
                        mAccessPoint);
            }
        } else {
            mSyncConsentActivityLauncher.launchActivityForPromoDefaultFlow(
                    context, mAccessPoint, mProfileData.getAccountEmail());
        }
    }

    private void signinWithNotDefaultAccount(Context context, boolean launchSigninFlow) {
        recordShowCountHistogram(UserAction.CONTINUED);
        if (launchSigninFlow) {
            if (mAccessPoint == SigninAccessPoint.RECENT_TABS) {
                mSigninAndHistorySyncActivityLauncher.launchActivityForHistorySyncDedicatedFlow(
                        context,
                        mProfile,
                        mBottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .CHOOSE_ACCOUNT_BOTTOM_SHEET,
                        mAccessPoint);
            } else {
                mSigninAndHistorySyncActivityLauncher.launchActivityIfAllowed(
                        context,
                        mProfile,
                        mBottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .CHOOSE_ACCOUNT_BOTTOM_SHEET,
                        mHistoryOptInMode,
                        mAccessPoint);
            }
        } else {
            mSyncConsentActivityLauncher.launchActivityForPromoChooseAccountFlow(
                    context, mAccessPoint, mProfileData.getAccountEmail());
        }
    }

    private void recordShowCountHistogram(@UserAction String actionType) {
        final String accessPoint;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                accessPoint = SigninPreferencesManager.SyncPromoAccessPointId.BOOKMARKS;
                break;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                accessPoint = SigninPreferencesManager.SyncPromoAccessPointId.NTP;
                break;
            case SigninAccessPoint.RECENT_TABS:
                accessPoint = SigninPreferencesManager.SyncPromoAccessPointId.RECENT_TABS;
                break;
            case SigninAccessPoint.SETTINGS:
                accessPoint = SigninPreferencesManager.SyncPromoAccessPointId.SETTINGS;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point" + mAccessPoint);
        }
        RecordHistogram.recordExactLinearHistogram(
                "Signin.SyncPromo." + actionType + ".Count." + accessPoint,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT),
                MAX_TOTAL_PROMO_SHOW_COUNT);
    }

    private void setImageSize(
            Context context, PersonalizedSigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }

    private void recordSigninPromoImpression() {
        RecordUserAction.record(mImpressionUserActionName);
    }

    private String getPromoPrimaryButtonText(Context context, DisplayableProfileData profileData) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return profileData == null
                    ? context.getResources().getString(R.string.signin_promo_signin)
                    : SigninUtils.getContinueAsButtonText(context, profileData);
        }

        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN) || profileData == null) {
            return context.getResources().getString(R.string.sync_promo_turn_on_sync);
        }

        return profileData == null
                ? context.getResources().getString(R.string.signin_promo_turn_on)
                : SigninUtils.getContinueAsButtonText(context, profileData);
    }

    public static void setPrefSigninPromoDeclinedBookmarksForTests(boolean isDeclined) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, isDeclined);
    }

    public static int getMaxImpressionsBookmarksForTests() {
        return MAX_IMPRESSIONS_BOOKMARKS;
    }

    @VisibleForTesting
    static boolean shouldLaunchSigninFlow(
            @SigninAccessPoint int accessPoint,
            IdentityManager identityManager,
            SigninManager signinManager,
            @Nullable List<CoreAccountInfo> accounts,
            PrefService prefService) {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            assert SyncFeatureMap.isEnabled(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE);
            return true;
        }

        if (!SyncFeatureMap.isEnabled(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)) {
            return false;
        }
        if (accessPoint != SigninAccessPoint.BOOKMARK_MANAGER) {
            return false;
        }
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
        }

        // If the last syncing user did not remove data during sign-out, show the sync promo
        // instead.
        if (!prefService.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_GAIA_ID).isEmpty()) {
            return false;
        }

        return accounts != null
                && !accounts.isEmpty()
                && !existsNonGmailAccount(signinManager, accounts);
    }

    // Returns whether at least one non-gmail account exist in `accounts`.
    @VisibleForTesting
    static boolean existsNonGmailAccount(
            SigninManager signinManager, List<CoreAccountInfo> accounts) {
        assert accounts != null && !accounts.isEmpty();
        return !accounts.stream()
                .allMatch(
                        coreAccountInfo ->
                                signinManager
                                        .extractDomainName(coreAccountInfo.getEmail())
                                        .equals(GMAIL_DOMAIN));
    }
}
