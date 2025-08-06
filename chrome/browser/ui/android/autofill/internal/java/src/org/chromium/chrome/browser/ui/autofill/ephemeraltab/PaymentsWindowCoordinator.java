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
class PaymentsWindowCoordinator {
    private final WebContents mWebContents;
    private @Nullable EphemeralTabCoordinator mEphemeralTabCoordinator;
    private @Nullable EphemeralTabObserver mEphemeralTabObserver;

    /** Constructs a new {@code PaymentsWindowCoordinator} from the provided {@code WebContents}. */
    PaymentsWindowCoordinator(WebContents webContents) {
        mWebContents = webContents;
    }

    /**
     * Attempts to open an ephemeral tab; it involves obtaining the {@code WindowAndroid} from the
     * managed {@code WebContents} and using it to present the UI. It also adds {@code
     * EphemeralTabObserver} to listen URL navigation.
     */
    void openEphemeralTab(GURL url, String title) {
        assert mWebContents != null;
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        ObservableSupplier<EphemeralTabCoordinator> supplier =
                EphemeralTabCoordinatorSupplier.from(windowAndroid);
        if (supplier == null) return;
        mEphemeralTabCoordinator = supplier.get();
        mEphemeralTabObserver =
                new EphemeralTabObserver() {
                    @Override
                    public void onNavigationFinished(GURL clickedUrl) {
                        // TODO(crbug.com/430575808): Notify AndroidPaymentsWindowManager of the URL
                        // navigation to check for issuer flow completion.
                    }

                    @Override
                    public void onWebContentsDestroyed() {
                        // TODO(crbug.com/430575808): Notify AndroidPaymentsWindowManager when web
                        // contents are destroyed and remove observer.
                    }
                };
        mEphemeralTabCoordinator.addObserver(mEphemeralTabObserver);
        Profile profile = Profile.fromWebContents(mWebContents);
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

    WebContents getWebContentsForTesting() {
        return mWebContents;
    }

    void setEphemeralTabCoordinatorForTesting(EphemeralTabCoordinator ephemeralTabCoordinator) {
        mEphemeralTabCoordinator = ephemeralTabCoordinator;
    }
}
