// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.test;

import org.jni_zero.NativeMethods;

public final class IpProtectionAuthTestNatives {
    static {
        IpProtectionAuthTestNativesJni.get().initialize();
    }

    @NativeMethods
    public interface Natives {
        void initialize();

        void createConnectedInstanceForTesting();

        void testGetInitialData();

        void testAuthAndSign();
    }

    public static void createConnectedInstanceForTesting() {
        IpProtectionAuthTestNativesJni.get().createConnectedInstanceForTesting();
    }

    public static void testGetInitialData() {
        IpProtectionAuthTestNativesJni.get().testGetInitialData();
    }

    public static void testAuthAndSign() {
        IpProtectionAuthTestNativesJni.get().testAuthAndSign();
    }
}
