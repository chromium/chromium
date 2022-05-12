// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.Account;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher.AccessPoint;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.browser_ui.widget.impression.OneShotImpressionListener;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A controller for configuring the sign in promo. It sets up the sign in promo depending on the
 * context: whether there are any Google accounts on the device which have been previously signed in
 * or not. The controller also takes care of counting impressions, recording signin related user
 * actions and histograms.
 */
public class SigninPromoController {
    /** Specifies the various states of sync promo. */
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
    /**
     * Receives notifications when user clicks close button in the promo.
     */
    public interface OnDismissListener {
        /**
         * Action to be performed when the promo is being dismissed.
         */
        void onDismiss();
    }

    private static final int MAX_TOTAL_PROMO_SHOW_COUNT = 100;
    private static final int MAX_IMPRESSIONS_BOOKMARKS = 20;
    private static final int MAX_IMPRESSIONS_SETTINGS = 20;

    /** Suffix strings for promo shown count preference and histograms. */
    @StringDef({AccessPointId.BOOKMARKS, AccessPointId.NTP, AccessPointId.RECENT_TABS,
            AccessPointId.SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPointId {
        String BOOKMARKS = "Bookmarks";
        String NTP = "Ntp";
        String RECENT_TABS = "RecentTabs"; // Only used for histograms
        String SETTINGS = "Settings";
    }

    /** Strings used for promo shown count histograms. */
    @StringDef({UserAction.CONTINUED, UserAction.DISMISSED, UserAction.SHOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface UserAction {
        String CONTINUED = "Continued";
        String DISMISSED = "Dismissed";
        String SHOWN = "Shown";
    }

    private @Nullable DisplayableProfileData mProfileData;
    private @Nullable ImpressionTracker mImpressionTracker;
    private final @AccessPoint int mAccessPoint;
    // TODO(https://crbug.com/1254399): Remove this field. This is over counted.
    private final @Nullable String mImpressionCountName;
    private final String mImpressionUserActionName;
    private final String mImpressionWithAccountUserActionName;
    private final String mImpressionWithNoAccountUserActionName;
    private final String mSigninWithDefaultUserActionName;
    private final String mSigninNotDefaultUserActionName;
    private final String mSigninNewAccountUserActionName;
    private final @Nullable String mSyncPromoDismissedPreferenceTracker;
    // TODO(https://crbug.com/1254399): Remove these fields related to impressions.
    private final @Nullable String mImpressionsTilDismissHistogramName;
    private final @StringRes int mTitleStringId;
    private final @StringRes int mDescriptionStringId;
    private final @StringRes int mDescriptionStringIdNoAccount;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private boolean mWasDisplayed;
    private boolean mWasUsed;

    /**
     * Determines whether the Sync promo can be shown.
     * @param accessPoint The access point for which the impression limit is being checked.
     */
    public static boolean canShowSyncPromo(@AccessPoint int accessPoint) {
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return canShowBookmarkPromo();
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                return canShowNTPPromo();
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit or dismiss button in Recent Tabs promo.
                return true;
            case SigninAccessPoint.SETTINGS:
                return canShowSettingsPromo();
            default:
                assert false : "Unexpected value for access point: " + accessPoint;
                return false;
        }
    }

    /**
     * If the signin promo card has been hidden for longer than the {@link
     * StartSurfaceConfiguration#SIGNIN_PROMO_NTP_RESET_AFTER_HOURS}, resets the impression counts,
     * {@link ChromePreferenceKeys#SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME} and {@link
     * ChromePreferenceKeys#SIGNIN_PROMO_NTP_LAST_SHOWN_TIME} to allow the promo card to show again.
     */
    public static void resetNTPSyncPromoLimitsIfHiddenForTooLong() {
        final long currentTime = System.currentTimeMillis();
        final long resetAfterMs =
                StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS.getValue()
                * DateUtils.HOUR_IN_MILLIS;
        final long lastShownTime = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
        if (resetAfterMs <= 0 || lastShownTime <= 0) return;

        if (currentTime - lastShownTime >= resetAfterMs) {
            SharedPreferencesManager.getInstance().writeInt(
                    getPromoShowCountPreferenceName(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS), 0);
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME);
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME);
        }
    }

    private static boolean canShowBookmarkPromo() {
        boolean isPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);
        return SharedPreferencesManager.getInstance().readInt(
                       getPromoShowCountPreferenceName(SigninAccessPoint.BOOKMARK_MANAGER))
                < MAX_IMPRESSIONS_BOOKMARKS
                && !isPromoDismissed;
    }

    private static boolean timeElapsedSinceFirstShownExceedsLimit() {
        final long timeSinceFirstShownLimitMs =
                StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS
                        .getValue()
                * DateUtils.HOUR_IN_MILLIS;
        if (timeSinceFirstShownLimitMs <= 0) return false;

        final long currentTime = System.currentTimeMillis();
        final long firstShownTime = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, 0L);
        return firstShownTime > 0 && currentTime - firstShownTime >= timeSinceFirstShownLimitMs;
    }

    private static boolean canShowNTPPromo() {
        final int maxImpressions =
                StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT.getValue();
        if (timeElapsedSinceFirstShownExceedsLimit()
                || SharedPreferencesManager.getInstance().readInt(getPromoShowCountPreferenceName(
                           SigninAccessPoint.NTP_CONTENT_SUGGESTIONS))
                        >= maxImpressions) {
            return false;
        }

        if (SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false)) {
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS)) {
            return false;
        }
        final @Nullable Account visibleAccount = getVisibleAccount();
        if (visibleAccount == null) {
            return true;
        }
        final Promise<AccountInfo> visibleAccountPromise =
                AccountInfoServiceProvider.get().getAccountInfoByEmail(visibleAccount.name);
        return visibleAccountPromise.isFulfilled()
                && visibleAccountPromise.getResult()
                           .getAccountCapabilities()
                           .canOfferExtendedSyncPromos()
                == Tribool.TRUE;
    }

    private static boolean canShowSettingsPromo() {
        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        boolean isPromoDismissed = preferencesManager.readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, false);
        return preferencesManager.readInt(
                       getPromoShowCountPreferenceName(SigninAccessPoint.SETTINGS))
                < MAX_IMPRESSIONS_SETTINGS
                && !isPromoDismissed;
    }

    // Find the visible account for sync promos
    private static @Nullable Account getVisibleAccount() {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        @Nullable
        Account visibleAccount = CoreAccountInfo.getAndroidAccountFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultAccountIfFulfilled(accountManagerFacade.getAccounts());
        }
        return visibleAccount;
    }

    @VisibleForTesting
    public static String getPromoShowCountPreferenceName(@AccessPoint int accessPoint) {
        switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        AccessPointId.BOOKMARKS);
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                // This preference may get reset while the other ones are never reset unless device
                // data is wiped.
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(AccessPointId.NTP);
            case SigninAccessPoint.SETTINGS:
                return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(AccessPointId.SETTINGS);
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + accessPoint);
        }
    }

    /**
     * Creates a new SigninPromoController.
     * @param accessPoint Specifies the AccessPoint from which the promo is to be shown.
     * @param syncConsentActivityLauncher Launcher of {@link SyncConsentActivity}.
     */
    public SigninPromoController(
            @AccessPoint int accessPoint, SyncConsentActivityLauncher syncConsentActivityLauncher) {
        mAccessPoint = accessPoint;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                mImpressionCountName =
                        ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_BOOKMARKS;
                mImpressionUserActionName = "Signin_Impression_FromBookmarkManager";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromBookmarkManager";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromBookmarkManager";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromBookmarkManager";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromBookmarkManager";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromBookmarkManager";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED;
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.BookmarkManager.ImpressionsTilDismiss";
                mTitleStringId = R.string.sync_promo_title_bookmarks;
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
                    // TODO(crbug.com/1323197): mDescriptionStringIdNoAccount should be deleted if
                    // the SYNC_ANDROID_PROMOS_WITH_TITLE flag is enabled by default.
                    mDescriptionStringId = R.string.sync_promo_description_bookmarks;
                    mDescriptionStringIdNoAccount = R.string.sync_promo_description_bookmarks;
                } else {
                    mDescriptionStringId = R.string.signin_promo_description_bookmarks;
                    mDescriptionStringIdNoAccount =
                            R.string.signin_promo_description_bookmarks_no_account;
                }
                break;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                mImpressionCountName = ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_NTP;
                mImpressionUserActionName = "Signin_Impression_FromNTPContentSuggestions";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromNTPContentSuggestions";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromNTPContentSuggestions";
                mSigninWithDefaultUserActionName =
                        "Signin_SigninWithDefault_FromNTPContentSuggestions";
                mSigninNotDefaultUserActionName =
                        "Signin_SigninNotDefault_FromNTPContentSuggestions";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromNTPContentSuggestions";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED;
                mImpressionsTilDismissHistogramName = null;
                mTitleStringId = R.string.sync_promo_title_ntp_content_suggestions;
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
                    mDescriptionStringId = R.string.sync_promo_description_ntp_content_suggestions;
                    mDescriptionStringIdNoAccount =
                            R.string.sync_promo_description_ntp_content_suggestions;
                } else {
                    mDescriptionStringId =
                            R.string.signin_promo_description_ntp_content_suggestions;
                    mDescriptionStringIdNoAccount =
                            R.string.signin_promo_description_ntp_content_suggestions_no_account;
                }
                break;
            case SigninAccessPoint.RECENT_TABS:
                // There is no impression limit for Recent Tabs.
                mImpressionCountName = null;
                mImpressionUserActionName = "Signin_Impression_FromRecentTabs";
                mImpressionWithAccountUserActionName =
                        "Signin_ImpressionWithAccount_FromRecentTabs";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromRecentTabs";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromRecentTabs";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromRecentTabs";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromRecentTabs";
                mSyncPromoDismissedPreferenceTracker = null;
                mImpressionsTilDismissHistogramName = null;
                mTitleStringId = R.string.sync_promo_title_recent_tabs;
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
                    mDescriptionStringId = R.string.sync_promo_description_recent_tabs;
                    mDescriptionStringIdNoAccount = R.string.sync_promo_description_recent_tabs;
                } else {
                    mDescriptionStringId = R.string.signin_promo_description_recent_tabs;
                    mDescriptionStringIdNoAccount =
                            R.string.signin_promo_description_recent_tabs_no_account;
                }
                break;
            case SigninAccessPoint.SETTINGS:
                mImpressionCountName = ChromePreferenceKeys.SIGNIN_PROMO_IMPRESSIONS_COUNT_SETTINGS;
                mImpressionUserActionName = "Signin_Impression_FromSettings";
                mImpressionWithAccountUserActionName = "Signin_ImpressionWithAccount_FromSettings";
                mSigninWithDefaultUserActionName = "Signin_SigninWithDefault_FromSettings";
                mSigninNotDefaultUserActionName = "Signin_SigninNotDefault_FromSettings";
                // On Android, the promo does not have a button to add and account when there is
                // already an account on the device. Always use the NoExistingAccount variant.
                mSigninNewAccountUserActionName =
                        "Signin_SigninNewAccountNoExistingAccount_FromSettings";
                mImpressionWithNoAccountUserActionName =
                        "Signin_ImpressionWithNoAccount_FromSettings";
                mSyncPromoDismissedPreferenceTracker =
                        ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED;
                mImpressionsTilDismissHistogramName =
                        "MobileSignInPromo.SettingsManager.ImpressionsTilDismiss";
                mTitleStringId = R.string.sync_promo_title_settings;
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
                    mDescriptionStringId = R.string.sync_promo_description_settings;
                    mDescriptionStringIdNoAccount = R.string.sync_promo_description_settings;
                } else {
                    mDescriptionStringId = R.string.signin_promo_description_settings;
                    mDescriptionStringIdNoAccount =
                            R.string.signin_promo_description_settings_no_account;
                }
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point: " + mAccessPoint);
        }
    }

    /**
     * Sets up the sync promo view.
     * @param profileDataCache The {@link ProfileDataCache} that stores profile data.
     * @param view The {@link PersonalizedSigninPromoView} that should be set up.
     * @param listener The {@link SigninPromoController.OnDismissListener} to be set to the view.
     */
    public void setUpSyncPromoView(ProfileDataCache profileDataCache,
            PersonalizedSigninPromoView view, SigninPromoController.OnDismissListener listener) {
        final IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        assert !identityManager.hasPrimaryAccount(ConsentLevel.SYNC) : "Sync is already enabled!";

        final @Nullable Account visibleAccount = getVisibleAccount();
        // Set up the sync promo
        if (visibleAccount == null) {
            setupPromoView(view, /* profileData= */ null, listener);
            return;
        }
        setupPromoView(
                view, profileDataCache.getProfileDataOrDefault(visibleAccount.name), listener);
    }

    /**
     * Called when the signin promo is destroyed.
     */
    public void onPromoDestroyed() {
        if (!mWasDisplayed || mWasUsed || mImpressionsTilDismissHistogramName == null) {
            return;
        }
        RecordHistogram.recordCount100Histogram(
                mImpressionsTilDismissHistogramName, getNumImpressions());
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
    private void setupPromoView(PersonalizedSigninPromoView view,
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
        mWasDisplayed = true;
        if (mProfileData == null) {
            setupColdState(view);
        } else {
            setupHotState(view);
        }

        if (onDismissListener != null) {
            // Recent Tabs promos can't be dismissed.
            assert mAccessPoint != SigninAccessPoint.RECENT_TABS;
            view.getDismissButton().setVisibility(View.VISIBLE);
            view.getDismissButton().setOnClickListener(promoView -> {
                assert mSyncPromoDismissedPreferenceTracker != null;
                mWasUsed = true;
                SharedPreferencesManager.getInstance().writeBoolean(
                        mSyncPromoDismissedPreferenceTracker, true);
                recordShowCountHistogram(UserAction.DISMISSED);
                onDismissListener.onDismiss();
            });
        } else {
            view.getDismissButton().setVisibility(View.GONE);
        }
    }

    /**
     * Should be called when the view is not in use anymore (e.g. it's being recycled).
     */
    public void detach() {
        if (mImpressionTracker != null) {
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    /** Increases promo show count by one. */
    public void increasePromoShowCount() {
        if (mAccessPoint != SigninAccessPoint.RECENT_TABS) {
            SharedPreferencesManager.getInstance().incrementInt(
                    getPromoShowCountPreferenceName(mAccessPoint));
        }
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        recordShowCountHistogram(UserAction.SHOWN);

        if (mAccessPoint == SigninAccessPoint.NTP_CONTENT_SUGGESTIONS) {
            final long currentTime = System.currentTimeMillis();
            if (SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME)
                    == 0) {
                SharedPreferencesManager.getInstance().writeLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, currentTime);
            }
            SharedPreferencesManager.getInstance().writeLong(
                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, currentTime);
        }
    }

    private void setupColdState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        view.getImage().setImageResource(R.drawable.chrome_sync_logo);
        setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
            // TODO(crbug.com/1323197): remove getDescription() or getNewDescription if the feature
            // enabled or disabled by default.
            view.getDescription().setVisibility(View.GONE);
            view.getNewDescription().setText(mDescriptionStringIdNoAccount);
            view.getTitle().setVisibility(View.VISIBLE);
            view.getTitle().setText(mTitleStringId);
        } else {
            view.getNewDescription().setVisibility(View.GONE);
            view.getDescription().setText(mDescriptionStringIdNoAccount);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON)) {
            view.getPrimaryButton().setText(R.string.sync_promo_continue);
        } else {
            view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
        }
        view.getPrimaryButton().setOnClickListener(v -> signinWithNewAccount(context));

        view.getSecondaryButton().setVisibility(View.GONE);
    }

    private void setupHotState(PersonalizedSigninPromoView view) {
        final Context context = view.getContext();
        Drawable accountImage = mProfileData.getImage();
        view.getImage().setImageDrawable(accountImage);
        setImageSize(context, view, R.dimen.signin_promo_account_image_size);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE)) {
            view.getDescription().setVisibility(View.GONE);
            view.getNewDescription().setText(mDescriptionStringId);
            view.getTitle().setVisibility(View.VISIBLE);
            view.getTitle().setText(mTitleStringId);
        } else {
            view.getNewDescription().setVisibility(View.GONE);
            view.getDescription().setText(mDescriptionStringId);
        }

        view.getPrimaryButton().setOnClickListener(v -> signinWithDefaultAccount(context));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON)) {
            view.getPrimaryButton().setText(R.string.sync_promo_continue);
            view.getSecondaryButton().setVisibility(View.GONE);
            return;
        }
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
            view.getSecondaryButton().setVisibility(View.GONE);
            return;
        }
        view.getPrimaryButton().setText(context.getString(
                R.string.signin_promo_continue_as, mProfileData.getGivenNameOrFullNameOrEmail()));

        view.getSecondaryButton().setText(R.string.signin_promo_choose_another_account);
        view.getSecondaryButton().setOnClickListener(v -> signinWithNotDefaultAccount(context));
        view.getSecondaryButton().setVisibility(View.VISIBLE);
    }

    private int getNumImpressions() {
        return SharedPreferencesManager.getInstance().readInt(mImpressionCountName);
    }

    private void signinWithNewAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNewAccountUserActionName);
        mSyncConsentActivityLauncher.launchActivityForPromoAddAccountFlow(context, mAccessPoint);
    }

    private void signinWithDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninWithDefaultUserActionName);
        mSyncConsentActivityLauncher.launchActivityForPromoDefaultFlow(
                context, mAccessPoint, mProfileData.getAccountEmail());
    }

    private void signinWithNotDefaultAccount(Context context) {
        recordSigninButtonUsed();
        RecordUserAction.record(mSigninNotDefaultUserActionName);
        mSyncConsentActivityLauncher.launchActivityForPromoChooseAccountFlow(
                context, mAccessPoint, mProfileData.getAccountEmail());
    }

    private void recordSigninButtonUsed() {
        mWasUsed = true;
        recordShowCountHistogram(UserAction.CONTINUED);
    }

    private void recordShowCountHistogram(@UserAction String actionType) {
        final String accessPoint;
        switch (mAccessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER:
                accessPoint = AccessPointId.BOOKMARKS;
                break;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                accessPoint = AccessPointId.NTP;
                break;
            case SigninAccessPoint.RECENT_TABS:
                accessPoint = AccessPointId.RECENT_TABS;
                break;
            case SigninAccessPoint.SETTINGS:
                accessPoint = AccessPointId.SETTINGS;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unexpected value for access point" + mAccessPoint);
        }
        RecordHistogram.recordExactLinearHistogram(
                "Signin.SyncPromo." + actionType + ".Count." + accessPoint,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT),
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
        if (mProfileData == null) {
            RecordUserAction.record(mImpressionWithNoAccountUserActionName);
        } else {
            RecordUserAction.record(mImpressionWithAccountUserActionName);
        }

        // If mImpressionCountName is not null then we should record impressions.
        if (mImpressionCountName != null) {
            SharedPreferencesManager.getInstance().incrementInt(mImpressionCountName);
        }
    }

    @VisibleForTesting
    public static void setPrefSigninPromoDeclinedBookmarksForTests(boolean isDeclined) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, isDeclined);
    }

    @VisibleForTesting
    public static int getMaxImpressionsBookmarksForTests() {
        return MAX_IMPRESSIONS_BOOKMARKS;
    }
}
