// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.List;

/** Native bridge for Enterprise Signals Disclaimer. */
@NullMarked
public class EnterpriseSignalsDisclaimerBridge {
    private EnterpriseSignalsDisclaimerBridge() {}

    public static boolean hasAccountAcknowledgedSignalsDisclaimer(GaiaId gaiaId) {
        return EnterpriseSignalsDisclaimerBridgeJni.get()
                .hasAccountAcknowledgedSignalsDisclaimer(gaiaId);
    }

    public static void setAccountAcknowledgedSignalsDisclaimer(GaiaId gaiaId) {
        EnterpriseSignalsDisclaimerBridgeJni.get().setAccountAcknowledgedSignalsDisclaimer(gaiaId);
    }

    public static void removeUnknownAccounts(List<GaiaId> gaiaIds) {
        EnterpriseSignalsDisclaimerBridgeJni.get().removeUnknownAccounts(gaiaIds);
    }

    @NativeMethods
    public interface Natives {
        boolean hasAccountAcknowledgedSignalsDisclaimer(@JniType("GaiaId") GaiaId gaiaId);

        void setAccountAcknowledgedSignalsDisclaimer(@JniType("GaiaId") GaiaId gaiaId);

        void removeUnknownAccounts(@JniType("std::vector<GaiaId>") List<GaiaId> gaiaIds);
    }
}
