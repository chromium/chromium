// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.one_time_tokens.backend.sms;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This factory returns an implementation for the otp fetcher. The factory itself is also
 * implemented downstream.
 */
@NullMarked
public abstract class AndroidSmsOtpFetcherFactory {
    private static @Nullable AndroidSmsOtpFetcherFactory sInstance;

    /**
     * Returns a factory to be invoked whenever {@link #createSmsOtpFetcher()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link AndroidSmsOtpFetcherFactory} instance.
     */
    public static AndroidSmsOtpFetcherFactory getInstance() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(AndroidSmsOtpFetcherFactory.class);
        }
        if (sInstance == null) {
            sInstance = new AndroidSmsOtpFetcherFactoryUpstreamImpl();
        }
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link AndroidSmsOtpFetcher} if one exists.
     */
    public @Nullable AndroidSmsOtpFetcher createSmsOtpFetcher() {
        return null;
    }

    public static void setFactoryForTesting( // IN-TEST
            AndroidSmsOtpFetcherFactory androidSmsOtpFetcherFactory) {
        var oldValue = sInstance;
        sInstance = androidSmsOtpFetcherFactory;
        ResettersForTesting.register(() -> sInstance = oldValue); // IN-TEST
    }
}
