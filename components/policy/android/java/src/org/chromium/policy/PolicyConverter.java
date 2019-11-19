// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Arrays;
import java.util.Set;

/**
 * Allows converting Java policies, contained as key/value pairs in {@link android.os.Bundle}s to
 * native {@code PolicyBundle}s.
 *
 * This class is to be used to send key/value pairs to its native equivalent, that can then be used
 * to retrieve the native {@code PolicyBundle}.
 *
 * It should be created by calling {@link #create(long)} from the native code, and sending it back
 * to Java.
 */
@JNINamespace("policy::android")
public class PolicyConverter {
    private static final String TAG = "PolicyConverter";
    private long mNativePolicyConverter;

    private PolicyConverter(long nativePolicyConverter) {
        mNativePolicyConverter = nativePolicyConverter;
    }

    /** Convert and send the key/value pair for a policy to the native {@code PolicyConverter}. */
    public void setPolicy(String key, Object value) {
        assert mNativePolicyConverter != 0;

        if (value instanceof Boolean) {
            PolicyConverterJni.get().setPolicyBoolean(
                    mNativePolicyConverter, PolicyConverter.this, key, (Boolean) value);
            return;
        }
        if (value instanceof String) {
            PolicyConverterJni.get().setPolicyString(
                    mNativePolicyConverter, PolicyConverter.this, key, (String) value);
            return;
        }
        if (value instanceof Integer) {
            PolicyConverterJni.get().setPolicyInteger(
                    mNativePolicyConverter, PolicyConverter.this, key, (Integer) value);
            return;
        }
        if (value instanceof String[]) {
            PolicyConverterJni.get().setPolicyStringArray(
                    mNativePolicyConverter, PolicyConverter.this, key, (String[]) value);
            return;
        }
        // App restrictions can only contain bundles and bundle arrays on Android M, however our
        // version of Robolectric only supports Lollipop, and allowing this on LOLLIPOP doesn't
        // cause problems.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            if (value instanceof Bundle) {
                Bundle bundle = (Bundle) value;
                // JNI can't take a Bundle argument without a lot of extra work, but the native code
                // already accepts arbitrary JSON strings, so convert to JSON.
                try {
                    PolicyConverterJni.get().setPolicyString(mNativePolicyConverter,
                            PolicyConverter.this, key, convertBundleToJson(bundle).toString());
                } catch (JSONException e) {
                    // Chrome requires all policies to be expressible as JSON, so this can't be a
                    // valid policy.
                    Log.w(TAG, "Invalid bundle in app restrictions " + bundle.toString()
                                    + " for key " + key);
                }
                return;
            }
            if (value instanceof Bundle[]) {
                Bundle[] bundleArray = (Bundle[]) value;
                // JNI can't take a Bundle[] argument without a lot of extra work, but the native
                // code already accepts arbitrary JSON strings, so convert to JSON.
                try {
                    PolicyConverterJni.get().setPolicyString(mNativePolicyConverter,
                            PolicyConverter.this, key,
                            convertBundleArrayToJson(bundleArray).toString());
                } catch (JSONException e) {
                    // Chrome requires all policies to be expressible as JSON, so this can't be a
                    // valid policy.
                    Log.w(TAG, "Invalid bundle array in app restrictions "
                                    + Arrays.toString(bundleArray) + " for key " + key);
                }
                return;
            }
        }
        assert false : "Invalid setting " + value + " for key " + key;
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private JSONObject convertBundleToJson(Bundle bundle) throws JSONException {
        JSONObject json = new JSONObject();
        Set<String> keys = bundle.keySet();
        for (String key : keys) {
            Object value = bundle.get(key);
            if (value instanceof Bundle) value = convertBundleToJson((Bundle) value);
            if (value instanceof Bundle[]) value = convertBundleArrayToJson((Bundle[]) value);
            json.put(key, JSONObject.wrap(value));
        }
        return json;
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private JSONArray convertBundleArrayToJson(Bundle[] bundleArray) throws JSONException {
        JSONArray json = new JSONArray();
        for (Bundle bundle : bundleArray) {
            json.put(convertBundleToJson(bundle));
        }
        return json;
    }

    @VisibleForTesting
    @CalledByNative
    static PolicyConverter create(long nativePolicyConverter) {
        return new PolicyConverter(nativePolicyConverter);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePolicyConverter = 0;
    }

    @NativeMethods
    interface Natives {
        void setPolicyBoolean(long nativePolicyConverter, PolicyConverter caller, String policyKey,
                boolean value);
        void setPolicyInteger(
                long nativePolicyConverter, PolicyConverter caller, String policyKey, int value);
        void setPolicyString(
                long nativePolicyConverter, PolicyConverter caller, String policyKey, String value);
        void setPolicyStringArray(long nativePolicyConverter, PolicyConverter caller,
                String policyKey, String[] value);
    }
}
