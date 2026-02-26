// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.function.Supplier;

/** Coordinator for the signin promo card. */
@NullMarked
public class SigninPromoCoordinator
        implements BottomSheetSigninAndHistorySyncCoordinator.Delegate,
                SigninPromoMediator.Delegate {
    private static boolean sPromoDisabledForTesting;
    private final Context mContext;
    private final SigninPromoDelegate mDelegate;
    private final SigninPromoMediator mMediator;
    private @Nullable ImpressionTracker mImpressionTracker;
    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    // TODO(https://crbug.com/469778109): Remove nullability after launching Seamless sign-in.
    protected @Nullable BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;

    /** Disables promo in tests. */
    public static void disablePromoForTesting() {
        sPromoDisabledForTesting = true;
        ResettersForTesting.register(() -> sPromoDisabledForTesting = false);
    }

    /**
     * Creates an instance of the {@link SigninPromoCoordinator}.
     *
     * @param context The Android {@link Context}.
     * @param profile A {@link Profile} object to access identity services. This must be the
     *     original profile, not the incognito one.
     * @param delegate A {@link SigninPromoDelegate} to customize the view.
     */
    // TODO(https://crbug.com/448227402): Remove the constructor below.
    @Deprecated
    public SigninPromoCoordinator(Context context, Profile profile, SigninPromoDelegate delegate) {
        mContext = context;
        mDelegate = delegate;
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(identityManager);
        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext, identityManager);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assumeNonNull(syncService);
        mMediator =
                new SigninPromoMediator(
                        identityManager,
                        syncService,
                        AccountManagerFacadeProvider.getInstance(),
                        profileDataCache,
                        delegate,
                        this);
    }

    /**
     * Creates an instance of the {@link SigninPromoCoordinator}.
     *
     * @param windowAndroid The window showing this promo.
     * @param activity The Android Activity showing this promo.
     * @param profile A {@link Profile} object to access identity services. This must be the
     *     original profile, not the incognito one.
     * @param activityResultTracker Tracker of activity results.
     * @param launcher Launcher of the sign-in flow.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param snackbarManager Manages snackbars shown in the app.
     * @param deviceLockActivityLauncher Launcher of the Device Lock Activity.
     * @param delegate A {@link SigninPromoDelegate} to customize the view.
     */
    public SigninPromoCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            Profile profile,
            ActivityResultTracker activityResultTracker,
            SigninAndHistorySyncActivityLauncher launcher,
            Supplier<BottomSheetController> bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            SigninPromoDelegate delegate) {
        this(activity, profile, delegate);

        if (!SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            return;
        }

        OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
        profileSupplier.set(profile);
        mSigninCoordinator =
                launcher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                        windowAndroid,
                        activity,
                        activityResultTracker,
                        this,
                        deviceLockActivityLauncher,
                        profileSupplier,
                        bottomSheetController,
                        modalDialogManagerSupplier,
                        snackbarManager,
                        mDelegate.getAccessPoint());
    }

    public void destroy() {
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
        }
        mMediator.destroy();
        destroyPropertyModelChangeProcessor();
    }

    /** Determines whether the signin promo can be shown. */
    public boolean canShowPromo() {
        return !sPromoDisabledForTesting && mMediator.canShowPromo();
    }

    /** Builds a promo view object for the corresponding access point. */
    public View buildPromoView(@Nullable ViewGroup parent) {
        return LayoutInflater.from(mContext)
                .inflate(getLayoutResId(mDelegate.getAccessPoint()), parent, false);
    }

    /** Sets the view that is controlled by this coordinator. */
    public void setView(View view) {
        PersonalizedSigninPromoView promoView = view.findViewById(R.id.signin_promo_view_container);
        if (promoView == null) {
            throw new IllegalArgumentException("Promo view doesn't exist in container");
        }
        destroyPropertyModelChangeProcessor();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), promoView, SigninPromoViewBinder::bind);
        mImpressionTracker = new ImpressionTracker(promoView);
        mImpressionTracker.setListener(mMediator::recordImpression);
    }

    void setLoadingStateForTesting(boolean shouldShowLoadingState) {
        if (shouldShowLoadingState) {
            mMediator.onFlowStarted();
        } else {
            mMediator.onFlowCompleted();
        }
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void onFlowComplete(SigninAndHistorySyncCoordinator.Result result) {
        mMediator.onFlowCompleted();
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate} */
    @Override
    public void onSigninUndone() {
        assert SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
        mMediator.onSigninUndone();
    }

    /** Implements {@link SigninPromoMediator.Delegate} */
    @Override
    public void startSigninFlow(BottomSheetSigninAndHistorySyncConfig config) {
        assert (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
                && mSigninCoordinator != null);
        mMediator.onFlowStarted();
        mSigninCoordinator.startSigninFlow(config);
    }

    private void destroyPropertyModelChangeProcessor() {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
            // Always initialized and destroyed with mPropertyModelChangeProcessor.
            assumeNonNull(mImpressionTracker);
            mImpressionTracker.setListener(null);
            mImpressionTracker = null;
        }
    }

    static int getLayoutResId(@SigninAccessPoint int accessPoint) {
        return switch (accessPoint) {
            case SigninAccessPoint.BOOKMARK_MANAGER -> R.layout.sync_promo_view_bookmarks;
            case SigninAccessPoint.HISTORY_PAGE -> R.layout.sync_promo_view_history_page;
            case SigninAccessPoint.NTP_FEED_TOP_PROMO ->
                    R.layout.sync_promo_view_content_suggestions;
            case SigninAccessPoint.RECENT_TABS -> R.layout.sync_promo_view_recent_tabs;
            default -> throw new IllegalArgumentException(
                    "Invalid sign-in promo access point: " + accessPoint);
        };
    }
}
