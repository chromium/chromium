// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator.Delegate;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.CreateProfileDataCache;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.NotifyInitialLoadCompleted;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordLoadCompleted;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.AccountsUpdated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.ChildAccountStatusLoaded;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.PoliciesLoaded;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.ProfileDataCacheCreated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninReducer.Result;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninState.Phase;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManager.RefreshTokensLoadingState;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;

/**
 * State-machine implementation of {@link FullscreenSigninCoordinator.Mediator}, behind {@link
 * org.chromium.components.signin.SigninFeatures#FULLSCREEN_SIGNIN_MEDIATOR_STATE_MACHINE}.
 *
 * <p>Decisions are made by {@link FullscreenSigninReducer}. This class feeds it events, runs the
 * effects it returns and renders the resulting {@link FullscreenSigninState} into the model.
 */
@NullMarked
class FullscreenSigninMediator implements FullscreenSigninCoordinator.Mediator {
    private final Context mContext;
    private final Delegate mDelegate;
    private final @SigninAccessPoint int mAccessPoint;
    private final PropertyModel mModel;
    private final ProfileDataCache.Observer mProfileDataCacheObserver =
            new ProfileDataCache.Observer() {
                @Override
                public void onAccountsUpdated(List<DisplayableProfileData> accounts) {
                    List<CoreAccountId> accountIds = getAccountIdsIfLoaded();
                    if (accountIds != null) dispatch(new AccountsUpdated(accountIds));
                }

                @Override
                public void onProfileDataUpdated(DisplayableProfileData profileData) {
                    // The selected account's name or avatar can arrive after its account.
                    CoreAccountId selectedAccountId = mState.getSelectedAccountId();
                    if (selectedAccountId != null
                            && selectedAccountId.equals(profileData.getAccountId())) {
                        render();
                    }
                }
            };

    private FullscreenSigninConfig mConfig;
    private FullscreenSigninState mState = new FullscreenSigninState();
    // Created once native is ready. Provides the account list and the selected account's data.
    private @Nullable ProfileDataCache mProfileDataCache;
    // Set with the cache.
    private @Nullable IdentityManager mIdentityManager;
    // Events dispatched while another event is being handled, in order.
    private final ArrayDeque<FullscreenSigninEvent> mPendingEvents = new ArrayDeque<>();
    private boolean mIsDispatching;
    private boolean mDestroyed;

    FullscreenSigninMediator(
            Context context,
            ModalDialogManager modalDialogManager,
            Delegate delegate,
            PrivacyPreferencesManager privacyPreferencesManager,
            FullscreenSigninConfig config,
            @SigninAccessPoint int accessPoint) {
        mContext = context;
        mDelegate = delegate;
        mConfig = config;
        mAccessPoint = accessPoint;

        mModel =
                FullscreenSigninProperties.createModel(
                        this::onSelectedAccountClicked,
                        this::onContinueAsClicked,
                        this::onDismissClicked,
                        ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                                && !mConfig.shouldDisableSignin,
                        mConfig.logoId,
                        mContext.getString(R.string.fre_welcome),
                        mConfig.subtitle,
                        mConfig.dismissText,
                        /* showInitialLoadProgressSpinner= */ true);

        mDelegate
                .getNativeInitializationPromise()
                .then(
                        result -> {
                            onNativeLoaded();
                        });
        // An input that already has a value counts as loaded now (the suppliers post their
        // callbacks, which could otherwise arrive after other inputs).
        if (mDelegate.getPolicyLoadListener().get() != null) {
            onPolicyLoad();
        } else {
            mDelegate.getPolicyLoadListener().onAvailable(hasPolicies -> onPolicyLoad());
        }
        if (mDelegate.getChildAccountStatusSupplier().get() != null) {
            onChildAccountStatusAvailable();
        } else {
            mDelegate
                    .getChildAccountStatusSupplier()
                    .onAvailable(ignored -> onChildAccountStatusAvailable());
        }

        SigninMetricsUtils.logSigninStarted(mAccessPoint);
        render();
    }

    @Override
    public PropertyModel getModel() {
        return mModel;
    }

    @Override
    public void destroy() {
        assert !mDestroyed;
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(mProfileDataCacheObserver);
            mProfileDataCache = null;
        }
        mDestroyed = true;
    }

    @Override
    public void reset() {
        // TODO(crbug.com/579875669): Clear the sign-in spinners and animation when the user comes
        // back to this screen.
    }

    @Override
    public boolean isContinueOrDismissClicked() {
        // TODO(crbug.com/579875669): Return whether sign-in or dismissal is in progress.
        return false;
    }

    @Override
    public void onAccountAdded(String accountEmail) {
        // TODO(crbug.com/579875669): Select the newly added account.
    }

    // TODO(crbug.com/486190792): Remove once FREStringChanges feature is launched.
    @Override
    public void setConfig(FullscreenSigninConfig config) {
        mConfig = config;
    }

    @Override
    public void onAddAccountCanceled() {
        // TODO(crbug.com/579875669): Handle the add-account flow being canceled.
    }

    @Override
    public void onDeviceLockCanceled() {
        // TODO(crbug.com/579875669): Let the user continue after device lock is canceled.
    }

    @Override
    public void proceedWithSignIn() {
        // TODO(crbug.com/579875669): Sign in with the selected account.
    }

    @Override
    public void dismiss() {
        // TODO(crbug.com/579875669): Dismiss the screen without signing in.
    }

    @Override
    public Drawable getProfilePictureForTesting() {
        return mModel.get(FullscreenSigninProperties.PROFILE_PICTURE);
    }

    @Override
    public void setStartAnimationForTesting(boolean start) {
        mModel.set(FullscreenSigninProperties.START_ANIMATION, start);
    }

    @Override
    public @Nullable BadgeConfig getSigninAnimationBadgeConfigForTesting() {
        // TODO(crbug.com/579875669): Return the selected account's sign-in animation badge.
        return null;
    }

    @Override
    public @Nullable BadgeConfig getContinueButtonBadgeConfigForTesting() {
        // TODO(crbug.com/579875669): Return the selected account's continue button badge.
        return null;
    }

    private void onNativeLoaded() {
        dispatch(new NativeReady());
    }

    private void onPolicyLoad() {
        dispatch(new PoliciesLoaded());
    }

    private void onChildAccountStatusAvailable() {
        dispatch(new ChildAccountStatusLoaded());
    }

    /**
     * Reduces {@code event}, renders the new state and runs the resulting effects in order. An
     * event dispatched while another is being handled (from {@link #render()} or an effect) is
     * queued and handled after it.
     */
    private void dispatch(FullscreenSigninEvent event) {
        // Inputs arrive asynchronously, so they can arrive after the mediator is destroyed. See
        // https://crbug.com/40214143.
        if (mDestroyed) return;
        mPendingEvents.add(event);
        if (mIsDispatching) return;
        mIsDispatching = true;
        // An effect can destroy the mediator, so stop handling effects and events once it is.
        while (!mDestroyed && !mPendingEvents.isEmpty()) {
            Result result = FullscreenSigninReducer.reduce(mState, mPendingEvents.remove());
            mState = result.getState();
            // Render before running effects, so the delegate sees the model for the new state (e.g.
            // the FRE swaps its layout when told the initial load completed).
            render();
            for (FullscreenSigninEffect effect : result.getEffects()) {
                if (mDestroyed) break;
                runEffect(effect);
            }
        }
        mIsDispatching = false;
    }

    private void runEffect(FullscreenSigninEffect effect) {
        switch (effect) {
            case RecordNativeInitialized _ -> mDelegate.recordNativeInitializedHistogram();
            case CreateProfileDataCache _ -> {
                assert mProfileDataCache == null;
                // TODO(crbug.com/571803558): Remove once MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS
                // is enabled by default. The cache must read accounts from IdentityManager too.
                assert SigninFeatureMap.isEnabled(
                        SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
                IdentityManager identityManager =
                        assertNonNull(
                                IdentityServicesProvider.get()
                                        .getIdentityManager(
                                                assertNonNull(mDelegate.getProfileSupplier().get())
                                                        .getOriginalProfile()));
                mIdentityManager = identityManager;
                ProfileDataCache profileDataCache =
                        ProfileDataCache.createWithDefaultImageSize(mContext, identityManager);
                mProfileDataCache = profileDataCache;
                profileDataCache.addObserver(mProfileDataCacheObserver);
                // The cache doesn't notify its observers about the accounts it already has, so
                // send them with ProfileDataCacheCreated.
                dispatch(new ProfileDataCacheCreated(getAccountIdsIfLoaded()));
            }
            case NotifyInitialLoadCompleted _ -> mDelegate.onInitialLoadCompleted();
            case RecordLoadCompleted e ->
                    mDelegate.recordLoadCompletedHistograms(e.getSlowestLoadPoint());
        }
    }

    private void render() {
        mModel.set(
                FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER,
                mState.getPhase() == Phase.LOADING);
        // An account can only be selected once the cache has loaded the accounts.
        @Nullable CoreAccountId selectedAccountId = mState.getSelectedAccountId();
        mModel.set(
                FullscreenSigninProperties.BOTTOM_GROUP_ACCOUNT_DATA,
                selectedAccountId == null
                        ? null
                        : assertNonNull(mProfileDataCache).getById(selectedAccountId));
    }

    /**
     * Returns the account IDs, or null if the accounts haven't finished loading.
     *
     * <p>Accounts are read only from IdentityManager. This assumes
     * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS is enabled (so {@link ProfileDataCache} also reads
     * from IdentityManager), which must be the default before this mediator launches.
     */
    private @Nullable List<CoreAccountId> getAccountIdsIfLoaded() {
        if (!assertNonNull(mProfileDataCache).getAccounts().isFulfilled()) return null;
        IdentityManager identityManager = assertNonNull(mIdentityManager);
        // As in ProfileDataCache, a failed load counts as no accounts.
        if (identityManager.getRefreshTokensLoadingState() != RefreshTokensLoadingState.SUCCEEDED) {
            return List.of();
        }
        List<CoreAccountId> accountIds = new ArrayList<>();
        for (AccountInfo account :
                identityManager.getExtendedAccountInfoForAccountsWithRefreshToken()) {
            accountIds.add(account.getId());
        }
        return accountIds;
    }

    private void onSelectedAccountClicked() {
        // TODO(crbug.com/579875669): Open the account picker.
    }

    private void onContinueAsClicked() {
        // TODO(crbug.com/579875669): Start signing in with the selected account.
    }

    private void onDismissClicked() {
        // TODO(crbug.com/579875669): Start dismissing the screen.
    }
}
