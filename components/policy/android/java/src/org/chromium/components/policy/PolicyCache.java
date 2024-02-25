// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;

import java.util.List;
import java.util.Map;

/**
 * Manage policy cache that will be used during browser launch stage.
 *
 * Policy loading is async on Android and caching policy values makes them
 * available during launch stage even before native library is ready.
 */
public class PolicyCache {
    @VisibleForTesting static final String POLICY_PREF = "Components.Policy";

    private static PolicyCache sInstance;

    public enum Type {
        Integer,
        Boolean,
        String,
        List,
        Dict,
    }

    private boolean mReadable = true;

    private SharedPreferences mSharedPreferences;

    private ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    /**
     * Creates and returns SharedPreferences instance that is used to cache policy
     * value.
     *
     * @return The SharedPreferences instance that is used for policy caching. Returns null if
     *         application context is not available.
     */
    private SharedPreferences getSharedPreferences() {
        assert mReadable;
        mThreadChecker.assertOnValidThread();
        if (mSharedPreferences == null) {
            Context context = ContextUtils.getApplicationContext();
            // Policy cache is not accessiable without application context.
            if (context == null) return null;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mSharedPreferences =
                        context.getSharedPreferences(POLICY_PREF, Context.MODE_PRIVATE);
            }
        }
        return mSharedPreferences;
    }

    private SharedPreferences.Editor getSharedPreferencesEditor() {
        mThreadChecker.assertOnValidThread();
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getApplicationContext()
                    .getSharedPreferences(POLICY_PREF, Context.MODE_PRIVATE)
                    .edit();
        }
    }

    public static PolicyCache get() {
        var ret = sInstance;
        if (ret == null) {
            ret = new PolicyCache();
            sInstance = ret;
            ResettersForTesting.register(() -> sInstance = null);
        }
        return ret;
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached integer policy, null if there is no valid
     * cached policy.
     */
    public Integer getIntValue(String policy) {
        SharedPreferences sharedPreferences = getSharedPreferences();
        if (sharedPreferences == null) return null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!sharedPreferences.contains(policy)) return null;
            return sharedPreferences.getInt(policy, 0);
        }
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached boolean policy, null if there is no valid
     * cached policy.
     */
    public Boolean getBooleanValue(String policy) {
        SharedPreferences sharedPreferences = getSharedPreferences();
        if (sharedPreferences == null) return null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!sharedPreferences.contains(policy)) return null;
            return sharedPreferences.getBoolean(policy, false);
        }
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached string policy, null if there is no valid
     * cached policy.
     */
    public String getStringValue(String policy) {
        SharedPreferences sharedPreferences = getSharedPreferences();
        if (sharedPreferences == null) return null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!sharedPreferences.contains(policy)) return null;
            return sharedPreferences.getString(policy, null);
        }
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached list policy, null if there is no valid
     * cached policy.
     */
    public JSONArray getListValue(String policy) {
        SharedPreferences sharedPreferences = getSharedPreferences();
        if (sharedPreferences == null) return null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!sharedPreferences.contains(policy)) return null;
            try {
                return new JSONArray(sharedPreferences.getString(policy, null));
            } catch (JSONException e) {
                return null;
            }
        }
    }

    /**
     * @param policy The name of policy.
     * @return The value of cached dictionary policy, null if there is no valid
     * cached policy.
     */
    public JSONObject getDictValue(String policy) {
        SharedPreferences sharedPreferences = getSharedPreferences();
        if (sharedPreferences == null) return null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!sharedPreferences.contains(policy)) return null;
            try {
                return new JSONObject(sharedPreferences.getString(policy, null));
            } catch (JSONException e) {
                return null;
            }
        }
    }

    /** @return All cached policies. */
    public Map<String, ?> getAllPolicies() {
        SharedPreferences sharedPreferences = getSharedPreferences();
        if (sharedPreferences == null) return null;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return sharedPreferences.getAll();
        }
    }

    public boolean isReadable() {
        return mReadable;
    }

    /**
     * @param policyMap The latest policy value bundle.
     * @param policyNames The list of policies that needs to be cached if available.
     * Caches the policies that are available in both |policyNames| and
     * |policyMap|. It also disables {@link PolicyCache} reading.
     */
    public void cachePolicies(PolicyMap policyMap, List<Pair<String, Type>> policyNames) {
        // TODO(zmin): support policy level while caching policy.
        SharedPreferences.Editor sharedPreferencesEditor = getSharedPreferencesEditor();

        sharedPreferencesEditor.clear();

        for (Pair<String, Type> policy : policyNames) {
            String policyName = policy.first;
            switch (policy.second) {
                case Integer:
                    {
                        Integer value = policyMap.getIntValue(policyName);
                        if (value != null) {
                            sharedPreferencesEditor.putInt(policyName, value.intValue());
                        }
                        break;
                    }
                case Boolean:
                    {
                        Boolean value = policyMap.getBooleanValue(policyName);
                        if (value != null) {
                            sharedPreferencesEditor.putBoolean(policyName, value.booleanValue());
                        }
                        break;
                    }
                case String:
                    {
                        String value = policyMap.getStringValue(policyName);
                        if (value != null) {
                            sharedPreferencesEditor.putString(policyName, value);
                        }
                        break;
                    }
                    // List and Dict policy values are stored in the native library
                    // as base::Value and converted to JSON string to passed through
                    // the JNI. It's stored to the SharedPreferences as String and
                    // will be converted to JSON object when being read.
                case List:
                    {
                        String value = policyMap.getListValueAsString(policyName);
                        if (value != null) {
                            sharedPreferencesEditor.putString(policyName, value);
                        }
                        break;
                    }
                case Dict:
                    {
                        String value = policyMap.getDictValueAsString(policyName);
                        if (value != null) {
                            sharedPreferencesEditor.putString(policyName, value);
                        }
                        break;
                    }
            }
        }

        // Update sharedPreferences. The first round of updating during launch
        // will use the main thread.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            sharedPreferencesEditor.apply();
        }

        // Policy Service is up and there is no need to get policy from here anymore.
        enableWriteOnlyMode();
    }

    /** Delete all entries from the cache. */
    public void reset() {
        SharedPreferences.Editor sharedPreferencesEditor = getSharedPreferencesEditor();
        sharedPreferencesEditor.clear();
        sharedPreferencesEditor.apply();
    }

    private void enableWriteOnlyMode() {
        mSharedPreferences = null;
        mReadable = false;
    }

    public void setReadableForTesting(boolean readable) {
        mReadable = readable;
    }
}
