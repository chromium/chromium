// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerCoordinator.PresentationMode;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.function.Consumer;

/**
 * Presents the enterprise signals disclaimer and reports the user's decision.
 *
 * <p>This class acts as the embedder of {@link EnterpriseSignalsDisclaimerCoordinator}: it decides
 * how the disclaimer is presented (a modal dialog on large form factors, a bottom sheet otherwise)
 * and hosts the disclaimer view. It also records metrics about the user's interaction with the
 * disclaimer. It contains no business logic: it doesn't decide whether the disclaimer should be
 * shown, and doesn't act on the user's decision beyond notifying the {@link Listener}. See {@link
 * StartupEnterpriseSignalsDisclaimerController} for the startup/sign-in flow.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerController {
    private static final long UNSET_TIME = -1;

    /** The event that triggered showing the disclaimer. Used for metrics. */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(EnterpriseSignalsDisclaimerShownOn)
    @IntDef({ShownOn.STARTUP, ShownOn.SIGN_IN, ShownOn.SIGNIN_STATE_RECOVERY, ShownOn.COUNT})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShownOn {
        int STARTUP = 0;
        int SIGN_IN = 1;
        int SIGNIN_STATE_RECOVERY = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseSignalsDisclaimerShownOn)

    /** Notified about the user's decision. */
    public interface Listener {
        /** The user accepted the disclaimer. */
        void onAccepted();

        /**
         * The user declined the disclaimer, either explicitly or by dismissing it (e.g. back press,
         * swipe down, tap outside).
         */
        void onDeclined();
    }

    /** Used for testing to mock EnterpriseSignalsDisclaimerCoordinator. */
    interface CoordinatorFactory {
        EnterpriseSignalsDisclaimerCoordinator create(
                Context context,
                IdentityManager identityManager,
                CoreAccountInfo account,
                @PresentationMode int presentationMode,
                EnterpriseSignalsDisclaimerCoordinator.Delegate delegate);
    }

    /** Creates the host for the disclaimer. Can be replaced in tests. */
    interface HostFactory {
        EnterpriseSignalsDisclaimerHost create(
                @PresentationMode int presentationMode,
                EnterpriseSignalsDisclaimerCoordinator coordinator,
                Consumer<@DismissalCause Integer> onDismissedCallback);
    }

    /** Handles callbacks from the {@link EnterpriseSignalsDisclaimerCoordinator}. */
    private class CoordinatorDelegate implements EnterpriseSignalsDisclaimerCoordinator.Delegate {
        @Override
        public void showInfoPage(String url) {
            mShowInfoPageCallback.onResult(url);
        }

        @Override
        public void onShown() {
            EnterpriseSignalsDisclaimerController.this.onShown();
        }

        @Override
        public void onAccept() {
            if (mDisclaimerHost != null) {
                mDisclaimerHost.dismiss(DismissalCause.TAPPED_ACCEPT);
            }
        }

        @Override
        public void onDecline() {
            if (mDisclaimerHost != null) {
                mDisclaimerHost.dismiss(DismissalCause.TAPPED_SIGN_OUT);
            }
        }
    }

    private final Context mContext;
    private final Callback<String> mShowInfoPageCallback;
    private final CoordinatorFactory mCoordinatorFactory;
    private final HostFactory mHostFactory;
    private final EnterpriseSignalsDisclaimerCoordinator.Delegate mCoordinatorDelegate =
            new CoordinatorDelegate();
    // Must persist across multiple disclaimer instances, see MetricsHelper#recordResult.
    private final MetricsHelper mMetricsHelper = new MetricsHelper();

    private @Nullable EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    private @Nullable EnterpriseSignalsDisclaimerHost mDisclaimerHost;
    private boolean mIsDestroyed;
    private @Nullable @ShownOn Integer mShownOn;
    private long mShownAtUptimeMillis = UNSET_TIME;

    /**
     * @param context The Activity {@link Context} the disclaimer is shown in.
     * @param bottomSheetController The {@link BottomSheetController} for showing the disclaimer.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the modal dialog.
     * @param showInfoPageCallback Callback invoked with a URL to open an info page.
     */
    public EnterpriseSignalsDisclaimerController(
            Context context,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            Callback<String> showInfoPageCallback) {
        this(
                context,
                showInfoPageCallback,
                EnterpriseSignalsDisclaimerCoordinator::new,
                createDefaultHostFactory(bottomSheetController, modalDialogManager));
    }

    EnterpriseSignalsDisclaimerController(
            Context context,
            Callback<String> showInfoPageCallback,
            CoordinatorFactory coordinatorFactory,
            HostFactory hostFactory) {
        mContext = context;
        mShowInfoPageCallback = showInfoPageCallback;
        mCoordinatorFactory = coordinatorFactory;
        mHostFactory = hostFactory;
    }

    /**
     * Returns a {@link HostFactory} which presents the disclaimer in a modal dialog for {@link
     * PresentationMode#MODAL_DIALOG} and in a bottom sheet otherwise.
     */
    @VisibleForTesting
    static HostFactory createDefaultHostFactory(
            BottomSheetController bottomSheetController, ModalDialogManager modalDialogManager) {
        return (presentationMode, coordinator, onDismissedCallback) -> {
            if (presentationMode == PresentationMode.MODAL_DIALOG) {
                return new ModalDialogDisclaimerHost(
                        modalDialogManager, coordinator.getView(), onDismissedCallback);
            }
            return new BottomSheetDisclaimerHost(
                    bottomSheetController,
                    coordinator.getView(),
                    coordinator::getVerticalScrollOffset,
                    onDismissedCallback);
        };
    }

    /**
     * Shows the disclaimer for the given account. If it can't be shown immediately it is queued and
     * shown whenever possible. No checks on whether the disclaimer should be shown are made.
     *
     * @param identityManager The {@link IdentityManager} used to fetch the account information.
     * @param account The account the disclaimer is shown for.
     * @param shownOn The event that triggered showing the disclaimer, used for metrics.
     * @param listener Notified about the user's decision.
     * @return true if the disclaimer was shown (or queued), false if this controller was destroyed
     *     or a disclaimer is already active.
     */
    public boolean show(
            IdentityManager identityManager,
            CoreAccountInfo account,
            @ShownOn int shownOn,
            Listener listener) {
        if (mIsDestroyed || isDisclaimerActive()) {
            return false;
        }

        destroyDisclaimer();

        // If the disclaimer can't be shown immediately it will be queued by the host and shown
        // whenever possible.
        MetricsHelper.recordShownRequested(shownOn);
        mShownOn = shownOn;

        @PresentationMode int presentationMode = getPresentationMode();
        mCoordinator =
                mCoordinatorFactory.create(
                        mContext, identityManager, account, presentationMode, mCoordinatorDelegate);
        mDisclaimerHost =
                mHostFactory.create(
                        presentationMode,
                        mCoordinator,
                        dismissalCause -> onHostDismissed(dismissalCause, listener));
        mDisclaimerHost.show();
        return true;
    }

    /** Returns whether the disclaimer is being shown or is queued to be shown. */
    public boolean isDisclaimerActive() {
        return mDisclaimerHost != null && mDisclaimerHost.isActive();
    }

    /** Destroys the controller, tearing down the disclaimer if it's showing or queued. */
    public void destroy() {
        mIsDestroyed = true;
        destroyDisclaimer();
    }

    protected boolean isDestroyed() {
        return mIsDestroyed;
    }

    /** Tears down the currently displayed (or queued) disclaimer, if any. */
    protected void destroyDisclaimer() {
        if (mDisclaimerHost != null) {
            var host = mDisclaimerHost;
            mDisclaimerHost = null;
            host.destroy();
        }
        if (mCoordinator != null) {
            var coordinator = mCoordinator;
            mCoordinator = null;
            coordinator.destroy();
        }
        mShownOn = null;
        mShownAtUptimeMillis = UNSET_TIME;
    }

    /** Called the first time the disclaimer view is shown. */
    private void onShown() {
        if (mShownOn == null) return;
        MetricsHelper.recordShown(mShownOn);
        mShownAtUptimeMillis = TimeUtils.uptimeMillis();
    }

    /** Called when the disclaimer is dismissed, before it's torn down. */
    private void onDismissed(@DismissalCause int dismissalCause) {
        mMetricsHelper.recordResult(dismissalCause);
        if (mShownAtUptimeMillis != UNSET_TIME) {
            MetricsHelper.recordTimeToUserAction(TimeUtils.uptimeMillis() - mShownAtUptimeMillis);
        }
    }

    /** Large form factors get a modal dialog, while smaller screens get a bottom sheet. */
    private @PresentationMode int getPresentationMode() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                ? PresentationMode.MODAL_DIALOG
                : PresentationMode.BOTTOM_SHEET;
    }

    private void onHostDismissed(@DismissalCause int dismissalCause, Listener listener) {
        onDismissed(dismissalCause);
        destroyDisclaimer();

        if (dismissalCause == DismissalCause.TAPPED_ACCEPT) {
            listener.onAccepted();
        } else if (isDeclined(dismissalCause)) {
            listener.onDeclined();
        }
        // Dismissals without an explicit user action are not reported.
    }

    private static boolean isDeclined(@DismissalCause int dismissalCause) {
        return dismissalCause == DismissalCause.TAPPED_SIGN_OUT
                || dismissalCause == DismissalCause.DISMISSED_BY_BACK_PRESS
                || dismissalCause == DismissalCause.DISMISSED_BY_SWIPE_DOWN
                || dismissalCause == DismissalCause.DISMISSED_BY_TAP_OUTSIDE
                || dismissalCause == DismissalCause.DISMISSED_BY_CLOSE_BUTTON;
    }
}
