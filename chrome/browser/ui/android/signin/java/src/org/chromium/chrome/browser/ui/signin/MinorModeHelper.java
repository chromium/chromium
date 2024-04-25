// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

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
 * <p>Use {@link resolveMinorMode} and {@link trackLatency} methods as entry points.
 */
public class MinorModeHelper implements IdentityManager.Observer {

    /** Screen modes indicated by capability. */
    @IntDef({ScreenMode.PENDING, ScreenMode.RESTRICTED, ScreenMode.UNRESTRICTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenMode {
        // Screen mode is pending resolution to RESTRICTED or UNRESTRICTED.
        int PENDING = 0;
        // The UI must be presented in minor-mode aware way.
        int RESTRICTED = 1;
        // The UI does not need to be presented in minor-mode aware way.
        int UNRESTRICTED = 2;
    }

    /** Controls the actual UI Update. */
    interface UiUpdater {
        void onScreenModeReady(@ScreenMode int screenMode);
    }

    private static final String USER_LATENCY_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.UserVisibleLatency";
    private static final String FETCH_LATENCY_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.FetchLatency";
    private static final String IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.ImmediatelyAvailable";

    private static final String BUTTONS_SHOWN_HISTOGRAM_NAME = "Signin.SyncButtons.Shown";
    private static final String BUTTON_CLICKED_HISTOGRAM_NAME = "Signin.SyncButtons.Clicked";

    public @interface SyncButtonsType {
        // These values are persisted to logs. Entries should not be renumbered and
        // numeric values should never be reused.
        int SYNC_EQUAL_WEIGHTED = 0;
        int SYNC_NOT_EQUAL_WEIGHTED = 1;
        int HISTORY_SYNC_EQUAL_WEIGHTED = 2;
        int HISTORY_SYNC_NOT_EQUAL_WEIGHTED = 3;
        int NUM_ENTRIES = 4;
    };

    public @interface SyncButtonClicked {
        // These values are persisted to logs. Entries should not be renumbered and
        // numeric values should never be reused.
        int SYNC_OPT_IN_EQUAL_WEIGHTED = 0;
        int SYNC_CANCEL_EQUAL_WEIGHTED = 1;
        int SYNC_SETTINGS_EQUAL_WEIGHTED = 2;
        int SYNC_OPT_IN_NOT_EQUAL_WEIGHTED = 3;
        int SYNC_CANCEL_NOT_EQUAL_WEIGHTED = 4;
        int SYNC_SETTINGS_NOT_EQUAL_WEIGHTED = 5;
        int HISTORY_SYNC_OPT_IN_EQUAL_WEIGHTED = 6;
        int HISTORY_SYNC_CANCEL_EQUAL_WEIGHTED = 7;
        int HISTORY_SYNC_OPT_IN_NOT_EQUAL_WEIGHTED = 8;
        int HISTORY_SYNC_CANCEL_NOT_EQUAL_WEIGHTED = 9;
        int NUM_ENTRIES = 8;
    };

    private static final int CAPABILITY_TIMEOUT_MS = 400;

    private static boolean sDisableHistorySyncOptInTimeout;

    private final long mCreated = SystemClock.elapsedRealtime();

    private final IdentityManager mIdentityManager;

    private final CoreAccountInfo mPrimaryAccount;

    // Disposable updater which is executed only once.
    private UiUpdater mUiUpdater;

    /**
     * Waits for the capability to be loaded. When this happens, the ui is updated in minor-mode
     * safe way by executing the {@link uiUpdater} callback. If the capability is not loaded in
     * relatively short {@link CAPABILITY_TIMEOUT_MS} time then minor more is resolved with a
     * default value.
     *
     * <p>Tracks the availability latency of {@link AccountCapabilities} for the signed-in primary
     * account.
     */
    static void resolveMinorMode(
            IdentityManager identityManager, CoreAccountInfo primaryAccount, UiUpdater uiUpdater) {
        if (uiUpdater == null) {
            throw new IllegalArgumentException("uiUpdater must not be null.");
        }
        AccountInfo accountInfo =
                identityManager.findExtendedAccountInfoByEmailAddress(primaryAccount.getEmail());

        // TODO(b/40284908): remove accountInfo null check
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

    /** Similar to {@link resolveMinorMode}, but only tracks latency, without altering the UI. */
    static void trackLatency(IdentityManager identityManager, CoreAccountInfo primaryAccount) {
        resolveMinorMode(identityManager, primaryAccount, (mode) -> {});
    }

    static void recordButtonsShown(@SyncButtonsType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                BUTTONS_SHOWN_HISTOGRAM_NAME, type, SyncButtonsType.NUM_ENTRIES);
    }

    static void recordButtonClicked(@SyncButtonClicked int type) {
        RecordHistogram.recordEnumeratedHistogram(
                BUTTON_CLICKED_HISTOGRAM_NAME, type, SyncButtonClicked.NUM_ENTRIES);
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

        // When the sDisableHistorySyncOptInTimeout is enabled in tests, the buttons should only be
        // updated due to a capability change and not due to a timeout.
        if (!sDisableHistorySyncOptInTimeout) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT, this::defaultToRestricted, CAPABILITY_TIMEOUT_MS);
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

    private void defaultToRestricted() {
        executeUiChanges(ScreenMode.RESTRICTED);
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
        sDisableHistorySyncOptInTimeout = true;
    }
}
