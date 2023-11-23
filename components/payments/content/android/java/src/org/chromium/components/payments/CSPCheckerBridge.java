// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;

/**
 * Native bridge for Content-Security-Policy (CSP) checker. Provides access to a Java implementation
 * of CSP-checking, albeit that implementation pipes over to renderer code.
 *
 * The destroy() method must always be called to clean up the native owned object.
 *
 * Usage example.
 *   CSPCheckerBridge bridge = new CSPCheckerBridge(cspChecker);
 *   useNativeCSPChecker(bridge.getNativeCSPChecker());
 *   bridge.destroy();
 */
@JNINamespace("payments")
public class CSPCheckerBridge {
    // Performs the CSP checks.
    private final CSPChecker mImpl;

    // The native interface for accessing the CSP checker.
    private long mNativeBridge;

    /**
     * Initializes the CSP checker bridge.
     * @param cspChecker The object that will perform the CSP checks.
     */
    public CSPCheckerBridge(@NonNull CSPChecker cspChecker) {
        mImpl = cspChecker;
        mNativeBridge = CSPCheckerBridgeJni.get().createNativeCSPChecker(this);
    }

    /** Destroys the CSP checker bridge. Must be called when this class is no longer being used. */
    public void destroy() {
        if (mNativeBridge != 0) {
            CSPCheckerBridgeJni.get().destroy(mNativeBridge);
            mNativeBridge = 0;
        }
    }

    /** @return The native C++ pointer to a CSP checker object. Owned by the Java object. */
    public long getNativeCSPChecker() {
        return mNativeBridge;
    }

    /**
     * Checks whether CSP connect-src directive allows the given URL. The parameters match
     * ContentSecurityPolicy::AllowConnectToSource() in:
     * third_party/blink/renderer/core/frame/csp/content_security_policy.h
     * @param url The URL to check.
     * @param urlBeforeRedirects The URL before redirects, if there was a redirect.
     * @param didFollowRedirect Whether there was a redirect.
     * @param callbackId The identifier of the callback to invoke with the result of the CSP check.
     */
    @CalledByNative
    public void allowConnectToSource(
            GURL url, GURL urlBeforeRedirects, boolean didFollowRedirect, int callbackId) {
        mImpl.allowConnectToSource(
                url,
                urlBeforeRedirects,
                didFollowRedirect,
                (Boolean result) -> {
                    if (mNativeBridge != 0) {
                        CSPCheckerBridgeJni.get().onResult(mNativeBridge, callbackId, result);
                    }
                });
    }

    @NativeMethods
    public interface Natives {
        long createNativeCSPChecker(CSPCheckerBridge bridge);

        void onResult(long nativeCSPCheckerAndroid, int callbackId, boolean result);

        void destroy(long nativeCSPCheckerAndroid);
    }
}
