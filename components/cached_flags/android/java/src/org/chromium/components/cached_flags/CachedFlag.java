// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureMap;
import org.chromium.base.Flag;
import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * CachedFlags are Flags that may be used before native is loaded and the FeatureList is
 * initialized.
 *
 * <p>They return a flag value read from native in a previous run, using SharedPreferences as
 * persistence.
 *
 * <p>@see {@link #isEnabled()} for more details about the logic.
 *
 * <p>To cache a flag from a {@link FeatureMap}, e.g. FooFeatureMap:
 *
 * <ul>
 *   <li>Create a static CachedFlag object in FooFeatureMap "sMyFlag"
 *   <li>Add it to the list FooFeatureMap#sFlagsCachedFullBrowser
 *   <li>Call {@code FooFeatureMap.sMyFlag.isEnabled()} to query whether the cached flag is enabled.
 *       Consider this the source of truth for whether the flag is turned on in the current session.
 * </ul>
 *
 * <p>Metrics caveat: For cached flags that are queried before native is initialized, when a new
 * experiment configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart.
 */
public class CachedFlag extends Flag {
    private final boolean mDefaultValue;
    private String mPreferenceKey;
    private Supplier<Boolean> mValueSupplier;

    public CachedFlag(FeatureMap featureMap, String featureName, boolean defaultValue) {
        super(featureMap, featureName);
        mDefaultValue = defaultValue;
    }

    /**
     * Constructor to use when a |defaultValueInTests| is specified to mimic reading from
     * field_trial_config.json.
     */
    public CachedFlag(
            FeatureMap featureMap,
            String featureName,
            boolean defaultValue,
            boolean defaultValueInTests) {
        super(featureMap, featureName);
        mDefaultValue = BuildConfig.IS_FOR_TEST ? defaultValueInTests : defaultValue;
    }

    /**
     * Rules from highest to lowest priority:
     *
     * <ul>
     *   <li>1. If the flag has been forced by @EnableFeatures/@DisableFeatures or {@link
     *       CachedFlag#setForTesting}, the forced value is returned.
     *   <li>2. If a value was previously returned in the same run, the same value is returned for
     *       consistency.
     *   <li>3. If Safe Mode has activated, use a safe value.
     *   <li>4. If in a previous run, the value from {@link FeatureMap} was cached to SharedPrefs,
     *       it is returned.
     *   <li>5. The |defaultValue| passed as a constructor parameter is returned.
     * </ul>
     */
    @Override
    public boolean isEnabled() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        return ValuesReturned.getReturnedOrNewBoolValue(
                getSharedPreferenceKey(), getValueSupplier());
    }

    private Supplier<Boolean> getValueSupplier() {
        if (mValueSupplier == null) {
            mValueSupplier =
                    () -> {
                        String preferenceName = getSharedPreferenceKey();
                        Boolean flag =
                                CachedFlagsSafeMode.getInstance()
                                        .isEnabled(mFeatureName, preferenceName, mDefaultValue);
                        if (flag == null) {
                            SharedPreferencesManager prefs =
                                    CachedFlagsSharedPreferences.getInstance();
                            if (prefs.contains(preferenceName)) {
                                flag = prefs.readBoolean(preferenceName, false);
                            } else {
                                flag = mDefaultValue;
                            }
                        }

                        return flag;
                    };
        }
        return mValueSupplier;
    }

    /**
     * @return the default value to be returned if no value is cached.
     */
    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        // ValuesReturned is cleared by CachedFlagUtils#resetFlagsForTesting().
    }

    /**
     * Forces a feature to be enabled or disabled for testing.
     *
     * @deprecated do not call this from tests; use @EnableFeatures/@DisableFeatures instead, since
     *     batched tests need to be split by feature flag configuration.
     */
    @VisibleForTesting
    @Deprecated
    public void setForTesting(@Nullable Boolean value) {
        ValuesReturned.setFeaturesForTesting(Collections.singletonMap(getFeatureName(), value));
    }

    /**
     * Writes the value of the feature from {@link FeatureMap} to the provided SharedPrefs Editor
     * for caching. Does not apply or commit the change - that is left up to the caller to perform.
     */
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final boolean isEnabledInNative = mFeatureMap.isEnabledInNative(mFeatureName);
        editor.putBoolean(getSharedPreferenceKey(), isEnabledInNative);
    }

    String getSharedPreferenceKey() {
        // Create the key only once to avoid String concatenation every flag check.
        if (mPreferenceKey == null) {
            mPreferenceKey = CachedFlagsSharedPreferences.FLAGS_CACHED.createKey(mFeatureName);
        }
        return mPreferenceKey;
    }

    /** Create a Map of feature names -> {@link CachedFlag} from multiple lists of CachedFlags. */
    public static Map<String, CachedFlag> createCachedFlagMap(
            List<List<CachedFlag>> allCachedFlagsLists) {
        HashMap<String, CachedFlag> cachedFlagMap = new HashMap<>();
        for (List<CachedFlag> cachedFlagsList : allCachedFlagsLists) {
            for (CachedFlag cachedFlag : cachedFlagsList) {
                cachedFlagMap.put(cachedFlag.getFeatureName(), cachedFlag);
            }
        }
        return cachedFlagMap;
    }
}
