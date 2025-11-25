// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/** {@link SigninPromoDelegate} for bookmark signin promo. */
@NullMarked
public class BookmarkSigninPromoDelegate extends SigninPromoDelegate {

    /** Indicates the type of content the should be shown in the visible promo. */
    @IntDef({PromoState.NONE, PromoState.SIGNIN, PromoState.ACCOUNT_SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /** The promo content should promote sign-in. Shown to signed-out user. */
        int SIGNIN = 1;

        /**
         * The promo content should promote enabling bookmark sync in the settings. Shown to
         * signed-in user with bookmarks or reading list sync disabled in settings.
         */
        int ACCOUNT_SETTINGS = 2;
    }

    @VisibleForTesting static final int MAX_IMPRESSIONS_BOOKMARKS = 20;

    private final String mPromoShowCountPreferenceName;
    private final Runnable mOnOpenSettingsClicked;
    private @PromoState int mPromoState = PromoState.NONE;

    public BookmarkSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange,
            Runnable onOpenSettingsClicked) {
        super(context, profile, launcher, onPromoStateChange);

        mPromoShowCountPreferenceName =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS);
        mOnOpenSettingsClicked = onOpenSettingsClicked;
    }

    @Override
    String getTitle() {
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        switch (mPromoState) {
            case PromoState.SIGNIN:
                if (seamlessSigninStringType
                                == SigninFeatureMap.SeamlessSigninStringType.NON_SEAMLESS
                        || seamlessSigninStringType
                                == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
                    return mContext.getString(R.string.signin_promo_title_bookmarks);
                }
                return mContext.getString(R.string.signin_account_picker_bottom_sheet_title);
            case PromoState.ACCOUNT_SETTINGS:
                return mContext.getString(R.string.sync_promo_title_bookmarks);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    String getDescription(@Nullable String accountEmail) {
        @SigninFeatureMap.SeamlessSigninPromoType
        int seamlessSigninPromoType = SigninFeatureMap.getInstance().getSeamlessSigninPromoType();
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        switch (mPromoState) {
            case PromoState.SIGNIN:
                if (accountEmail == null) {
                    return mContext.getString(R.string.signin_promo_description_bookmarks);
                }
                if (seamlessSigninStringType
                        == SigninFeatureMap.SeamlessSigninStringType.CONTINUE_BUTTON) {
                    if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
                        return mContext.getString(
                                R.string.signin_promo_description_bookmarks_group1, accountEmail);
                    } else if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                        return mContext.getString(
                                R.string.signin_promo_description_bookmarks_group2);
                    }
                } else if (seamlessSigninStringType
                        == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
                    if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
                        return mContext.getString(
                                R.string.signin_promo_description_bookmarks_group3, accountEmail);
                    } else if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                        return mContext.getString(
                                R.string.signin_promo_description_bookmarks_group4);
                    }
                }
                return mContext.getString(R.string.signin_promo_description_bookmarks);
            case PromoState.ACCOUNT_SETTINGS:
                return mContext.getString(R.string.account_settings_promo_description_bookmarks);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    @SigninPreferencesManager.SigninPromoAccessPointId
    String getAccessPointName() {
        return SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS;
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.BOOKMARK_MANAGER;
    }

    @Override
    void onDismissButtonClicked() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, true);
    }

    @Override
    boolean canShowPromo() {
        return mPromoState != PromoState.NONE;
    }

    @Override
    boolean refreshPromoState(@Nullable CoreAccountInfo visibleAccount) {
        @PromoState int newState = computePromoState();
        boolean wasStateChanged = mPromoState != newState;
        mPromoState = newState;
        return wasStateChanged;
    }

    @Override
    boolean isSeamlessSigninAllowed() {
        return SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
    }

    @Override
    boolean shouldHideSecondaryButton() {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                return false;
            case PromoState.ACCOUNT_SETTINGS:
                return true;
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                return super.getTextForPrimaryButton(profileData);
            case PromoState.ACCOUNT_SETTINGS:
                return mContext.getString(R.string.open_settings_button);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    void recordImpression() {
        ChromeSharedPreferences.getInstance().incrementInt(mPromoShowCountPreferenceName);
    }

    @Override
    boolean isMaxImpressionsReached() {
        return getPromoShownCount() >= MAX_IMPRESSIONS_BOOKMARKS;
    }

    @Override
    void onPrimaryButtonClicked(@Nullable CoreAccountInfo visibleAccount) {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                super.onPrimaryButtonClicked(visibleAccount);
                break;
            case PromoState.ACCOUNT_SETTINGS:
                mOnOpenSettingsClicked.run();
                break;
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    int getPromoShownCount() {
        return ChromeSharedPreferences.getInstance().readInt(mPromoShowCountPreferenceName);
    }

    private @PromoState int computePromoState() {
        if (wasPromoDeclined() || !canManuallyEnableSyncTypes()) {
            return PromoState.NONE;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return PromoState.ACCOUNT_SETTINGS;
        }

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assumeNonNull(signinManager);
        return signinManager.isSigninAllowed() ? PromoState.SIGNIN : PromoState.NONE;
    }

    private boolean wasPromoDeclined() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);
    }

    private boolean canManuallyEnableSyncTypes() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        assumeNonNull(syncService);

        for (@UserSelectableType
        int type : Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST)) {
            boolean isTypeEnabled = syncService.getSelectedTypes().contains(type);
            boolean isTypeManaged = syncService.isTypeManagedByPolicy(type);
            if (!isTypeEnabled && !isTypeManaged) {
                return true;
            }
        }
        return false;
    }

    @Override
    boolean shouldDisplaySignedInLayout() {
        return mPromoState == PromoState.ACCOUNT_SETTINGS;
    }
}
