// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;
import androidx.annotation.StringDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.browser_ui.widget.impression.OneShotImpressionListener;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * A controller for configuring the sync promo. It sets up the sync promo depending on the context:
 * whether there are any Google accounts on the device which have been previously signed in or not.
 * The controller also takes care of counting impressions, recording signin related user actions and
 * histograms.
 */
@NullMarked
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

    /** Receives notifications when user clicks close button in the promo. */
    public interface OnDismissListener {
        /** Action to be performed when the promo is being dismissed. */
        void onDismiss();
    }

    private static final int MAX_TOTAL_PROMO_SHOW_COUNT = 100;
    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private static final int NTP_SYNC_PROMO_INCREASE_SHOW_COUNT_AFTER_MINUTE = 30;

    @VisibleForTesting static final int NTP_SYNC_PROMO_RESET_AFTER_DAYS = 30;

    @VisibleForTesting static final int SYNC_ANDROID_NTP_PROMO_MAX_IMPRESSIONS = 5;

    @VisibleForTesting
    static final int NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS =
            336; // 14 days in hours.

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
    private final @SigninAndHistorySyncActivityLauncher.AccessPoint int mAccessPoint;
    private final String mImpressionUserActionName;
    // TODO(b/332704829): Move the declaration of most of these access-point specific fields to the
    // Delegate.
    private final @Nullable String mSyncPromoDismissedPreferenceTracker;
    private final @StringRes int mTitleStringId;
    private final @StringRes int mDescriptionStringId;
    private final boolean mShouldSuppressSecondaryButton;
    private final SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private final @HistorySyncConfig.OptInMode int mHistoryOptInMode;
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

    @VisibleForTesting
    public static String getPromoShowCountPreferenceName(@SigninAccessPoint int accessPoint) {
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS);
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                // This preference may get reset while the other ones are never reset unless device
                // data is wiped.
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.NTP);
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
     * @param signinAndHistorySyncActivityLauncher Launcher of {@link SigninAndHistorySyncActivity}.
     */
    public SyncPromoController(
            Profile profile,
            AccountPickerBottomSheetStrings bottomSheetStrings,
            @SigninAndHistorySyncActivityLauncher.AccessPoint int accessPoint,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher) {
        mProfile = profile;
        mBottomSheetStrings = bottomSheetStrings;
        mAccessPoint = accessPoint;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED;
                mTitleStringId = R.string.signin_promo_title_bookmarks;
                mDescriptionStringId = R.string.signin_promo_description_bookmarks;
                mShouldSuppressSecondaryButton = false;
                mHistoryOptInMode = HistorySyncConfig.OptInMode.NONE;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate = this::getPromoPrimaryButtonText;
                break;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                mImpressionUserActionName = "Signin_Impression_FromNTPFeedTopPromo";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED;
                mTitleStringId = R.string.signin_promo_title_ntp_feed_top_promo;
                mDescriptionStringId = R.string.signin_promo_description_ntp_feed_top_promo;
                mShouldSuppressSecondaryButton = false;
                mHistoryOptInMode = HistorySyncConfig.OptInMode.NONE;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate = this::getPromoPrimaryButtonText;
                break;
            case SigninAccessPoint.RECENT_TABS:
                mImpressionUserActionName = "Signin_Impression_FromRecentTabs";
                mSyncPromoDismissedPreferenceTracker = null;
                mTitleStringId = R.string.signin_promo_title_recent_tabs;
                mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                mShouldSuppressSecondaryButton = true;
                mHistoryOptInMode = HistorySyncConfig.OptInMode.REQUIRED;
                // TODO(b/332704829): Move delegate creation outside of this constructor.
                mDelegate =
                        (context, profileData) -> {
                            return context.getString(R.string.signin_promo_turn_on);
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
            default:
                assert false : "Unexpected value for access point: " + mAccessPoint;
                return false;
        }
    }

    private boolean canShowNTPPromo() {
        if (assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile))
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
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

        final @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        if (visibleAccount == null) {
            return true;
        }

        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        final @Nullable AccountInfo accountInfo =
                identityManager != null
                        ? identityManager.findExtendedAccountInfoByEmailAddress(
                                visibleAccount.getEmail())
                        : null;
        return accountInfo != null;
    }

    private boolean canShowBookmarkPromo() {
        if (assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile))
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
        }

        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        assumeNonNull(syncService);
        if (syncService
                .getSelectedTypes()
                .containsAll(
                        Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST))) {
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
        final HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(mProfile);
        final SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(mProfile);
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(signinManager);
        assumeNonNull(identityManager);
        if (!signinManager.isSigninAllowed()
                && !identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // If sign-in is not possible, then history sync isn't possible either.
            return false;
        }
        return historySyncHelper.shouldDisplayHistorySync();
    }

    // Find the visible account for sync promos
    private @Nullable CoreAccountInfo getVisibleAccount() {
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        CoreAccountInfo visibleAccount = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultAccountIfFulfilled(accountManagerFacade.getAccounts());
        }
        return visibleAccount;
    }

    /**
     * Sets up the sync promo view.
     *
     * @param profileDataCache The {@link ProfileDataCache} that stores profile data.
     * @param view The {@link PersonalizedSigninPromoView} that should be set up.
     * @param listener The {@link SyncPromoController.OnDismissListener} to be set to the view.
     */
    public void setUpSyncPromoView(
            ProfileDataCache profileDataCache,
            PersonalizedSigninPromoView view,
            SyncPromoController.@Nullable OnDismissListener listener) {
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
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
        view.getPrimaryButton().setText(mDelegate.getTextForPrimaryButton(context, null));
        view.getPrimaryButton().setOnClickListener(v -> signinWithNewAccount(context));

        view.getSecondaryButton().setVisibility(View.GONE);
    }

    private void setupHotState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        assumeNonNull(mProfileData);
        Drawable accountImage = mProfileData.getImage();
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.sync_promo_account_image_size);

        view.getTitle().setText(mTitleStringId);
        view.getDescription().setText(mDescriptionStringId);
        view.getPrimaryButton().setOnClickListener(v -> signinWithDefaultAccount(context));
        view.getPrimaryButton().setText(mDelegate.getTextForPrimaryButton(context, mProfileData));
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                || mShouldSuppressSecondaryButton) {
            view.getSecondaryButton().setVisibility(View.GONE);
            return;
        }

        // Hide secondary button on automotive devices, as they only support one account per device
        if (DeviceInfo.isAutomotive()) {
            view.getSecondaryButton().setVisibility(View.GONE);
        } else {
            view.getSecondaryButton().setText(R.string.signin_promo_choose_another_account);
            view.getSecondaryButton().setOnClickListener(v -> signinWithNotDefaultAccount(context));
            view.getSecondaryButton().setVisibility(View.VISIBLE);
        }
    }

    private void signinWithNewAccount(Context context) {
        recordShowCountHistogram(UserAction.CONTINUED);
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                mBottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                mHistoryOptInMode,
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .build();
        @Nullable Intent intent =
                mSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        context, mProfile, config, mAccessPoint);
        if (intent != null) {
            context.startActivity(intent);
        }
    }

    private void signinWithDefaultAccount(Context context) {
        recordShowCountHistogram(UserAction.CONTINUED);
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                mBottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                mHistoryOptInMode,
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .build();
        @Nullable Intent intent =
                mSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        context, mProfile, config, mAccessPoint);
        if (intent != null) {
            context.startActivity(intent);
        }
    }

    private void signinWithNotDefaultAccount(Context context) {
        recordShowCountHistogram(UserAction.CONTINUED);
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                mBottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                                mHistoryOptInMode,
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .build();
        @Nullable Intent intent =
                mSigninAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        context, mProfile, config, mAccessPoint);
        if (intent != null) {
            context.startActivity(intent);
        }
    }

    private void recordShowCountHistogram(@UserAction String actionType) {
        final String accessPoint;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                accessPoint = SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS;
                break;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO:
                accessPoint = SigninPreferencesManager.SigninPromoAccessPointId.NTP;
                break;
            case SigninAccessPoint.RECENT_TABS:
                accessPoint = SigninPreferencesManager.SigninPromoAccessPointId.RECENT_TABS;
                break;
            case SigninAccessPoint.SETTINGS:
                accessPoint = SigninPreferencesManager.SigninPromoAccessPointId.SETTINGS;
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

    private String getPromoPrimaryButtonText(
            Context context, @Nullable DisplayableProfileData profileData) {
        return profileData == null
                ? context.getString(R.string.signin_promo_signin)
                : SigninUtils.getContinueAsButtonText(context, profileData);
    }

    public static void setPrefSigninPromoDeclinedBookmarksForTests(boolean isDeclined) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, isDeclined);
    }

    public static int getMaxImpressionsBookmarksForTests() {
        return MAX_IMPRESSIONS_BOOKMARKS;
    }
}
