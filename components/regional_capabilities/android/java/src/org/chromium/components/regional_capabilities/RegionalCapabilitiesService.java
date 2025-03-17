// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.regional_capabilities;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Android wrapper of the RegionalCapabilitiesService which provides access from the Java layer.
 *
 * <p>Only usable from the UI thread.
 *
 * <p>See //components/regional_capabilities/regional_capabilities_service.h for more details.
 */
@JNINamespace("regional_capabilities")
@NullMarked
public class RegionalCapabilitiesService {

    private long mNativeService;

    /**
     * Creates an instance of the Java bridge to the C++ RegionalCapabilitiesService. Intended to be
     * only called from C++.
     */
    @CalledByNative
    @VisibleForTesting
    public RegionalCapabilitiesService(long nativeRegionalCapabilitiesService) {
        mNativeService = nativeRegionalCapabilitiesService;
    }

    @CalledByNative
    private void destroy() {
        mNativeService = 0;
    }

    /**
     * Returns whether the profile country is a EEA member. Uses the understanding of profile
     * country that is used for search engine choice screens and similar features, which might be
     * different than what LocaleUtils returns.
     *
     * <p>Testing note: To control the value this returns in manual or automated consider using the
     * {@code --search-engine-choice-country} command line flag and its special arguments, defined
     * in //components/regional_capabilities/regional_capabilities_switches.h
     */
    @MainThread
    public boolean isInEeaCountry() {
        ThreadUtils.assertOnUiThread();
        return RegionalCapabilitiesServiceJni.get().isInEeaCountry(mNativeService);
    }

    @NativeMethods
    public interface Natives {
        boolean isInEeaCountry(long nativeRegionalCapabilitiesService);
    }
}
