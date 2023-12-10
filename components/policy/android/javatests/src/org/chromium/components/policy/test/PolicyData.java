// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy.test;

import android.os.Bundle;

import org.json.JSONArray;
import org.junit.Assert;

import org.chromium.base.Log;

/**
 * Helper class to transform Java types to {@link Bundle}s usable by the Policy system.
 *
 * Use the subclasses to define the data and then transform it using {@link #asBundle(Iterable)}
 */
public abstract class PolicyData {
    private static final String TAG = "policy_test";
    private final String mKey;

    public PolicyData(String key) {
        mKey = key;
    }

    public String getKey() {
        return mKey;
    }

    public abstract void putInBundle(Bundle bundle);

    public static Bundle asBundle(Iterable<PolicyData> policies) {
        Bundle bundle = new Bundle();
        for (PolicyData data : policies) {
            Log.d(TAG, "Adding to policy bundle: %s", data);
            data.putInBundle(bundle);
        }
        return bundle;
    }

    /** {@link PolicyData} for the {@link String} type. */
    public static class Str extends PolicyData {
        private final String mValue;

        public Str(String key, String value) {
            super(key);
            mValue = value;
        }

        public String getValue() {
            return mValue;
        }

        @Override
        public void putInBundle(Bundle bundle) {
            bundle.putString(getKey(), mValue);
        }

        @Override
        public String toString() {
            return String.format("PolicyData.Str{%s=%s}", getKey(), mValue);
        }
    }

    /** {@link PolicyData} with no value, for error states. Doesn't put anything in a bundle.*/
    public static class Undefined extends PolicyData {
        public Undefined(String key) {
            super(key);
        }

        @Override
        public void putInBundle(Bundle bundle) {
            Assert.fail(
                    String.format(
                            "Attempted to push the '%s' policy without value to a bundle.",
                            getKey()));
        }

        @Override
        public String toString() {
            return String.format("PolicyData.Undefined{%s}", getKey());
        }
    }

    /**
     * {@link PolicyData} for the {@link String} array type.
     * Outputs a string encoded as a JSON array.
     */
    public static class StrArray extends PolicyData {
        private final String[] mValue;

        public StrArray(String key, String[] value) {
            super(key);
            mValue = value.clone();
        }

        public String[] getValue() {
            return mValue.clone();
        }

        private String valueToString() {
            // JSONArray(Object[]) requires API 19
            JSONArray array = new JSONArray();
            for (String s : mValue) {
                array.put(s);
            }
            return array.toString();
        }

        @Override
        public void putInBundle(Bundle bundle) {
            bundle.putString(getKey(), valueToString());
        }

        @Override
        public String toString() {
            return String.format("PolicyData.StrArray{%s=%s}", getKey(), valueToString());
        }
    }
}
