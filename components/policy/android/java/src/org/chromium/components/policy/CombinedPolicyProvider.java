// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Reads enterprise policies from one or more policy providers and plumbs them through to the policy
 * subsystem.
 */
@JNINamespace("policy::android")
public class CombinedPolicyProvider {
    private static final String TAG = "CombinedPProvider";

    private static CombinedPolicyProvider sInstance;

    private long mNativeCombinedPolicyProvider;

    private PolicyConverter mPolicyConverter;
    private PolicyCacheProvider mPolicyCacheProvider;
    private final List<PolicyProvider> mPolicyProviders = new ArrayList<>();
    private final List<Bundle> mCachedPolicies = new ArrayList<>();
    private final List<PolicyChangeListener> mPolicyChangeListeners = new ArrayList<>();

    public static CombinedPolicyProvider get() {
        if (sInstance == null) {
            sInstance = new CombinedPolicyProvider();
        }
        return sInstance;
    }

    private void linkNativeInternal(
            long nativeCombinedPolicyProvider, PolicyConverter policyConverter) {
        mNativeCombinedPolicyProvider = nativeCombinedPolicyProvider;
        mPolicyConverter = policyConverter;
        if (nativeCombinedPolicyProvider == 0) {
            return;
        }

        Log.i(TAG, "#linkNativeInternal() " + mPolicyProviders.size());

        if (mPolicyProviders.isEmpty()) {
            mPolicyCacheProvider = new PolicyCacheProvider();
            mPolicyCacheProvider.setManagerAndSource(this, /* source= */ 0);
        }
        refreshPolicies();
    }

    @CalledByNative
    public static CombinedPolicyProvider linkNative(
            long nativeCombinedPolicyProvider, PolicyConverter policyConverter) {
        ThreadUtils.assertOnUiThread();
        get().linkNativeInternal(nativeCombinedPolicyProvider, policyConverter);
        return get();
    }

    /**
     * PolicyProviders are assigned a unique precedence based on their order of registration. Later
     * Registration -> Higher Precedence. This precedence is also used as a 'source' tag for
     * disambiguating updates.
     */
    public void registerProvider(PolicyProvider provider) {
        Log.i(
                TAG,
                "#registerProvider() provider:"
                        + provider
                        + " isPolicyCacheEnabled:"
                        + isPolicyCacheEnabled()
                        + " policyProvidersSize:"
                        + mPolicyProviders.size());
        if (isPolicyCacheEnabled()) {
            mPolicyCacheProvider = null;
        }

        mPolicyProviders.add(provider);
        mCachedPolicies.add(null);
        provider.setManagerAndSource(this, mPolicyProviders.size() - 1);
        if (mNativeCombinedPolicyProvider != 0) provider.refresh();
    }

    public void destroy() {
        // All the activities registered should have been destroyed by then.
        assert mPolicyChangeListeners.isEmpty();

        for (PolicyProvider provider : mPolicyProviders) {
            provider.destroy();
        }
        mPolicyProviders.clear();
        mPolicyConverter = null;
    }

    void onSettingsAvailable(int source, Bundle newSettings) {
        Log.i(TAG, "#onSettingsAvailable() " + source);
        if (mNativeCombinedPolicyProvider == 0) return;

        List<Bundle> policies;
        if (isPolicyCacheEnabled()) {
            policies = Arrays.asList(newSettings);
        } else {
            mCachedPolicies.set(source, newSettings);
            // Check if we have policies from all the providers before applying them.
            for (Bundle settings : mCachedPolicies) {
                if (settings == null) return;
            }

            policies = mCachedPolicies;
        }
        for (Bundle settings : policies) {
            for (String key : settings.keySet()) {
                Log.i(TAG, "#setPolicy() " + key + " -> " + settings.get(key));
                mPolicyConverter.setPolicy(key, settings.get(key));
            }
        }
        Log.i(TAG, "#flushPolicies()");
        CombinedPolicyProviderJni.get().flushPolicies(mNativeCombinedPolicyProvider, get());
    }

    void terminateIncognitoSession() {
        for (PolicyChangeListener listener : mPolicyChangeListeners) {
            listener.terminateIncognitoSession();
        }
    }

    public void addPolicyChangeListener(PolicyChangeListener listener) {
        mPolicyChangeListeners.add(listener);
    }

    public void removePolicyChangeListener(PolicyChangeListener listener) {
        assert mPolicyChangeListeners.contains(listener);
        mPolicyChangeListeners.remove(listener);
    }

    @VisibleForTesting
    @CalledByNative
    public void refreshPolicies() {
        if (isPolicyCacheEnabled()) {
            mPolicyCacheProvider.refresh();
            return;
        }

        assert mPolicyProviders.size() == mCachedPolicies.size();
        for (int i = 0; i < mCachedPolicies.size(); ++i) {
            mCachedPolicies.set(i, null);
        }
        for (PolicyProvider provider : mPolicyProviders) {
            provider.refresh();
        }
    }

    List<PolicyProvider> getPolicyProvidersForTesting() {
        return mPolicyProviders;
    }

    @VisibleForTesting
    boolean isPolicyCacheEnabled() {
        return mPolicyCacheProvider != null;
    }

    /** Interface to handle actions related with policy changes. */
    public interface PolicyChangeListener {
        /** Call to notify the listener that incognito browsing is unavailable due to policy. */
        void terminateIncognitoSession();
    }

    static void setForTesting(CombinedPolicyProvider p) {
        var oldValue = sInstance;
        sInstance = p;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    @NativeMethods
    interface Natives {
        void flushPolicies(long nativeAndroidCombinedPolicyProvider, CombinedPolicyProvider caller);
    }
}
