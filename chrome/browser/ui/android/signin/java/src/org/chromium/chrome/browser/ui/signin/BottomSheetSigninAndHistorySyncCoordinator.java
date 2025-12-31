// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.annotation.ColorInt;

import org.chromium.base.Log;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Responsible of showing the correct sub-component of the sign-in and history opt-in flow. */
@NullMarked
public class BottomSheetSigninAndHistorySyncCoordinator extends SigninAndHistorySyncCoordinator
        implements SigninBottomSheetCoordinator.Delegate,
                HistorySyncCoordinator.HistorySyncDelegate,
                SigninSnackbarController.Listener {
    private static final String TAG = "BottomSheetSignin";
    private static final String ADD_ACCOUNT_ACTIVITY_KEY = "ADD_ACCOUNT_ACTIVITY_KEY";
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final ActivityResultTracker mActivityResultTracker;

    // TODO(https://crbug.com/469772349): Remove this interface once the migration is
    // completed, as this delegate is necessary only when the sign-in activity is used.
    private final @Nullable ActivityDelegate mActivityDelegate;
    private final Delegate mDelegate;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final OneshotSupplier<ProfileProvider> mProfileSupplier;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final @Nullable SnackbarManager mSnackbarManager;
    private BottomSheetSigninAndHistorySyncConfig mConfig;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private @Nullable Profile mProfile;

    private @Nullable SigninBottomSheetCoordinator mSigninBottomSheetCoordinator;
    private @Nullable HistorySyncCoordinator mHistorySyncCoordinator;
    private @Nullable PropertyModel mDialogModel;
    private boolean mDidShowSigninStep;
    private boolean mFlowInitialized;
    private @ColorInt int mScrimStatusBarColor = ScrimProperties.INVALID_COLOR;

    /**
     * This is a delegate that the sign-in activity needs to implement.
     *
     * <p>TODO(https://crbug.com/469772349): Remove this interface once the migration is completed,
     * as this delegate is necessary only when the sign-in activity is used.
     */
    @Deprecated
    public interface ActivityDelegate {
        /** Called when the user starts the Google Play Services "add account" flow. */
        void addAccount();

        /**
         * Returns whether the history sync modal dialog is shown in full screen mode instead of
         * dialog mode.
         */
        boolean isHistorySyncShownFullScreen();

        /** Called to change the status bar color. */
        void setStatusBarColor(int statusBarColor);
    }

    /** This is a delegate that the sign-in flow embedder needs to implement. */
    public interface Delegate {

        /** Called when the whole flow finishes. */
        void onFlowComplete(SigninAndHistorySyncCoordinator.Result result);
    }

    /**
     * Creates an instance of {@link BottomSheetSigninAndHistorySyncCoordinator} for the sign-in
     * bottom sheet based flow, hosted by the access point's activity.
     *
     * <p>This method **Does Not** start the sign-in UI flow, {@link #startSigninFlow} must be
     * called to do so.
     *
     * <p>This 2-step start design is due to the need of registering the add account result callback
     * early, even without the need ot starting a sign-in flow, to catch any in-flight add account
     * result received after the base activity is recreated (e.g Chrome killed due to memory
     * pressure).
     *
     * @param windowAndroid The window that hosts the sign-in & history opt-in flow.
     * @param activity The {@link Activity} that hosts the sign-in &opt-in flow.
     * @param activityResultTracker the {@link ActivityResultTracker} for launching new activities
     *     and watching for their result.
     * @param delegate The delegate for this coordinator.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param profileSupplier The supplier of the current profile.
     * @param bottomSheetController The controller of the sign-in bottomsheet.
     * @param modalDialogManagerSupplier The supplier of the {@link ModalDialogManager}
     * @param snackbarManager The manager for displaying snackbars at the bottom of the activity.
     * @param signinAccessPoint The entry point for the sign-in.
     */
    public static BottomSheetSigninAndHistorySyncCoordinator createAndObserveAddAccountResult(
            WindowAndroid windowAndroid,
            Activity activity,
            ActivityResultTracker activityResultTracker,
            BottomSheetSigninAndHistorySyncCoordinator.Delegate delegate,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            OneshotSupplier<ProfileProvider> profileSupplier,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager,
            @SigninAccessPoint int signinAccessPoint) {
        assert SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
        return new BottomSheetSigninAndHistorySyncCoordinator(
                windowAndroid,
                activity,
                activityResultTracker,
                delegate,
                deviceLockActivityLauncher,
                profileSupplier,
                bottomSheetController,
                modalDialogManagerSupplier,
                snackbarManager,
                signinAccessPoint);
    }

    private BottomSheetSigninAndHistorySyncCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            ActivityResultTracker activityResultTracker,
            Delegate delegate,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            OneshotSupplier<ProfileProvider> profileSupplier,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mActivityResultTracker = activityResultTracker;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mProfileSupplier = profileSupplier;
        mBottomSheetController = bottomSheetController;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSnackbarManager = snackbarManager;
        mSigninAccessPoint = signinAccessPoint;
        mActivityDelegate = null;

        activityResultTracker.register(
                ADD_ACCOUNT_ACTIVITY_KEY,
                new ActivityResultCallback<ActivityResult>() {
                    @Override
                    public void onActivityResult(ActivityResult result) {
                        onAddAccountResult(result.getResultCode(), result.getData());
                    }
                });

        // TODO(crbug.com/41493768): Implement the loading state UI.
    }

    /**
     * Creates an instance of {@link BottomSheetSigninAndHistorySyncCoordinator} and shows the
     * sign-in bottom sheet based flow, hosted by the (@link SigninAndHistorySyncActivity).
     *
     * <p>This method **Does** start the sign-in UI flow.
     *
     * @param windowAndroid The window that hosts the sign-in & history opt-in flow.
     * @param activity The {@link Activity} that hosts the sign-in &opt-in flow.
     * @param activityResultTracker the {@link ActivityResultTracker} for launching new activities
     *     and watching for their result.
     * @param activityDelegate The delegate for this coordinator implemented for the activity-based
     *     sign-in flow.
     * @param delegate The delegate for this coordinator.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param profileSupplier The supplier of the current profile.
     * @param bottomSheetController The controller of the sign-in bottomsheet.
     * @param modalDialogManagerSupplier The supplier of the {@link ModalDialogManager}
     * @param config The configuration for the bottom sheet.
     * @param signinAccessPoint The entry point for the sign-in.
     * @deprecated Use the other constructor instead. This will be removed in
     *     https://crbug.com/469772349.
     */
    @Deprecated
    public BottomSheetSigninAndHistorySyncCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            ActivityResultTracker activityResultTracker,
            ActivityDelegate activityDelegate,
            Delegate delegate,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            OneshotSupplier<ProfileProvider> profileSupplier,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            BottomSheetSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mActivityResultTracker = activityResultTracker;
        mActivityDelegate = activityDelegate;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mProfileSupplier = profileSupplier;
        mBottomSheetController = bottomSheetController;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSigninAccessPoint = signinAccessPoint;
        mConfig = config;
        mSnackbarManager = null;
        mProfileSupplier.onAvailable(this::onProfileAvailable);

        // TODO(crbug.com/41493768): Implement the loading state UI.
    }

    /**
     * Starts the sign-in and history sync UI flow.
     *
     * <p>This method should be called when the user takes an action to begin the sign-in flow
     * (e.g., tapping a "sign-in" button). While the coordinator constructor should be called early
     * (e.g. as soon as the access point's UI is ready) to handle prerequisites like registering the
     * "add account" activity callback or initialize the coordinator internals, this method is what
     * launches the flow with the given configuration.
     *
     * @param config The configuration for the bottom sheet.
     */
    @Initializer
    public void startSigninFlow(BottomSheetSigninAndHistorySyncConfig config) {
        // TODO(https://crbug.com/437039516): trigger error UI when sign-in can't be
        // launched from the sign-in coordinator.
        assert SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);

        mConfig = config;
        mProfileSupplier.onAvailable(this::onProfileAvailable);
    }

    /**
     * Implements {@link SigninAndHistorySyncCoordinator}. Call the child coordinators' `destroy`
     * method to release resources, should be called when the hosting activity is destroyed.
     */
    @Override
    public void destroy() {
        if (mSigninBottomSheetCoordinator != null) {
            mSigninBottomSheetCoordinator.destroy();
            mSigninBottomSheetCoordinator = null;
        }

        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
    }

    /**
     * Implements {@link SigninAndHistorySyncCoordinator}. Called when an Google Play Services "add
     * account" flow started at the activity level has finished without being completed.
     */
    @Override
    public void onAddAccountCanceled() {
        // If the activity was killed during the add account flow (reason why the flow is not yet
        // initialized), proceed as if the user started the sign-in flow for the first time.
    }

    /**
     * Implements {@link SigninAndHistorySyncCoordinator}. Called when an account is added via
     * Google Play Services "add account" flow started at the activity level.
     */
    @Override
    public void onAccountAdded(String accountEmail) {
        // If the activity was killed during the add account flow (reason why the flow is not yet
        // initialized), proceed as if the user started the sign-in flow for the first time.
        if (!mFlowInitialized) {
            // TODO(crbug.com/41493767): Select added account or sign in once done loading.
        }

        if (mSigninBottomSheetCoordinator != null) {
            mSigninBottomSheetCoordinator.onAccountAdded(accountEmail);
        }
    }

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
    public void onConfigurationChange() {
        if (mHistorySyncCoordinator != null) {
            assert mProfile != null;
            mHistorySyncCoordinator.maybeRecreateView();
            showDialogContentView();
        }
    }

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
    public @BackPressResult int handleBackPress() {
        return BackPressResult.UNKNOWN;
    }

    /** Implements {@link SigninBottomSheetCoordinator.Delegate}. */
    @Override
    public void addAccount() {
        if (mActivityDelegate == null) {
            assert SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
            SigninMetricsUtils.logAddAccountStateHistogram(State.REQUESTED);
            AccountManagerFacadeProvider.getInstance()
                    .createAddAccountIntent(
                            null,
                            intent -> {
                                if (intent == null) {
                                    // AccountManagerFacade couldn't create the intent, use
                                    // SigninUtils to open settings instead.
                                    SigninMetricsUtils.logAddAccountStateHistogram(State.FAILED);
                                    SigninUtils.openSettingsForAllAccounts(mActivity);
                                    return;
                                }
                                SigninMetricsUtils.logAddAccountStateHistogram(State.STARTED);
                                // TODO(https://crbug.com/437039516): Save the config in instance
                                // state via ActivityResultTracker.
                                mActivityResultTracker.startActivity(
                                        ADD_ACCOUNT_ACTIVITY_KEY, intent);
                            });
        } else {
            mActivityDelegate.addAccount();
        }
    }

    /** Implements {@link SigninBottomSheetCoordinator.Delegate}. */
    @Override
    public void onSignInComplete() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
                && mSigninAccessPoint == SigninAccessPoint.BOOKMARK_MANAGER) {
            SyncService syncService =
                    assumeNonNull(SyncServiceFactory.getForProfile(assertNonNull(mProfile)));
            syncService.setSelectedType(UserSelectableType.BOOKMARKS, true);
            syncService.setSelectedType(UserSelectableType.READING_LIST, true);
        }

        if (mSigninBottomSheetCoordinator == null) {
            return;
        }

        mSigninBottomSheetCoordinator.destroy();
        mSigninBottomSheetCoordinator = null;
        maybeShowHistoryOptInDialog();
    }

    /** Implements {@link SigninBottomSheetCoordinator.Delegate}. */
    @Override
    public void onSignInCancel() {
        if (mSigninBottomSheetCoordinator == null) {
            return;
        }

        mSigninBottomSheetCoordinator.destroy();
        mSigninBottomSheetCoordinator = null;
        onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());
    }

    /** Implements {@link SigninSnackbarController.Listener} */
    @Override
    public void onUndoSignin() {
        Log.d(TAG, "onUndoSignin");
        // TODO(crbug.com/437039311): Permanently dismiss the promo for non-recent-tabs promos
    }

    /** Implements {@link SigninBottomSheetCoordinator.Delegate}. */
    @Override
    public void setStatusBarColor(@ColorInt int color) {
        if (mActivityDelegate == null) {
            return;
        }
        // INVALID_COLOR is set at the start and end of the bottom sheet scrim fade out animation.
        // After the scrim fades out, the status bar background needs to be reset to match the
        // history sync full screen dialog if it's appearing next. In case the history sync dialog
        // is skipped, the activity will finish and the status bar color change is not shown to the
        // user.
        if (color != ScrimProperties.INVALID_COLOR) {
            mActivityDelegate.setStatusBarColor(color);
        } else if (mDialogModel != null && mScrimStatusBarColor != ScrimProperties.INVALID_COLOR) {
            updateStatusBarColorForHistorySync();
        }
        mScrimStatusBarColor = color;
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void dismissHistorySync(boolean didSignOut, boolean isHistorySyncAccepted) {
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
        SigninAndHistorySyncCoordinator.Result flowResult =
                new SigninAndHistorySyncCoordinator.Result(
                        mDidShowSigninStep && !didSignOut, isHistorySyncAccepted);
        onFlowComplete(flowResult);
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void recordHistorySyncOptIn(int accessPoint, boolean isHistorySyncAccepted) {
        if (isHistorySyncAccepted) {
            SigninMetricsUtils.logHistorySyncAcceptButtonClicked(accessPoint);
        } else {
            SigninMetricsUtils.logHistorySyncDeclineButtonClicked(accessPoint);
        }
    }

    private void onProfileAvailable(ProfileProvider profileProvider) {
        mProfile = assumeNonNull(profileProvider.getOriginalProfile());
        AccountManagerFacadeProvider.getInstance()
                .getAccounts()
                .then(
                        accounts -> {
                            finishLoadingAndSelectSigninFlow(accounts);
                        });
    }

    private void finishLoadingAndSelectSigninFlow(List<AccountInfo> accounts) {
        // The history opt-in screen should be shown after the coreAccountInfos
        // become available to avoid showing additional loading UI after history
        // opt-in screen is shown.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(assertNonNull(mProfile));
        assumeNonNull(identityManager);
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            maybeShowHistoryOptInDialog();
            return;
        }

        if (!accounts.isEmpty()) {
            showSigninBottomSheet();
            SigninMetricsUtils.logSigninStarted(mSigninAccessPoint);
            return;
        }

        switch (mConfig.noAccountSigninMode) {
            case NoAccountSigninMode.BOTTOM_SHEET:
                showSigninBottomSheet();
                SigninMetricsUtils.logSigninStarted(mSigninAccessPoint);
                break;
            case NoAccountSigninMode.NO_SIGNIN:
                // TODO(crbug.com/41493768): Implement the error state UI.
                onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());
                break;
        }
    }

    private void showSigninBottomSheet() {
        SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(assertNonNull(mProfile));
        assumeNonNull(signinManager);
        @AccountPickerLaunchMode int accountPickerMode = AccountPickerLaunchMode.DEFAULT;
        switch (mConfig.withAccountSigninMode) {
            case WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET:
                accountPickerMode = AccountPickerLaunchMode.DEFAULT;
                break;
            case WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET:
                accountPickerMode = AccountPickerLaunchMode.CHOOSE_ACCOUNT;
                break;
        }

        mSigninBottomSheetCoordinator =
                new SigninBottomSheetCoordinator(
                        mWindowAndroid,
                        mActivity,
                        this,
                        mBottomSheetController,
                        mDeviceLockActivityLauncher,
                        signinManager,
                        mConfig.bottomSheetStrings,
                        accountPickerMode,
                        mConfig.withAccountSigninMode == WithAccountSigninMode.SEAMLESS_SIGNIN,
                        mSigninAccessPoint,
                        mConfig.selectedCoreAccountId);
        mDidShowSigninStep = true;
    }

    private void maybeShowHistoryOptInDialog() {
        assert mProfile != null;
        if (!SigninAndHistorySyncCoordinator.shouldShowHistorySync(
                mProfile, mConfig.historyOptInMode)) {
            HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(mProfile);
            historySyncHelper.recordHistorySyncNotShown(mSigninAccessPoint);
            // TODO(crbug.com/376469696): Differentiate the failure & completion case here.
            onFlowComplete(new SigninAndHistorySyncCoordinator.Result(mDidShowSigninStep, false));
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
                                            dismissHistorySync(
                                                    /* didSignOut= */ false,
                                                    /* isHistorySyncAccepted= */ false);
                                        } else {
                                            // TODO(crbug.com/453930445): onFlowComplete can be
                                            // called twice, hide behind seamless sign-in flag
                                            onFlowComplete(
                                                    SigninAndHistorySyncCoordinator.Result
                                                            .aborted());
                                        }
                                    }
                                })
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                // TODO(crbug.com/453930445): remove entire handleOnBackPressed
                                // block, back pressing by default dismisses the dialog
                                new OnBackPressedCallback(true) {
                                    @Override
                                    public void handleOnBackPressed() {
                                        if (mHistorySyncCoordinator != null) {
                                            dismissHistorySync(
                                                    /* didSignOut= */ false,
                                                    /* isHistorySyncAccepted= */ false);
                                        } else {
                                            onFlowComplete(
                                                    SigninAndHistorySyncCoordinator.Result
                                                            .aborted());
                                        }
                                    }
                                })
                        .build();

        createHistorySyncCoordinator(mProfile);
        showDialogContentView();

        // Updating the status bar color for the history sync view in case animations are disabled
        // and the dialog model is created after the scrim animation finishes.
        if (mScrimStatusBarColor == ScrimProperties.INVALID_COLOR) {
            updateStatusBarColorForHistorySync();
        }
    }

    private void createHistorySyncCoordinator(Profile profile) {
        assert mHistorySyncCoordinator == null;
        boolean shouldSignOutOnDecline =
                mDidShowSigninStep
                        && mConfig.historyOptInMode == HistorySyncConfig.OptInMode.REQUIRED;
        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        mActivity,
                        this,
                        profile,
                        mConfig.historySyncConfig,
                        mSigninAccessPoint,
                        /* showEmailInFooter= */ !mDidShowSigninStep,
                        shouldSignOutOnDecline,
                        null);
        assert mDialogModel != null;
        mHistorySyncCoordinator.maybeRecreateView();
    }

    void showDialogContentView() {
        assumeNonNull(mHistorySyncCoordinator);
        View view = mHistorySyncCoordinator.getView();
        assumeNonNull(view);
        view.setBackgroundColor(getHistorySyncBackgroundColor());
        assumeNonNull(mDialogModel);
        mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, view);
        ModalDialogManager manager = mModalDialogManagerSupplier.get();
        assert manager != null;
        manager.showDialog(
                mDialogModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    private void onFlowComplete(SigninAndHistorySyncCoordinator.Result result) {
        if (mConfig.shouldShowSigninSnackbar) {
            SigninSnackbarController.showUndoSnackbarIfNeeded(
                    mActivity,
                    assertNonNull(mProfile),
                    mSigninAccessPoint,
                    mSnackbarManager,
                    this,
                    result);
        }
        // TODO(https://crbug.com/437039516): Dismiss history sync dialog.
        mDelegate.onFlowComplete(result);
    }

    private @ColorInt int getHistorySyncBackgroundColor() {
        return SemanticColorUtils.getDefaultBgColor(mActivity);
    }

    private void updateStatusBarColorForHistorySync() {
        if (mActivityDelegate == null) {
            return;
        }
        if (mActivityDelegate.isHistorySyncShownFullScreen()) {
            mActivityDelegate.setStatusBarColor(getHistorySyncBackgroundColor());
        }
    }
}
