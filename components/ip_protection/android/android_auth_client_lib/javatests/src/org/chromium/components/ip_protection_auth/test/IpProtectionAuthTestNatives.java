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

        void testNonexistantService();

        void testNullBindingService();

        void testDisabledService();

        void testRestrictedService();

        void testGetInitialData();

        void testAuthAndSign();

        void testGetProxyConfig();

        void testTransientError();

        void testPersistentError();

        void testIllegalErrorCode();

        void testNullResponse();

        void testUnparsableResponse();

        void testSynchronousError();

        void testUnresolvedWhenClosed();

        void testCrashOnRequestSyncWithoutResponse();

        void testCrashOnRequestAsyncWithoutResponse();

        void testCrashOnRequestSyncWithResponse();

        void testUnresolvedCallbacksRejectedAfterCrash();
    }

    public static void createConnectedInstanceForTesting() {
        IpProtectionAuthTestNativesJni.get().createConnectedInstanceForTesting();
    }

    public static void testNonexistantService() {
        IpProtectionAuthTestNativesJni.get().testNonexistantService();
    }

    public static void testNullBindingService() {
        IpProtectionAuthTestNativesJni.get().testNullBindingService();
    }

    public static void testDisabledService() {
        IpProtectionAuthTestNativesJni.get().testDisabledService();
    }

    public static void testRestrictedService() {
        IpProtectionAuthTestNativesJni.get().testRestrictedService();
    }

    public static void testGetInitialData() {
        IpProtectionAuthTestNativesJni.get().testGetInitialData();
    }

    public static void testAuthAndSign() {
        IpProtectionAuthTestNativesJni.get().testAuthAndSign();
    }

    public static void testGetProxyConfig() {
        IpProtectionAuthTestNativesJni.get().testGetProxyConfig();
    }

    public static void testTransientError() {
        IpProtectionAuthTestNativesJni.get().testTransientError();
    }

    public static void testPersistentError() {
        IpProtectionAuthTestNativesJni.get().testPersistentError();
    }

    public static void testIllegalErrorCode() {
        IpProtectionAuthTestNativesJni.get().testIllegalErrorCode();
    }

    public static void testNullResponse() {
        IpProtectionAuthTestNativesJni.get().testNullResponse();
    }

    public static void testUnparsableResponse() {
        IpProtectionAuthTestNativesJni.get().testUnparsableResponse();
    }

    public static void testSynchronousError() {
        IpProtectionAuthTestNativesJni.get().testSynchronousError();
    }

    public static void testUnresolvedWhenClosed() {
        IpProtectionAuthTestNativesJni.get().testUnresolvedWhenClosed();
    }

    public static void testCrashOnRequestSyncWithoutResponse() {
        IpProtectionAuthTestNativesJni.get().testCrashOnRequestSyncWithoutResponse();
    }

    public static void testCrashOnRequestAsyncWithoutResponse() {
        IpProtectionAuthTestNativesJni.get().testCrashOnRequestAsyncWithoutResponse();
    }

    public static void testCrashOnRequestSyncWithResponse() {
        IpProtectionAuthTestNativesJni.get().testCrashOnRequestSyncWithResponse();
    }

    public static void testUnresolvedCallbacksRejectedAfterCrash() {
        IpProtectionAuthTestNativesJni.get().testUnresolvedCallbacksRejectedAfterCrash();
    }
}
