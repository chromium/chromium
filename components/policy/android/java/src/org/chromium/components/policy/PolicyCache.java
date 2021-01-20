// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;

/**
 * Manage policy cache that will be used during browser launch stage.
 *
 * Policy loading is async on Android and caching policy values makes them
 * available during launch stage even before native library is ready.
 */
public class PolicyCache {
    @VisibleForTesting
    static final String POLICY_PREF = "Components.Policy";

    private static SharedPreferences sSharedPreferences;

    /**
     * Creates and returns SharedPreferences instance that is used to cache policy
     * value.
     *
     * @return The SharedPreferences instance that is used for policy caching. Returns null if
     *         application context is not available.
     */
    private static SharedPreferences getSharedPreferences() {
        if (sSharedPreferences == null) {
            Context context = ContextUtils.getApplicationContext();
            // Policy cache is not accessiable without application context.
            if (context == null) return null;
            sSharedPreferences = context.getSharedPreferences(POLICY_PREF, Context.MODE_PRIVATE);
        }
        return sSharedPreferences;
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached integer policy, null if there is no valid
     * cached policy.
     */
    public static Integer getIntValue(String policy) {
        SharedPreferences sSharedPreferences = getSharedPreferences();
        if (sSharedPreferences == null) return null;
        if (!sSharedPreferences.contains(policy)) return null;
        return sSharedPreferences.getInt(policy, 0);
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached boolean policy, null if there is no valid
     * cached policy.
     */
    public static Boolean getBooleanValue(String policy) {
        SharedPreferences sSharedPreferences = getSharedPreferences();
        if (sSharedPreferences == null) return null;
        if (!sSharedPreferences.contains(policy)) return null;
        return sSharedPreferences.getBoolean(policy, false);
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached string policy, null if there is no valid
     * cached policy.
     */
    public static String getStringValue(String policy) {
        SharedPreferences sSharedPreferences = getSharedPreferences();
        if (sSharedPreferences == null) return null;
        if (!sSharedPreferences.contains(policy)) return null;
        return sSharedPreferences.getString(policy, null);
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached list policy, null if there is no valid
     * cached policy.
     */
    public static JSONArray getListValue(String policy) {
        SharedPreferences sSharedPreferences = getSharedPreferences();
        if (sSharedPreferences == null) return null;
        if (!sSharedPreferences.contains(policy)) return null;
        try {
            return new JSONArray(sSharedPreferences.getString(policy, null));
        } catch (JSONException e) {
            return null;
        }
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached dictionary policy, null if there is no valid
     * cached policy.
     */
    public static JSONObject getDictValue(String policy) {
        SharedPreferences sSharedPreferences = getSharedPreferences();
        if (sSharedPreferences == null) return null;
        if (!sSharedPreferences.contains(policy)) return null;
        try {
            return new JSONObject(sSharedPreferences.getString(policy, null));
        } catch (JSONException e) {
            return null;
        }
    }

    @VisibleForTesting
    static void resetSharedPreferencesForTests() {
        sSharedPreferences = null;
    }
}
