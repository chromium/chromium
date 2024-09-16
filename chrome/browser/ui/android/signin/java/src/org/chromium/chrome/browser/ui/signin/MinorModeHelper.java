// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SyncButtonClicked;
import org.chromium.components.signin.metrics.SyncButtonsType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls the screen mode of any UI with respect to minor-mode compliance as defined in the
 * CanShowHistorySyncOptInsWithoutMinorModeRestrictions capability.
 *
 * <p>The screen mode might be changed in one of the following circumstances:
 *
 * <ol>
 *   <li>The capability is immediately available and the UI is updated synchronously,
 *   <li>The capability is not immediately available, but the update comes in before deadline in
 *       IdentityManager.Observer::OnExtendedAccountInfoUpdated. The UI update depends on the
 *       incoming value from the IdentityManager,
 *   <li>The capability is not immediately available and neither arrives before the deadline. The UI
 *       is updated in minor-safe way.
 * </ol>
 *
 * <p>Use {@link resolveMinorMode} as an entry point.
 */
public class MinorModeHelper implements IdentityManager.Observer {

    /** Screen modes indicated by capability. */
    @IntDef({
        ScreenMode.UNSUPPORTED,
        ScreenMode.PENDING,
        ScreenMode.RESTRICTED,
        ScreenMode.UNRESTRICTED,
        ScreenMode.DEADLINED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenMode {
        int UNSUPPORTED = 0;
        // Screen mode is pending resolution to RESTRICTED or UNRESTRICTED.
        int PENDING = 1;
        // The UI must be presented in minor-mode aware way because determined by the capability.
        int RESTRICTED = 2;
        // The UI does not need to be presented in minor-mode aware way.
        int UNRESTRICTED = 3;
        // The UI must be presented in minor-mode aware way because the time to load the
        // capabilities was exceeded.
        int DEADLINED = 4;
    }

    /** Controls the actual UI Update. */
    public interface UiUpdater {
        void onScreenModeReady(@ScreenMode int screenMode);
    }

    private static final String USER_LATENCY_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.UserVisibleLatency";
    private static final String FETCH_LATENCY_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.FetchLatency";
    private static final String IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.ImmediatelyAvailable";

    private static boolean sDisableHistorySyncOptInTimeoutForTesting;

    private final long mCreated = SystemClock.elapsedRealtime();

    private final IdentityManager mIdentityManager;

    private final CoreAccountInfo mPrimaryAccount;

    // Disposable updater which is executed only once.
    private UiUpdater mUiUpdater;

    /**
     * Waits for the capability to be loaded. When this happens, the ui is updated in minor-mode
     * safe way by executing the {@link uiUpdater} callback. If the capability is not loaded in
     * relatively short time then minor more is resolved with a default value.
     *
     * <p>Tracks the availability latency of {@link AccountCapabilities} for the signed-in primary
     * account.
     *
     * @param identityManager The {@link IdentityManager} for the profile
     * @param primaryAccount {@link CoreAccountInfo} for the primary account.
     * @param uiUpdater Callback method to be run when the {@link CAPABILITY_TIMEOUT_MS} is reached
     *     or capability is retrieved.
     */
    public static void resolveMinorMode(
            IdentityManager identityManager, CoreAccountInfo primaryAccount, UiUpdater uiUpdater) {
        if (uiUpdater == null) {
            throw new IllegalArgumentException("uiUpdater must not be null.");
        }
        AccountInfo accountInfo =
                identityManager.findExtendedAccountInfoByEmailAddress(primaryAccount.getEmail());

        // TODO(crbug.com/363988040): Remove the nullness check once
        // ReplaceSyncPromosWithSigninPromos is removed.
        if (accountInfo != null && hasCapabilities(accountInfo)) {
            uiUpdater.onScreenModeReady(
                    screenModeFromCapabilities(accountInfo.getAccountCapabilities()));
            recordImmediateAvailability();
            return;
        }

        recordNoImmediateAvailability();
        identityManager.addObserver(
                new MinorModeHelper(identityManager, primaryAccount, uiUpdater));
    }

    /**
     * Records whether the buttons on sync screen and history sync were equally weighted. If the
     * buttons were unweighted it specifies if this was due to the deadline or the capability.
     *
     * @param type See {@link SyncButtonsType}
     */
    public static void recordButtonsShown(@SyncButtonsType int type) {
        SigninMetricsUtils.recordButtonsShown(type);
    }

    /**
     * Records which buttons (accept or decline) were clicked on sync screen and history sync and
     * whether the buttons were equally weighted.
     *
     * @param type See {@link SyncButtonClicked}
     */
    public static void recordButtonClicked(@SyncButtonClicked int type) {
        SigninMetricsUtils.recordButtonTypeClicked(type);
    }

    /**
     * Returns true iff {@param accountInfo} contains an
     * CanShowHistorySyncOptInsWithoutMinorModeRestrictions capability.
     */
    private static boolean hasCapabilities(AccountInfo accountInfo) {
        AccountCapabilities capabilities = accountInfo.getAccountCapabilities();
        return capabilities.canShowHistorySyncOptInsWithoutMinorModeRestrictions()
                != Tribool.UNKNOWN;
    }

    private static @ScreenMode int screenModeFromCapabilities(AccountCapabilities capabilities) {
        switch (capabilities.canShowHistorySyncOptInsWithoutMinorModeRestrictions()) {
            case Tribool.UNKNOWN:
                return ScreenMode.PENDING;
            case Tribool.TRUE:
                return ScreenMode.UNRESTRICTED;
            case Tribool.FALSE:
                return ScreenMode.RESTRICTED;
            default:
                throw new IllegalArgumentException(
                        "Unexpected capability value: "
                                + capabilities
                                        .canShowHistorySyncOptInsWithoutMinorModeRestrictions());
        }
    }

    private MinorModeHelper(
            IdentityManager identityManager, CoreAccountInfo primaryAccount, UiUpdater uiUpdater) {
        this.mIdentityManager = identityManager;
        this.mPrimaryAccount = primaryAccount;
        mUiUpdater = uiUpdater;

        // When the sDisableHistorySyncOptInTimeoutForTesting is enabled in tests, the buttons
        // should only be updated due to a capability change and not due to a timeout.
        if (!sDisableHistorySyncOptInTimeoutForTesting) {
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, this::onDeadline, /* delay= */ 1000);
        }
    }

    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        if (!mPrimaryAccount.getId().equals(accountInfo.getId())) {
            // Update intended for different account.
            return;
        }
        if (!hasCapabilities(accountInfo)) {
            // Non-interesting update with no capability-yet.
            return;
        }

        executeUiChanges(screenModeFromCapabilities(accountInfo.getAccountCapabilities()));
    }

    private void onDeadline() {
        executeUiChanges(ScreenMode.DEADLINED);
    }

    /** Executes ui changes defined in {@link mUiUpdater}, but does this only once. */
    private void executeUiChanges(@ScreenMode int screenMode) {
        if (screenMode == ScreenMode.PENDING) {
            throw new IllegalArgumentException("screenMode cannot not be PENDING.");
        }
        if (mUiUpdater == null) {
            // Ui changes might be only executed once.
            return;
        }

        mUiUpdater.onScreenModeReady(screenMode);
        mUiUpdater = null;
        recordFetchLatency();
        mIdentityManager.removeObserver(this);
    }

    private static void recordImmediateAvailability() {
        RecordHistogram.recordTimesHistogram(USER_LATENCY_HISTOGRAM_NAME, 0);
        RecordHistogram.recordBooleanHistogram(IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME, true);
    }

    private static void recordNoImmediateAvailability() {
        RecordHistogram.recordBooleanHistogram(IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME, false);
    }

    private void recordFetchLatency() {
        long latency = SystemClock.elapsedRealtime() - mCreated;
        RecordHistogram.recordTimesHistogram(USER_LATENCY_HISTOGRAM_NAME, latency);
        RecordHistogram.recordTimesHistogram(FETCH_LATENCY_HISTOGRAM_NAME, latency);
    }

    /** Disable timeout to show sync buttons on FRE for testing */
    public static void disableTimeoutForTesting() {
        sDisableHistorySyncOptInTimeoutForTesting = true;
    }
}
