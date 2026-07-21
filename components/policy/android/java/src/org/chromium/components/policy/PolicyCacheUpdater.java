// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

import java.util.Arrays;
import java.util.List;

/** Helper class to allow native library to update policy cache with {@link PolicyCache} */
@JNINamespace("policy::android")
@NullMarked
public class PolicyCacheUpdater {
    // A list of policies that will be cached. Note that policy won't be cached in case of any error
    // including but not limited to one of following situations:
    //  1) Dangerous policy is ignored on non-fully managed devices.
    //  2) Policy is deprecated and overridden by its replacement.
    //  3) Any fatal error set by ConfigurationPolicyHandler.
    static List<Pair<String, @PolicyCache.Type Integer>> sPolicies =
            Arrays.asList(
                    Pair.create("BrowserSignin", PolicyCache.Type.INTEGER),
                    Pair.create("CloudManagementEnrollmentToken", PolicyCache.Type.STRING),
                    Pair.create("ChromeVariations", PolicyCache.Type.INTEGER),
                    Pair.create("SafeSitesFilterBehavior", PolicyCache.Type.INTEGER),
                    Pair.create("URLAllowlist", PolicyCache.Type.LIST),
                    Pair.create("URLBlocklist", PolicyCache.Type.LIST),
                    Pair.create("FirstPartySetsEnabled", PolicyCache.Type.BOOLEAN),
                    Pair.create("FirstPartySetsOverrides", PolicyCache.Type.DICT));

    @CalledByNative
    public static void cachePolicies(PolicyMap policyMap) {
        PolicyCache.get().cachePolicies(policyMap, sPolicies);
    }
}
