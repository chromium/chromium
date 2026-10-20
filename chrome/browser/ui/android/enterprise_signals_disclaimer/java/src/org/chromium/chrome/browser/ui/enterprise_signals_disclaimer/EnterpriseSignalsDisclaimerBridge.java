// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.google_apis.gaia.GaiaId;

/**
 * Native bridge for Enterprise Signals Disclaimer.
 *
 * <p>The acknowledgment is stored per account in the profile's SigninPrefs.
 */
@NullMarked
class EnterpriseSignalsDisclaimerBridge {
    private EnterpriseSignalsDisclaimerBridge() {}

    /**
     * Returns whether the account represented by {@code gaiaId} has acknowledged the enterprise
     * signals disclaimer in {@code profile}.
     */
    public static boolean hasAccountAcknowledgedSignalsDisclaimer(Profile profile, GaiaId gaiaId) {
        return EnterpriseSignalsDisclaimerBridgeJni.get()
                .hasAccountAcknowledgedSignalsDisclaimer(profile, gaiaId);
    }

    /**
     * Marks the enterprise signals disclaimer as acknowledged by the account represented by {@code
     * gaiaId} in {@code profile}.
     */
    public static void setAccountAcknowledgedSignalsDisclaimer(Profile profile, GaiaId gaiaId) {
        EnterpriseSignalsDisclaimerBridgeJni.get()
                .setAccountAcknowledgedSignalsDisclaimer(profile, gaiaId);
    }

    /**
     * Clears the enterprise signals disclaimer acknowledgment of the account represented by {@code
     * gaiaId} in {@code profile}.
     */
    public static void clearAccountAcknowledgedSignalsDisclaimer(Profile profile, GaiaId gaiaId) {
        EnterpriseSignalsDisclaimerBridgeJni.get()
                .clearAccountAcknowledgedSignalsDisclaimer(profile, gaiaId);
    }

    @NativeMethods
    public interface Natives {
        boolean hasAccountAcknowledgedSignalsDisclaimer(
                @JniType("Profile*") Profile profile, @JniType("GaiaId") GaiaId gaiaId);

        void setAccountAcknowledgedSignalsDisclaimer(
                @JniType("Profile*") Profile profile, @JniType("GaiaId") GaiaId gaiaId);

        void clearAccountAcknowledgedSignalsDisclaimer(
                @JniType("Profile*") Profile profile, @JniType("GaiaId") GaiaId gaiaId);
    }
}
