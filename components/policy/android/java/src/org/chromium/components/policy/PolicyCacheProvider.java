// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.os.Bundle;

import org.chromium.base.Log;

import java.util.Map;

/** {@link PolicyProvider} based on values from {@link PolicyCache}. */
public class PolicyCacheProvider extends PolicyProvider {
    private static final String TAG = "PolicyCacheProvider";

    private Bundle mSettings;

    @Override
    public void refresh() {
        if (mSettings != null) {
            // Cache the value in memory as |PolicyCache| becomes unavailable after the first round
            // of policy refresh.
            notifySettingsAvailable(mSettings);
            return;
        }
        Bundle settings = new Bundle();
        for (Map.Entry<String, ?> entry : PolicyCache.get().getAllPolicies().entrySet()) {
            String key = entry.getKey();
            Object value = entry.getValue();
            if (value instanceof Integer) {
                settings.putInt(key, ((Integer) value).intValue());
                continue;
            }
            if (value instanceof Boolean) {
                settings.putBoolean(key, ((Boolean) value).booleanValue());
                continue;
            }
            if (value instanceof String) {
                // This covers string policies and list/dict policies which will be converted
                // later.
                settings.putString(key, (String) value);
                continue;
            }

            assert false : "Invalid policy type from cache";
        }
        mSettings = settings;
        Log.i(TAG, "#refresh() " + mSettings.isEmpty());
        if (!mSettings.isEmpty()) {
            // There's a trade off between Java code correctness and native code correctness here.
            // The native code assumes that policies are ready immediately and does not wait. So
            // this class attempts to get cached polices to native as fast as possible to improve
            // correctness. However this decreases Java code correctness, as now the PolicyService
            // claims to be initialized before real app restrictions are applied. When the settings
            // are empty, there's no point in pushing these to native. And it's possible we're in a
            // first run scenario where there are no cached values and sending an early init signal
            // breaks certain policies.
            notifySettingsAvailable(settings);
        }
    }
}
