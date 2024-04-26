// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.activity.ComponentActivity;
import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Responsible of showing the correct sub-component of the sign-in and history opt-in flow. */
public class SigninAndHistoryOptInCoordinator
        implements SigninAccountPickerCoordinator.Delegate,
                HistorySyncCoordinator.HistorySyncDelegate {
    private final WindowAndroid mWindowAndroid;
    private final ComponentActivity mActivity;
    private final ViewGroup mContainerView;

    private final Delegate mDelegate;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final OneshotSupplier<Profile> mProfileSupplier;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    private final AccountPickerBottomSheetStrings mBottomSheetStrings;
    private final @NoAccountSigninMode int mNoAccountSigninMode;
    private final @WithAccountSigninMode int mWithAccountSigninMode;
    private final @HistoryOptInMode int mHistoryOptInMode;

    private SigninAccountPickerCoordinator mAccountPickerCoordinator;
    private HistorySyncCoordinator mHistorySyncCoordinator;
    private PropertyModel mDialogModel;
    private boolean mDidShowSigninStep;
    private boolean mIsHistorySyncDedicatedFlow;

    /** This is a delegate that the embedder needs to implement. */
    public interface Delegate {
        /** Called when the user starts the Google Play Services "add account" flow. */
        void addAccount();

        /** Called when the whole flow finishes. */
        void onFlowComplete();
    }

    /** The sign-in step that should be shown to the user when there's no account on the device. */
    @IntDef({
        NoAccountSigninMode.BOTTOM_SHEET,
        NoAccountSigninMode.ADD_ACCOUNT,
        NoAccountSigninMode.NO_SIGNIN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NoAccountSigninMode {
        /** Show the 0-account version of the sign-in bottom sheet. */
        int BOTTOM_SHEET = 0;

        /** Bring the user to GMS Core to add an account, then sign-in with the new account. */
        int ADD_ACCOUNT = 1;

        /** No sign-in should be done, the entry point should not be visible to the user. */
        int NO_SIGNIN = 2;
    }

    /** The sign-in step that should be shown to the user when there's 1+ accounts on the device. */
    @IntDef({
        WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
        WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WithAccountSigninMode {
        /** Show the "collapsed" sign-in bottom sheet containing the default account. */
        int DEFAULT_ACCOUNT_BOTTOM_SHEET = 0;

        /** Show the "expanded" sign-in bottom sheet containing the accounts list. */
        int CHOOSE_ACCOUNT_BOTTOM_SHEET = 1;
    }

    /** The visibility rule to apply to the history opt-in step. */
    @IntDef({HistoryOptInMode.NONE, HistoryOptInMode.OPTIONAL, HistoryOptInMode.REQUIRED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HistoryOptInMode {
        /** Never show the history sync opt-in. */
        int NONE = 0;

        /** The history sync opt-in can be skipped (e.g. if the user declined too recently). */
        int OPTIONAL = 1;

        /** The history sync opt-in should always be shown. */
        int REQUIRED = 2;
    }

    public static boolean willShowSigninUI(Profile profile) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        return signinManager.isSigninAllowed();
    }

    public static boolean willShowHistorySyncUI(
            Profile profile, @HistoryOptInMode int historyOptInMode) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (!willShowSigninUI(profile) && !identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // Signin is suppressed because of something other than the user being signed in. Since
            // the user cannot sign in, we should not show history sync either.
            return false;
        }
        return switch (historyOptInMode) {
            case HistoryOptInMode.NONE -> false;
                // TODO(crbug.com/331568233): Show history opt-in only if it's not suppressed.
            case HistoryOptInMode.OPTIONAL, HistoryOptInMode.REQUIRED -> shouldShowHistorySync(
                    profile);
            default -> throw new IllegalArgumentException(
                    "Unexpected value for historyOptInMode :" + historyOptInMode);
        };
    }

    /**
     * Creates an instance of {@link SigninAndHistoryOptInCoordinator} and shows the sign-in bottom
     * sheet.
     *
     * @param windowAndroid The window that hosts the sign-in & history opt-in flow.
     * @param activity The {@link Activity} that hosts the sign-in &opt-in flow.
     * @param delegate The delegate for this coordinator.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param profileSupplier The supplier of the current profile.
     * @param modalDialogManagerSupplier The supplier of the {@link ModalDialogManager}
     * @param signinAccessPoint The entry point for the sign-in.
     * @param isHistorySyncDedicatedFlow Whether the flow is dedicated to enabling history sync
     *     (recent tabs for example).
     */
    public SigninAndHistoryOptInCoordinator(
            @NonNull WindowAndroid windowAndroid,
            @NonNull ComponentActivity activity,
            @NonNull Delegate delegate,
            @NonNull DeviceLockActivityLauncher deviceLockActivityLauncher,
            @NonNull OneshotSupplier<Profile> profileSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistoryOptInMode int historyOptInMode,
            @SigninAccessPoint int signinAccessPoint,
            boolean isHistorySyncDedicatedFlow) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.onAvailable(this::onProfileAvailable);
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mBottomSheetStrings = bottomSheetStrings;
        mNoAccountSigninMode = noAccountSigninMode;
        mWithAccountSigninMode = withAccountSigninMode;
        mHistoryOptInMode = historyOptInMode;
        mSigninAccessPoint = signinAccessPoint;
        mIsHistorySyncDedicatedFlow = isHistorySyncDedicatedFlow;
        mContainerView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.signin_history_opt_in_container, null);

        // TODO(crbug.com/41493768): Implement the loading state UI.
    }

    /**
     * Called when an account is added via Google Play Services "add account" flow started at the
     * activity level.
     */
    public void onAccountAdded(@NonNull String accountEmail) {
        showSigninBottomSheet();
        mAccountPickerCoordinator.onAccountAdded(accountEmail);
    }

    /** Implements {@link SigninAccountPickerCoordinator.Delegate}. */
    @Override
    public void addAccount() {
        mDelegate.addAccount();
    }

    /** Implements {@link SigninAccountPickerCoordinator.Delegate}. */
    @Override
    public void onSignInComplete() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                && SyncFeatureMap.isEnabled(
                        SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
                && mSigninAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            Profile profile = mProfileSupplier.get();
            SyncService syncService = SyncServiceFactory.getForProfile(profile);
            syncService.setSelectedType(UserSelectableType.BOOKMARKS, true);
            syncService.setSelectedType(UserSelectableType.READING_LIST, true);
        }

        if (mAccountPickerCoordinator == null) {
            return;
        }

        mAccountPickerCoordinator.destroy();
        mAccountPickerCoordinator = null;
        showHistoryOptInOrFinish();
    }

    /** Implements {@link SigninAccountPickerCoordinator.Delegate}. */
    @Override
    public void onSignInCancel() {
        if (mAccountPickerCoordinator == null) {
            return;
        }

        mAccountPickerCoordinator.destroy();
        mAccountPickerCoordinator = null;
        onFlowComplete();
    }

    /** Provides the root view of the sign-in and history opt-in flow. */
    public @NonNull ViewGroup getView() {
        assert mContainerView != null;
        return mContainerView;
    }

    /**
     * Call the child coordinators' `destroy` method to release resources, should be called when the
     * hosting activity is destroyed.
     */
    public void destroy() {
        if (mAccountPickerCoordinator != null) {
            mAccountPickerCoordinator.destroy();
            mAccountPickerCoordinator = null;
        }

        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
    }

    public void switchHistorySyncLayout() {
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            Profile profile = mProfileSupplier.get();
            assert profile != null;
            showDialogContentView(profile);
        }
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void dismissHistorySync() {
        mHistorySyncCoordinator.destroy();
        mHistorySyncCoordinator = null;
        onFlowComplete();
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public boolean isLargeScreen() {
        Configuration configuration = mActivity.getResources().getConfiguration();
        return configuration.isLayoutSizeAtLeast(Configuration.SCREENLAYOUT_SIZE_LARGE);
    }

    private void onProfileAvailable(Profile profile) {
        if (profile.isOffTheRecord()) {
            throw new IllegalStateException(
                    "Sign-in & history opt-in flow is using incognito profile");
        }

        AccountManagerFacadeProvider.getInstance()
                .getCoreAccountInfos()
                .then(
                        coreAccountInfos -> {
                            // The history opt-in screen should be shown after the coreAccountInfos
                            // become available to avoid showing additional loading UI after history
                            // opt-in screen is shown.
                            IdentityManager identityManager =
                                    IdentityServicesProvider.get()
                                            .getIdentityManager(mProfileSupplier.get());
                            if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                                showHistoryOptInOrFinish();
                                return;
                            }

                            if (!coreAccountInfos.isEmpty()) {
                                showSigninBottomSheet();
                                return;
                            }

                            switch (mNoAccountSigninMode) {
                                case NoAccountSigninMode.BOTTOM_SHEET:
                                    showSigninBottomSheet();
                                    break;
                                case NoAccountSigninMode.ADD_ACCOUNT:
                                    addAccount();
                                    mDidShowSigninStep = true;
                                    break;
                                case NoAccountSigninMode.NO_SIGNIN:
                                    // TODO(crbug.com/41493768): Implement the error state UI.
                                    onFlowComplete();
                                    break;
                            }
                        });
    }

    private void showSigninBottomSheet() {
        SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(mProfileSupplier.get());
        @AccountPickerLaunchMode int accountPickerMode = AccountPickerLaunchMode.DEFAULT;
        switch (mWithAccountSigninMode) {
            case WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET:
                accountPickerMode = AccountPickerLaunchMode.DEFAULT;
                break;
            case WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET:
                accountPickerMode = AccountPickerLaunchMode.CHOOSE_ACCOUNT;
                break;
        }
        mAccountPickerCoordinator =
                new SigninAccountPickerCoordinator(
                        mWindowAndroid,
                        mActivity,
                        mContainerView,
                        this,
                        mDeviceLockActivityLauncher,
                        signinManager,
                        mBottomSheetStrings,
                        accountPickerMode,
                        mSigninAccessPoint);
        mDidShowSigninStep = true;
    }

    private void showHistoryOptInOrFinish() {
        switch (mHistoryOptInMode) {
            case HistoryOptInMode.NONE:
                onFlowComplete();
                break;
            case HistoryOptInMode.OPTIONAL:
                // TODO(crbug.com/40944119): Show history opt-in only if it's not suppressed.
            case HistoryOptInMode.REQUIRED:
                maybeShowHistoryOptInDialog();
                break;
        }
    }

    private void maybeShowHistoryOptInDialog() {
        Profile profile = mProfileSupplier.get();
        assert profile != null;
        if (!shouldShowHistorySync(profile)) {
            HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
            historySyncHelper.recordHistorySyncNotShown(mSigninAccessPoint);
            onFlowComplete();
            return;
        }
        ModalDialogManager manager = mModalDialogManagerSupplier.get();
        assert manager != null;

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new ModalDialogProperties.Controller() {
                                    // Button interactions are implemented as HistorySyncProperties.
                                    @Override
                                    public void onClick(
                                            PropertyModel model, @ButtonType int buttonType) {}

                                    @Override
                                    public void onDismiss(
                                            PropertyModel model,
                                            @DialogDismissalCause int dismissalCause) {
                                        if (mHistorySyncCoordinator != null) {
                                            dismissHistorySync();
                                        } else {
                                            onFlowComplete();
                                        }
                                    }
                                })
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                new OnBackPressedCallback(true) {
                                    @Override
                                    public void handleOnBackPressed() {
                                        if (mHistorySyncCoordinator != null) {
                                            dismissHistorySync();
                                        } else {
                                            onFlowComplete();
                                        }
                                    }
                                })
                        .build();

        showDialogContentView(profile);
    }

    private void showDialogContentView(Profile profile) {
        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        mActivity,
                        this,
                        profile,
                        mSigninAccessPoint,
                        mDidShowSigninStep,
                        mDidShowSigninStep && mIsHistorySyncDedicatedFlow,
                        null);
        assert mDialogModel != null;
        mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, mHistorySyncCoordinator.getView());
        ModalDialogManager manager = mModalDialogManagerSupplier.get();
        assert manager != null;
        manager.showDialog(
                mDialogModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    private static boolean shouldShowHistorySync(Profile profile) {
        HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
        return !historySyncHelper.shouldSuppressHistorySync();
    }

    private void onFlowComplete() {
        mDelegate.onFlowComplete();
    }
}
