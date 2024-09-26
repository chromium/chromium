// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;

/**
 * This class is responsible for persisting subscription specific flags to make them available
 * before native has loaded.
 */
public class SubscriptionFlagManager {
    private static final String PREF_PACKAGE =
            "org.chromium.components.gcm_driver.subscription_flags";

    // Private constructor because all methods in this class are static, and it
    // shouldn't be instantiated.
    private SubscriptionFlagManager() {}

    /**
     * Given an appId and a senderId, this methods builds a unique identifier for a subscription.
     * Currently implementation concatenates both senderId and appId.
     *
     * @return The unique identifier for the subscription.
     */
    public static String buildSubscriptionUniqueId(final String appId, final String senderId) {
        return appId + senderId;
    }

    /** Stores the flags for a |subscriptionId| in SharedPreferences. */
    public static void setFlags(final String subscriptionId, int flags) {
        if (flags == 0) {
            clearFlags(subscriptionId);
            return;
        }
        ContextUtils.getApplicationContext()
                .getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE)
                .edit()
                .putInt(subscriptionId, flags)
                .apply();
    }

    /** Removes flags for |subscriptionId| from SharedPreferences. */
    public static void clearFlags(final String subscriptionId) {
        ContextUtils.getApplicationContext()
                .getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE)
                .edit()
                .remove(subscriptionId)
                .apply();
    }

    /** Returns whether the subscription with |subscriptionId| has all |flags|. */
    public static boolean hasFlags(final String subscriptionId, int flags) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            int subscriptionFlags =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE)
                            .getInt(subscriptionId, 0);
            return (subscriptionFlags & flags) == flags;
        }
    }
}
