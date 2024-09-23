// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Lifecycle;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.consent_auditor.ConsentAuditorFeature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.DataWipeOption;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.device_lock.DeviceLockCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncFirstSetupCompleteSource;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Contains the common logic of fragments used to sign in and enable sync.
 * The account to sync is selected among the list of Google Accounts present on
 * the device. A new account can also be added to the list, e.g. if there was
 * none at first.
 * Derived classes must implement {@link #onSyncAccepted}/{@link #onSyncRefused} to define
 * what happens after the signin flow.
 */
public abstract class SyncConsentFragmentBase extends Fragment
        implements AccountPickerCoordinator.Listener,
                AccountsChangeObserver,
                SigninManager.SignInStateObserver,
                DeviceLockCoordinator.Delegate {
    private static final String ARGUMENT_ACCESS_POINT = "SyncConsentFragmentBase.AccessPoint";
    private static final String ARGUMENT_ACCOUNT_NAME = "SyncConsentFragmentBase.AccountName";

    // This bundle argument is optional; it is set only if the child status cannot be reliably
    // inferred by looking at the last used regular profile, because child sign auto sign in may
    // not have completed.
    private static final String ARGUMENT_CHILD_ACCOUNT_STATUS =
            "SyncConsentFragmentBase.ChildAccountStatus";
    private static final String ARGUMENT_SIGNIN_FLOW_TYPE =
            "SyncConsentFragmentBase.SigninFlowType";

    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    private static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    @IntDef({SigninFlowType.DEFAULT, SigninFlowType.CHOOSE_ACCOUNT, SigninFlowType.ADD_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface SigninFlowType {
        int DEFAULT = 0;
        int CHOOSE_ACCOUNT = 1;
        int ADD_ACCOUNT = 2;
    }

    private final AccountManagerFacade mAccountManagerFacade;
    protected boolean mIsChild;

    private FrameLayout mFrameLayout;
    private SigninView mSigninView;
    private ConsentTextTracker mConsentTextTracker;

    private final ProfileDataCache.Observer mProfileDataCacheObserver;
    protected @Nullable String mSelectedAccountEmail;
    private ProfileDataCache mProfileDataCache;
    // Set to true when the user clicks "Yes, I'm in" or "settings" and the class consequently
    // triggers sign-in (asynchronous). The buttons are not clickable in this state, see
    // areControlsEnabled().
    // If sign-in fails, the fragment must not be closed, as this would give a false impression of
    // success. Instead, this member should be set to false to give the user a chance of clicking
    // "No, thanks".
    private boolean mIsSigninInProgress;
    // Set to true when the fragment is launched for add account flow. The value is only checked in
    // tangible sync flow where the activity would otherwise be terminated if selected account is
    // not provided.
    private boolean mCanUseGooglePlayServices;
    private boolean mRecordUndoSignin;
    private boolean mSyncStartedRecorded;
    private boolean mIsSignedInWithoutSync;
    protected @SigninAccessPoint int mSigninAccessPoint;
    private ModalDialogManager mModalDialogManager;
    private ConfirmSyncDataStateMachine mConfirmSyncDataStateMachine;
    private @Nullable AccountPickerDialogCoordinator mAccountPickerDialogCoordinator;
    private @Nullable DeviceLockCoordinator mDeviceLockCoordinator;

    private Runnable mDeviceLockPageCallback;
    private boolean mDeviceLockReady;

    /**
     * Creates an argument bundle for the default {@link SyncConsentFragment} flow.
     * (account selection is enabled, etc.).
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Bundle createArguments(
            @SigninAccessPoint int accessPoint, @Nullable String accountName) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.DEFAULT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putString(ARGUMENT_ACCOUNT_NAME, accountName);
        return result;
    }

    /**
     * Creates an argument bundle for the default {@link SyncConsentFragment} flow with
     * {@link ChildAccountStatus}.
     *
     * This version of the method should be used where we cannot guarantee that child auto-signin
     * has completed and therefore the child status is explicitly provided.
     *
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect.
     * @param isChild Whether the selected account is a child one.
     */
    protected static Bundle createArguments(
            @SigninAccessPoint int accessPoint, String accountName, boolean isChild) {
        Bundle result = createArguments(accessPoint, accountName);
        result.putBoolean(ARGUMENT_CHILD_ACCOUNT_STATUS, isChild);
        return result;
    }

    /**
     * Creates an argument bundle for "Choose account" sign-in flow. Account selection dialog will
     * be shown at the start of the sign-in process.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    protected static Bundle createArgumentsForChooseAccountFlow(
            @SigninAccessPoint int accessPoint, @Nullable String accountName) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.CHOOSE_ACCOUNT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putString(ARGUMENT_ACCOUNT_NAME, accountName);
        return result;
    }

    /**
     * Creates an argument bundle for "Add account" sign-in flow. Activity to add an account will be
     * shown at the start of the sign-in process.
     */
    protected static Bundle createArgumentsForAddAccountFlow(@SigninAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.ADD_ACCOUNT);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        return result;
    }

    protected SyncConsentFragmentBase() {
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mProfileDataCacheObserver = this::updateProfileData;
        mCanUseGooglePlayServices = true;
    }

    /** The sync consent was refused. */
    protected abstract void onSyncRefused();

    /**
     * The sync consent was accepted.
     *
     * @param accountName The name of the account
     * @param settingsClicked Whether the user requested to see their sync settings
     * @param callback The callback invoked when the process of enabling sync is finished or aborted
     */
    protected abstract void onSyncAccepted(
            String accountName, boolean settingsClicked, SigninManager.SignInCallback callback);

    /**
     * Called if signinAndEnableSync() succeeds.
     *
     * @param settingsClicked Whether the user requested to see their sync settings
     */
    protected abstract void closeAndMaybeOpenSyncSettings(boolean settingsClicked);

    private SigninManager.SignInCallback newSignInCallback(
            Profile profile, boolean settingsClicked, SigninManager.SignInCallback callback) {
        return new SigninManager.SignInCallback() {
            @Override
            public void onSignInComplete() {
                SyncService syncService = SyncServiceFactory.getForProfile(profile);
                if (!settingsClicked) {
                    UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                            profile, true);
                    syncService.setInitialSyncFeatureSetupComplete(
                            SyncFirstSetupCompleteSource.BASIC_FLOW);
                }
                closeAndMaybeOpenSyncSettings(settingsClicked);
                callback.onSignInComplete();
            }

            @Override
            public void onSignInAborted() {
                callback.onSignInAborted();
            }
        };
    }

    // TODO(crbug.com/40217047): |callback| is only used to set |mIsSigninInProgress| to false. Once
    // this method replaces onSyncAccepted(), the field can be set directly.
    // TODO(crbug.com/40274844): Refactor method to take CoreAccountInfo instead of String email.
    protected void signinAndEnableSync(
            String accountEmail, boolean settingsClicked, SigninManager.SignInCallback callback) {
        // Getting the profile depends on the Activity, which may be gone by the time the callback
        // runs.
        final Profile profile = getProfile();
        AccountManagerFacadeProvider.getInstance()
                .getCoreAccountInfos()
                .then(
                        coreAccountInfos -> {
                            @Nullable
                            CoreAccountInfo coreAccountInfo =
                                    AccountUtils.findCoreAccountInfoByEmail(
                                            coreAccountInfos, accountEmail);
                            if (coreAccountInfo == null) {
                                callback.onSignInAborted();
                                return;
                            }
                            SigninManager signinManager =
                                    IdentityServicesProvider.get().getSigninManager(profile);
                            signinManager.signinAndEnableSync(
                                    coreAccountInfo,
                                    mSigninAccessPoint,
                                    newSignInCallback(profile, settingsClicked, callback));
                        });
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mModalDialogManager = ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle arguments = getArguments();
        mSigninAccessPoint = arguments.getInt(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX);
        assert mSigninAccessPoint != SigninAccessPoint.MAX : "Cannot find SigninAccessPoint!";

        // TODO(crbug.com/40828116): remove usage of Profile.isChild() and the need for a bundle
        // argument in the FRE, but moving to a new API for determining device supervision status.
        mSelectedAccountEmail = arguments.getString(ARGUMENT_ACCOUNT_NAME, null);
        if (arguments.containsKey(ARGUMENT_CHILD_ACCOUNT_STATUS)) {
            mIsChild = arguments.getBoolean(ARGUMENT_CHILD_ACCOUNT_STATUS);
        } else {
            mIsChild = getProfile().isChild();
        }

        @SigninFlowType
        int signinFlowType = arguments.getInt(ARGUMENT_SIGNIN_FLOW_TYPE, SigninFlowType.DEFAULT);

        if (savedInstanceState == null) {
            // If this fragment is being recreated from a saved state there's no need to show
            // account picked or starting AddAccount flow.
            if (signinFlowType == SigninFlowType.CHOOSE_ACCOUNT) {
                mAccountPickerDialogCoordinator =
                        new AccountPickerDialogCoordinator(
                                requireContext(), this, mModalDialogManager);
            } else if (signinFlowType == SigninFlowType.ADD_ACCOUNT) {
                addAccount();
            }
        }

        mConsentTextTracker = new ConsentTextTracker(getResources());

        mProfileDataCache =
                mIsChild
                        ? ProfileDataCache.createWithDefaultImageSize(
                                requireContext(), R.drawable.ic_account_child_20dp)
                        : ProfileDataCache.createWithDefaultImageSizeAndNoBadge(requireContext());
        mProfileDataCache.addObserver(mProfileDataCacheObserver);

        IdentityServicesProvider.get().getSigninManager(getProfile()).addSignInStateObserver(this);

        // By default this is set to true so that when system back button is pressed user action
        // is recorded in onDestroy().
        mRecordUndoSignin = true;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        IdentityServicesProvider.get()
                .getSigninManager(getProfile())
                .removeSignInStateObserver(this);
        mProfileDataCache.removeObserver(mProfileDataCacheObserver);
        if (mConfirmSyncDataStateMachine != null) {
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed= */ true);
            mConfirmSyncDataStateMachine = null;
        }
        mModalDialogManager.destroy();
        if (mRecordUndoSignin) RecordUserAction.record("Signin_Undo_Signin");
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mFrameLayout = new FrameLayout(getContext());
        createSigninView(inflater, container);

        updateConsentText();
        mFrameLayout.addView(mSigninView);
        return mFrameLayout;
    }

    /**
     * Displays the device lock page UI on the SyncConsentFragment to inform the user about data
     * privacy on the device.
     * @param onSuccess The callback to run if the user successfully passes the device lock page
     *                  prompts.
     */
    protected void displayDeviceLockPage(Runnable onSuccess) {
        mDeviceLockPageCallback = onSuccess;

        // Getting the profile depends on the Activity, which may be gone by the time the callback
        // runs.
        Profile profile = getProfile();
        mAccountManagerFacade
                .getCoreAccountInfos()
                .then(
                        (coreAccountInfos) -> {
                            Activity activity = getActivity();
                            if (activity == null
                                    || activity.isFinishing()
                                    || activity.isDestroyed()) {
                                return;
                            }

                            CoreAccountInfo selectedCoreAccountInfo =
                                    AccountUtils.findCoreAccountInfoByEmail(
                                            coreAccountInfos, mSelectedAccountEmail);
                            assert selectedCoreAccountInfo != null;

                            mDeviceLockCoordinator =
                                    new DeviceLockCoordinator(
                                            this,
                                            getWindowAndroid(),
                                            profile,
                                            activity,
                                            CoreAccountInfo.getAndroidAccountFrom(
                                                    selectedCoreAccountInfo));
                        });
    }

    public boolean getDeviceLockReadyForTesting() {
        return mDeviceLockReady;
    }

    private void createSigninView(LayoutInflater inflater, ViewGroup container) {
        mSigninView = (SigninView) inflater.inflate(R.layout.signin_view, container, false);

        // Buttons are temporary to satisfy view calculations. Will be replaced by target
        // ones with recreateButtons call originating at
        // SyncConsentFragmentBase.updateProfileData once
        // IdentityManager provides the data on how to display them
        mSigninView.getAcceptButton().setVisibility(View.GONE);
        mSigninView.getRefuseButton().setVisibility(View.GONE);

        mSigninView.getAccountPickerView().setOnClickListener(view -> onAccountPickerClicked());
        mSigninView.getRefuseButton().setOnClickListener(this::onRefuseButtonClicked);

        mSigninView.getButtonBar().setVisibility(View.GONE);
        mSigninView.getMoreButton().setVisibility(View.VISIBLE);
        mSigninView
                .getMoreButton()
                .setOnClickListener(
                        view -> {
                            mSigninView
                                    .getScrollView()
                                    .smoothScrollBy(0, mSigninView.getScrollView().getHeight());
                            // TODO(crbug.com/41376043): Revise this user action.
                            RecordUserAction.record("Signin_MoreButton_Shown");
                        });
        mSigninView.getScrollView().setScrolledToBottomObserver(this::showButtonBar);
        mSigninView.getDetailsDescriptionView().setMovementMethod(LinkMovementMethod.getInstance());

        final Drawable endImageViewDrawable;
        if (mIsChild) {
            endImageViewDrawable = SigninView.getCheckmarkDrawable(getContext());
        } else {
            endImageViewDrawable = SigninView.getExpandArrowDrawable(getContext());
        }
        mSigninView.getAccountPickerEndImageView().setImageDrawable(endImageViewDrawable);

        setHasAccounts(true);
    }

    private WindowAndroid getWindowAndroid() {
        return getDelegate().getWindowAndroid();
    }

    @Nullable
    private Profile getProfile() {
        return getDelegate().getProfile();
    }

    /** Provides a {@link SyncConsentDelegate} for external dependencies. */
    protected abstract @NonNull SyncConsentDelegate getDelegate();

    @Override
    public void setView(View view) {
        mFrameLayout.removeAllViews();
        mFrameLayout.addView(view);
    }

    @Override
    public void onDeviceLockReady() {
        if (mDeviceLockCoordinator == null) {
            // `mDeviceLockPageCallback` should not be called more than once, even if
            // `OnDeviceLockReady` is invoked multiple times.
            return;
        }
        mDeviceLockCoordinator.destroy();
        mDeviceLockCoordinator = null;

        // Run the callback, or wait until #onResume if the fragment is not yet RESUMED.
        if (getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.RESUMED)) {
            mDeviceLockPageCallback.run();
        } else {
            mDeviceLockReady = true;
        }
    }

    @Override
    public void onDeviceLockRefused() {
        refuseSignIn();
    }

    @Override
    public @DeviceLockActivityLauncher.Source String getSource() {
        return DeviceLockActivityLauncher.Source.SYNC_CONSENT;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        boolean cancelable = !mIsChild;
        mCanUseGooglePlayServices =
                ExternalAuthUtils.getInstance()
                        .canUseGooglePlayServices(
                                new UserRecoverableErrorHandler.ModalDialog(
                                        requireActivity(), cancelable));
            mSigninView.getAcceptButton().setEnabled(mCanUseGooglePlayServices);
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onCoreAccountInfosChanged() {
        mAccountManagerFacade.getCoreAccountInfos().then(this::updateAccounts);
    }

    /** Implements {@link SigninManager.SignInStateObserver}. */
    @Override
    public void onSignedIn() {
        final CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        mIsSignedInWithoutSync =
                mSigninAccessPoint == SigninAccessPoint.START_PAGE && primaryAccount != null;
        if (mIsSignedInWithoutSync) {
            mSelectedAccountEmail = primaryAccount.getEmail();
            mAccountManagerFacade.getCoreAccountInfos().then(this::updateAccounts);
        }
    }

    /**
     * Account picker is hidden if there are no accounts on the device. Also, accept button becomes
     * "Add account" button in this case.
     */
    private void setHasAccounts(boolean hasAccounts) {
        if (hasAccounts) {
            final boolean hideAccountPicker =
                    mIsSignedInWithoutSync
                            || (mSigninAccessPoint == SigninAccessPoint.START_PAGE && mIsChild);
            mSigninView
                    .getAccountPickerView()
                    .setVisibility(hideAccountPicker ? View.GONE : View.VISIBLE);

            // The following calls register lambdas that will be executed on the current and every
            // recreated accept button.
            mSigninView.setAcceptConsentTextUpdater(this::setSigninAcceptConsent);
            mSigninView.setAcceptOnClickListener(this::onAcceptButtonClicked);
        } else {
            mSigninView.getAccountPickerView().setVisibility(View.GONE);

            // The following calls register lambdas that will be executed on the current and every
            // recreated accept button.
            mSigninView.setAcceptConsentTextUpdater(this::setSigninAddAccountConsent);
            mSigninView.setAcceptOnClickListener(this::onAddAccountButtonClicked);
        }

        // Show "Settings" link in description only if there are accounts on the device.
        updateSigninDetailsDescription(hasAccounts);
    }

    private void setSigninAcceptConsent(TextView textView) {
        mConsentTextTracker.setText(textView, R.string.signin_accept_button);
    }

    private void setSigninAddAccountConsent(TextView textView) {
        mConsentTextTracker.setText(textView, R.string.signin_add_account);
    }

    private void updateSigninDetailsDescription(boolean addSettingsLink) {
        final @Nullable Object settingsLinkSpan =
                addSettingsLink
                        ? new NoUnderlineClickableSpan(getContext(), this::onSettingsLinkClicked)
                        : null;
        final SpanApplier.SpanInfo spanInfo =
                new SpanApplier.SpanInfo(SETTINGS_LINK_OPEN, SETTINGS_LINK_CLOSE, settingsLinkSpan);
            mConsentTextTracker.setText(
                    mSigninView.getDetailsDescriptionView(),
                    R.string.signin_details_description,
                    input -> SpanApplier.applySpans(input.toString(), spanInfo));
    }

    /** Sets texts for immutable elements. Accept button text is set by {@link #setHasAccounts}. */
    private void updateConsentText() {
            final @StringRes int refuseButtonTextId =
                    mSigninAccessPoint == SigninAccessPoint.SIGNIN_PROMO
                                    || mSigninAccessPoint == SigninAccessPoint.START_PAGE
                            ? R.string.signin_sync_decline_button
                            : R.string.cancel;
            updateSigninViewText(refuseButtonTextId);
    }

    private void updateSigninViewText(@StringRes int refuseButtonTextId) {
        mConsentTextTracker.setText(mSigninView.getTitleView(), R.string.signin_title);

        mConsentTextTracker.setText(
                mSigninView.getSyncTitleView(),
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
                        ? R.string.signin_sync_title_without_passwords
                        : R.string.signin_sync_title);
        mConsentTextTracker.setText(
                mSigninView.getSyncDescriptionView(), R.string.signin_sync_description);

        mConsentTextTracker.setText(mSigninView.getRefuseButton(), refuseButtonTextId);
        mConsentTextTracker.setText(mSigninView.getMoreButton(), R.string.more);
    }

    private CharSequence getSigninViewAccountTextPrimary(
            DisplayableProfileData profileData, boolean canShowEmailAddress) {
        if (!TextUtils.isEmpty(profileData.getFullName())) {
            return profileData.getFullName();
        } else if (canShowEmailAddress) {
            // Full name is not available, show the email address if permitted.
            return profileData.getAccountEmail();
        }
        // Cannot show the email address and empty full name; use default account string.
        return getText(R.string.default_google_account_username);
    }

    private void updateProfileData(String accountEmail) {
        if (!TextUtils.equals(accountEmail, mSelectedAccountEmail)) {
            return;
        }
        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mSelectedAccountEmail);

        mSigninView.getAccountImageView().setImageDrawable(profileData.getImage());

        final boolean canShowEmailAddress = profileData.hasDisplayableEmailAddress();

        // The primary TextView is always visible.
        mConsentTextTracker.setTextNonRecordable(
                mSigninView.getAccountTextPrimary(),
                getSigninViewAccountTextPrimary(profileData, canShowEmailAddress));

        if (canShowEmailAddress) {
            // If the full name is available, the email will be in the secondary TextView.
            // Otherwise, the email is in the primary TextView; the secondary TextView hidden.
            final int secondaryTextVisibility =
                    TextUtils.isEmpty(profileData.getFullName()) ? View.GONE : View.VISIBLE;
            if (secondaryTextVisibility == View.VISIBLE) {
                mConsentTextTracker.setTextNonRecordable(
                        mSigninView.getAccountTextSecondary(), profileData.getAccountEmail());
            }
            mSigninView.getAccountTextSecondary().setVisibility(secondaryTextVisibility);
        } else {
            // If the email address cannot be shown, the primary TextView either displays the
            // full name or the default account string. The secondary TextView is hidden.
            mSigninView.getAccountTextSecondary().setVisibility(View.GONE);
        }

        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());

        // Promise may not yet be fulfilled.
        mAccountManagerFacade
                .getCoreAccountInfos()
                .then(
                        (List<CoreAccountInfo> accounts) -> {
                            CoreAccountInfo account =
                                    AccountUtils.findCoreAccountInfoByEmail(accounts, accountEmail);
                            if (account == null) {
                                return;
                            }

                            // Shows buttons hidden by createSigninView.
                            // MinorModeHelper.resolveMinorMode will either show the buttons
                            // immediately or after a short timeout during which the button
                            // configuration is retrieved.
                            MinorModeHelper.resolveMinorMode(
                                    identityManager,
                                    account,
                                    mSigninView::recreateSyncConsentButtons);
                        });
    }

    private void showButtonBar() {
            mSigninView.getButtonBar().setVisibility(View.VISIBLE);
            mSigninView.getMoreButton().setVisibility(View.GONE);
            mSigninView.getScrollView().setScrolledToBottomObserver(null);
    }

    private void onAccountPickerClicked() {
        if (mIsChild || !areControlsEnabled()) return;
        mAccountPickerDialogCoordinator =
                new AccountPickerDialogCoordinator(requireContext(), this, mModalDialogManager);
    }

    private void onRefuseButtonClicked(View button) {
        mSigninView.refuseButtonClicked();
        refuseSignIn();
    }

    private void refuseSignIn() {
        RecordUserAction.record("Signin_Undo_Signin");
        mRecordUndoSignin = false;
        onSyncRefused();
    }

    protected void onAcceptButtonClicked(View button) {
        if (!areControlsEnabled()) return;
        mIsSigninInProgress = true;
        mRecordUndoSignin = false;
        RecordUserAction.record("Signin_Signin_WithDefaultSyncSettings");
        seedAccountsAndSignin(false, button);
    }

    private void onAddAccountButtonClicked(View button) {
        if (!areControlsEnabled()) return;
        addAccount();
    }

    protected void onSettingsLinkClicked(View view) {
        mSigninView.settingsClicked();

        if (!areControlsEnabled()) return;
        mIsSigninInProgress = true;
        RecordUserAction.record("Signin_Signin_WithAdvancedSyncSettings");
        seedAccountsAndSignin(true, view);
    }

    /**
     * Whether account picker and accept button should react to clicks. This doesn't change the
     * visual appearance of these controls. Refuse button is always enabled.
     */
    private boolean areControlsEnabled() {
        // Ignore clicks if the fragment is being removed or the app is being backgrounded.
        if (!isResumed() || isStateSaved()) return false;
        return !mIsSigninInProgress && mCanUseGooglePlayServices;
    }

    private void seedAccountsAndSignin(boolean settingsClicked, View confirmationView) {
        // Getting the profile depends on the Activity, which may be gone by the time the callback
        // runs.
        final Profile profile = getProfile();
        AccountInfoServiceProvider.get()
                .getAccountInfoByEmail(mSelectedAccountEmail)
                .then(
                        accountInfo -> {
                            if (accountInfo == null) {
                                mIsSigninInProgress = false;
                                // If accountInfo is null, then the account may have been removed
                                // while sign-in is in progress. In this case update the UI with
                                // the updated account list.
                                mAccountManagerFacade
                                        .getCoreAccountInfos()
                                        .then(this::updateAccounts);
                                return;
                            }
                            mConsentTextTracker.recordConsent(
                                    profile,
                                    accountInfo.getId(),
                                    ConsentAuditorFeature.CHROME_SYNC,
                                    (TextView) confirmationView,
                                    mSigninView);
                            if (isResumed()) {
                                runStateMachineAndSignin(settingsClicked);
                            } else {
                                mIsSigninInProgress = false;
                            }
                        });
    }

    private void runStateMachineAndSignin(boolean settingsClicked) {
        var listener =
                new ConfirmSyncDataStateMachine.Listener() {
                    @Override
                    public void onConfirm(boolean wipeData, boolean acceptedAccountManagement) {
                        mConfirmSyncDataStateMachine = null;

                        // Don't start sign-in if this fragment has been destroyed.
                        if (getActivity().isDestroyed()) return;

                        SigninManager signinManager =
                                IdentityServicesProvider.get().getSigninManager(getProfile());
                        if (acceptedAccountManagement) {
                            signinManager.setUserAcceptedAccountManagement(true);
                        }

                        SigninManager.SignInCallback callback =
                                new SigninManager.SignInCallback() {
                                    @Override
                                    public void onSignInComplete() {
                                        mIsSigninInProgress = false;
                                    }

                                    @Override
                                    public void onSignInAborted() {
                                        if (acceptedAccountManagement) {
                                            signinManager.setUserAcceptedAccountManagement(false);
                                        }
                                        mIsSigninInProgress = false;
                                    }
                                };

                        signinManager.runAfterOperationInProgress(
                                () -> {
                                    if (wipeData) {
                                        signinManager.wipeSyncUserData(
                                                () -> {
                                                    onSyncAccepted(
                                                            mSelectedAccountEmail,
                                                            settingsClicked,
                                                            callback);
                                                },
                                                DataWipeOption.WIPE_SYNC_DATA);
                                    } else {
                                        onSyncAccepted(
                                                mSelectedAccountEmail, settingsClicked, callback);
                                    }
                                });
                    }

                    @Override
                    public void onCancel() {
                        mConfirmSyncDataStateMachine = null;
                        mIsSigninInProgress = false;
                    }
                };

        Profile profile = getProfile();
        mConfirmSyncDataStateMachine =
                new ConfirmSyncDataStateMachine(
                        profile,
                        new ConfirmSyncDataStateMachineDelegate(
                                requireContext(), profile, mModalDialogManager),
                        UserPrefs.get(profile)
                                .getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME),
                        mSelectedAccountEmail,
                        listener);
    }

    @Override
    public void onAccountSelected(String accountName) {
        selectAccount(accountName);
        mAccountPickerDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
        SigninMetricsUtils.logAddAccountStateHistogram(State.REQUESTED);
        mAccountManagerFacade.createAddAccountIntent(
                (@Nullable Intent intent) -> {
                    if (intent != null) {
                        SigninMetricsUtils.logAddAccountStateHistogram(State.STARTED);
                        startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                        return;
                    }

                    // AccountManagerFacade couldn't create intent, use SigninUtils to open settings
                    // instead.
                    SigninMetricsUtils.logAddAccountStateHistogram(State.FAILED);
                    SigninUtils.openSettingsForAllAccounts(getActivity());
                });
        // mAccountPickerDialogCoordinator could be null here as this method may be called without
        // showing the account picker.
        if (mAccountPickerDialogCoordinator != null) {
            mAccountPickerDialogCoordinator.dismissDialog();
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK && data != null) {
                SigninMetricsUtils.logAddAccountStateHistogram(State.SUCCEEDED);
                String addedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
                if (addedAccountName != null) {
                    mSelectedAccountEmail = addedAccountName;
                } else {
                    SigninMetricsUtils.logAddAccountStateHistogram(State.NULL_ACCOUNT_NAME);
                }
            } else {
                SigninMetricsUtils.logAddAccountStateHistogram(State.CANCELLED);
            }
            mAccountManagerFacade.getCoreAccountInfos().then(this::updateAccounts);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mAccountManagerFacade.addObserver(this);
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());

        final CoreAccountInfo primaryAccount =
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        mIsSignedInWithoutSync =
                mSigninAccessPoint == SigninAccessPoint.START_PAGE && primaryAccount != null;
        if (mIsSignedInWithoutSync) {
            mSelectedAccountEmail = primaryAccount.getEmail();
        }

        // When a fragment that was in the FragmentManager backstack becomes visible again, the view
        // will be recreated by onCreateView. Update the state of this recreated UI.
        if (mSelectedAccountEmail != null) {
            updateProfileData(mSelectedAccountEmail);
        } else {
            mSigninView.recreateAddAccountButtons();
        }

        updateAccounts(
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        mAccountManagerFacade.getCoreAccountInfos()));

        mSigninView.startAnimations();
        if (!mSyncStartedRecorded) {
            SigninMetricsUtils.logSyncConsentStarted(mSigninAccessPoint);
            SigninMetricsUtils.logSigninUserActionForAccessPoint(mSigninAccessPoint);
            mSyncStartedRecorded = true;
        }
        if (mDeviceLockReady) {
            mDeviceLockPageCallback.run();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        mAccountManagerFacade.removeObserver(this);

        mSigninView.stopAnimations();
    }

    private void selectAccount(String accountEmail) {
        mSelectedAccountEmail = accountEmail;
        updateProfileData(mSelectedAccountEmail);
    }

    protected void updateAccounts(List<CoreAccountInfo> coreAccountInfos) {
        if (!isResumed() || !mCanUseGooglePlayServices) {
                return;
            }

        if (coreAccountInfos.isEmpty()) {
            mSelectedAccountEmail = null;
            setHasAccounts(false);
            return;
        }
        setHasAccounts(true);
        final String defaultAccountEmail = coreAccountInfos.get(0).getEmail();
        if (mIsSignedInWithoutSync) {
            return;
        }

        if (mSelectedAccountEmail != null
                && AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, mSelectedAccountEmail)
                        != null) {
            selectAccount(mSelectedAccountEmail);
            return;
        }

        if (mConfirmSyncDataStateMachine != null) {
            // Any dialogs that may have been showing are now invalid (they were created
            // for the previously selected account).
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed= */ false);
            mConfirmSyncDataStateMachine = null;
        }

        if (mSelectedAccountEmail != null) {
            // Show account picker to user to confirm the account selection if
            // the original selected account is removed.
            mAccountPickerDialogCoordinator =
                    new AccountPickerDialogCoordinator(requireContext(), this, mModalDialogManager);
        }
        selectAccount(defaultAccountEmail);
    }
}
