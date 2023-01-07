// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.os.Bundle;

import java.util.Map;

/**
 * {@link PolicyProvider} based on values from {@link PolicyCache}.
 */
public class PolicyCacheProvider extends PolicyProvider {
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
        notifySettingsAvailable(settings);
    }
}
