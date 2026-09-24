// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the enterprise signals disclaimer bottom sheet. The disclaimer is shown on
 * startup and on primary account change for managed enterprise users who have not acknowledged the
 * disclaimer previously.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerCoordinator
        implements EnterpriseSignalsDisclaimerMediator.Delegate, View.OnAttachStateChangeListener {
    /** Delegate for the enterprise signals disclaimer. */
    public interface Delegate {
        /**
         * Opens the info page for the given URL.
         *
         * @param url The URL of the webpage to show.
         */
        void showInfoPage(String url);
    }

    private static final long UNSET_TIME = -1;

    private final EnterpriseSignalsDisclaimerMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final EnterpriseSignalsDisclaimerHost mDisclaimerHost;
    private final Delegate mDelegate;
    private final MetricsHelper mMetricsHelper;
    private final EnterpriseSignalsDisclaimerView mView;
    private boolean mIsDestroyed;
    private @Nullable Runnable mOnDestroyCallback;
    private long mShownAtUptimeMillis = UNSET_TIME;
    private @Nullable @MetricsHelper.ShownOn Integer mShownOn;
    private @Nullable Callback<@DismissalCause Integer> mOnDismissedCallback;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerCoordinator}.
     *
     * <p>This class should only be instantiated for a managed account.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The {@link BottomSheetController} for showing the bottom sheet.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the modal dialog.
     * @param signinManager The {@link SigninManager} for checking management status and fetching
     *     the profile picture.
     * @param account The account the disclaimer is shown for.
     * @param delegate The {@link Delegate} for embedder interactions.
     * @param onDestroyCallback Callback to be invoked when the coordinator is destroyed.
     * @param metricsHelper The {@link MetricsHelper} for recording interaction metrics.
     * @param onDismissedCallback Callback to be invoked with the {@link DismissalCause} when the
     *     disclaimer is dismissed.
     */
    public EnterpriseSignalsDisclaimerCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            IdentityManager identityManager,
            CoreAccountInfo account,
            Delegate delegate,
            Runnable onDestroyCallback,
            MetricsHelper metricsHelper,
            Callback<@DismissalCause Integer> onDismissedCallback) {
        mOnDestroyCallback = onDestroyCallback;
        mDelegate = delegate;
        mMetricsHelper = metricsHelper;
        mOnDismissedCallback = onDismissedCallback;

        // For the large form factors a modal dialog will be displayed, while smaller screens will
        // get a bottom sheet.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            mView = EnterpriseSignalsDisclaimerView.createForModalDialog(context);
            mDisclaimerHost =
                    new ModalDialogDisclaimerHost(
                            modalDialogManager, mView, this::onDialogDismissed);
        } else {
            var sheetContent = new EnterpriseSignalsDisclaimerBottomSheetView(context);
            mView = sheetContent;
            mDisclaimerHost =
                    new BottomSheetDisclaimerHost(
                            bottomSheetController, sheetContent, this::onDialogDismissed);
        }

        mView.addOnAttachStateChangeListener(this);

        mMediator =
                new EnterpriseSignalsDisclaimerMediator(
                        context, identityManager, account, /* delegate= */ this);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, EnterpriseSignalsDisclaimerViewBinder::bind);
    }

    /**
     * Attempts to show the enterprise signals disclaimer. If the dialog cannot be shown it will be
     * put in a queue and shown whenever possible.
     */
    public void show(@MetricsHelper.ShownOn int shownOn) {
        assert !mIsDestroyed;
        mShownOn = shownOn;
        mDisclaimerHost.show();
    }

    /**
     * @return true if dialog is being shown or is in queue, false otherwise.
     */
    public boolean isActive() {
        return !mIsDestroyed && mDisclaimerHost.isActive();
    }

    private void onDialogDismissed(@DismissalCause int dismissalCause) {
        mMetricsHelper.recordResult(dismissalCause);
        if (mShownAtUptimeMillis != UNSET_TIME) {
            MetricsHelper.recordTimeToUserAction(TimeUtils.uptimeMillis() - mShownAtUptimeMillis);
            mShownAtUptimeMillis = UNSET_TIME;
        }
        if (mOnDismissedCallback != null) {
            mOnDismissedCallback.onResult(dismissalCause);
            mOnDismissedCallback = null;
        }
        destroy();
    }

    /** Destroys the coordinator, hiding the sheet and cleaning up resources. */
    public void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mView.removeOnAttachStateChangeListener(this);
        mDisclaimerHost.destroy();
        mModelChangeProcessor.destroy();
        mMediator.destroy();
        if (mOnDestroyCallback != null) {
            mOnDestroyCallback.run();
            mOnDestroyCallback = null;
        }
    }

    // EnterpriseSignalsDisclaimerMediator.Delegate implementation.
    @Override
    public void showInfoPage(String url) {
        mDelegate.showInfoPage(url);
    }

    @Override
    public void onAccept() {
        mDisclaimerHost.dismiss(DismissalCause.TAPPED_ACCEPT);
    }

    @Override
    public void onDecline() {
        mDisclaimerHost.dismiss(DismissalCause.TAPPED_SIGN_OUT);
    }

    // View.OnAttachStateChangeListener implementation.
    @Override
    public void onViewAttachedToWindow(View view) {
        if (mIsDestroyed) {
            return;
        }

        MetricsHelper.recordShown(assertNonNull(mShownOn));
        mShownAtUptimeMillis = TimeUtils.uptimeMillis();
        view.removeOnAttachStateChangeListener(this);
    }

    @Override
    public void onViewDetachedFromWindow(View view) {}
}
