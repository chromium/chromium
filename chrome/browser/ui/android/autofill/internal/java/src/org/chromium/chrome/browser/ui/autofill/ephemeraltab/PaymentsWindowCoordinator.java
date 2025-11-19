// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinatorSupplier;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** The coordinator for triggering the Ephemeral Tab. */
@NullMarked
class PaymentsWindowCoordinator implements EphemeralTabObserver {
    private final PaymentsWindowBridge mPaymentsWindowBridge;
    private @Nullable EphemeralTabCoordinator mEphemeralTabCoordinator;

    /**
     * Constructs a new {@code PaymentsWindowCoordinator} from the provided {@code
     * PaymentsWindowBridge}.
     *
     * @param paymentsWindowBridge The {@code PaymentsWindowBridge} that facilitates communication
     *     with the native payments logic.
     */
    PaymentsWindowCoordinator(PaymentsWindowBridge paymentsWindowBridge) {
        mPaymentsWindowBridge = paymentsWindowBridge;
    }

    /**
     * Attempts to open an ephemeral tab; it involves obtaining the {@code WindowAndroid} from the
     * managed {@code WebContents} and using it to present the UI. It also adds {@code
     * EphemeralTabObserver} to listen URL navigation.
     *
     * @param url The URL to load in the new ephemeral tab.
     * @param title The title to be displayed in the header of the ephemeral tab.
     * @param merchantWebContents The {@code WebContents} for the merchant's page, which will
     *     display the BNPL provider's payment window.
     */
    void openEphemeralTab(GURL url, String title, WebContents merchantWebContents) {
        assert merchantWebContents != null;
        WindowAndroid windowAndroid = merchantWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        ObservableSupplier<EphemeralTabCoordinator> supplier =
                EphemeralTabCoordinatorSupplier.from(windowAndroid);
        if (supplier == null) return;
        mEphemeralTabCoordinator = supplier.get();
        mEphemeralTabCoordinator.addObserver(this);
        Profile profile = Profile.fromWebContents(merchantWebContents);
        assert profile != null;
        mEphemeralTabCoordinator.requestOpenSheet(
                url, /* fullPageUrl= */ null, title, profile, /* canPromoteToNewTab= */ false);
    }

    /** Attempts to close an ephemeral tab. */
    void closeEphemeralTab() {
        if (mEphemeralTabCoordinator != null && mEphemeralTabCoordinator.isOpened()) {
            mEphemeralTabCoordinator.close();
        }
    }

    // EphemeralTabObserver:
    @Override
    public void onNavigationFinished(GURL clickedUrl) {
        mPaymentsWindowBridge.onNavigationFinished(clickedUrl);
    }

    // EphemeralTabObserver:
    @Override
    public void onWebContentsObservationStarted(WebContents webContents) {
        mPaymentsWindowBridge.onWebContentsObservationStarted(webContents);
    }

    // EphemeralTabObserver:
    @Override
    public void onWebContentsDestroyed() {
        if (mEphemeralTabCoordinator != null) {
            mEphemeralTabCoordinator.removeObserver(this);
        }
        mPaymentsWindowBridge.onWebContentsDestroyed();
    }

    void setEphemeralTabCoordinatorForTesting(EphemeralTabCoordinator ephemeralTabCoordinator) {
        mEphemeralTabCoordinator = ephemeralTabCoordinator;
    }
}
