// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;
import androidx.credentials.exceptions.CreateCredentialCancellationException;
import androidx.credentials.exceptions.CreateCredentialInterruptedException;
import androidx.credentials.exceptions.CreateCredentialNoCreateOptionException;
import androidx.credentials.exceptions.GetCredentialCancellationException;
import androidx.credentials.exceptions.GetCredentialInterruptedException;
import androidx.credentials.exceptions.NoCredentialException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.webid.DigitalIdentityRequestStatusForMetrics;
import org.chromium.ui.base.WindowAndroid;

/** Class for issuing request to the Identity Credentials Manager in GMS core. */
@NullMarked
public class DigitalIdentityProvider {
    private static final String TAG = "DigitalIdentityProvider";
    private long mDigitalIdentityProvider;
    private static IdentityCredentialsDelegate sCredentials = new IdentityCredentialsDelegate();

    private DigitalIdentityProvider(long digitalIdentityProvider) {
        mDigitalIdentityProvider = digitalIdentityProvider;
    }

    public static void setDelegateForTesting(IdentityCredentialsDelegate mock) {
        var oldValue = sCredentials;
        sCredentials = mock;
        ResettersForTesting.register(() -> sCredentials = oldValue);
    }

    @VisibleForTesting
    public static @DigitalIdentityRequestStatusForMetrics int computeStatusForMetricsFromException(
            Exception e) {
        if (e instanceof GetCredentialCancellationException
                || e instanceof CreateCredentialCancellationException
                || e instanceof GetCredentialInterruptedException
                || e instanceof CreateCredentialInterruptedException) {
            return DigitalIdentityRequestStatusForMetrics.ERROR_USER_DECLINED;
        }
        if (e instanceof NoCredentialException
                || e instanceof CreateCredentialNoCreateOptionException) {
            return DigitalIdentityRequestStatusForMetrics.ERROR_NO_CREDENTIAL;
        }
        return DigitalIdentityRequestStatusForMetrics.ERROR_OTHER;
    }

    // Methods that are called by native implementation
    @CalledByNative
    private static DigitalIdentityProvider create(long digitalIdentityProvider) {
        return new DigitalIdentityProvider(digitalIdentityProvider);
    }

    @CalledByNative
    private void destroy() {
        mDigitalIdentityProvider = 0;
    }

    /**
     * Triggers a request to the Identity Credentials Manager in GMS.
     *
     * @param window The window associated with the request.
     * @param origin The origin of the requester.
     * @param request The request.
     */
    @CalledByNative
    void request(
            WindowAndroid window,
            @JniType("std::string") String origin,
            @JniType("std::string") String request) {
        sCredentials
                .get(window, origin, request)
                .then(
                        response -> {
                            if (mDigitalIdentityProvider != 0) {
                                DigitalIdentityProviderJni.get()
                                        .onReceive(
                                                mDigitalIdentityProvider,
                                                response.mProtocol,
                                                response.mData,
                                                DigitalIdentityRequestStatusForMetrics.SUCCESS);
                            }
                        },
                        e -> {
                            if (mDigitalIdentityProvider != 0) {
                                assumeNonNull(e);
                                DigitalIdentityProviderJni.get()
                                        .onReceive(
                                                mDigitalIdentityProvider,
                                                "",
                                                "",
                                                DigitalIdentityProvider
                                                        .computeStatusForMetricsFromException(e));
                            }
                        });
    }

    /**
     * Triggers a create request to the Identity Credentials Manager in GMS.
     *
     * @param window The window associated with the request.
     * @param origin The origin of the requester.
     * @param request The request.
     */
    @CalledByNative
    void create(
            WindowAndroid window,
            @JniType("std::string") String origin,
            @JniType("std::string") String request) {
        sCredentials
                .create(window, origin, request)
                .then(
                        response -> {
                            if (mDigitalIdentityProvider != 0) {
                                DigitalIdentityProviderJni.get()
                                        .onReceive(
                                                mDigitalIdentityProvider,
                                                response.mProtocol,
                                                response.mData,
                                                DigitalIdentityRequestStatusForMetrics.SUCCESS);
                            }
                        },
                        e -> {
                            if (mDigitalIdentityProvider != 0) {
                                assumeNonNull(e);
                                DigitalIdentityProviderJni.get()
                                        .onReceive(
                                                mDigitalIdentityProvider,
                                                "",
                                                "",
                                                DigitalIdentityProvider
                                                        .computeStatusForMetricsFromException(e));
                            }
                        });
    }

    @NativeMethods
    interface Natives {
        void onReceive(
                long nativeDigitalIdentityProviderAndroid,
                @JniType("std::string") String protocol,
                @JniType("std::string") String digitalIdentity,
                @DigitalIdentityRequestStatusForMetrics int statusForMetrics);
    }
}
