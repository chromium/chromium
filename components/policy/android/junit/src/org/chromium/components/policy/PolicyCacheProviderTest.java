// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric test for PolicyCacheProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PolicyCacheProviderTest {
    private static final String POLICY_NAME_1 = "policy-name-1";
    private static final String POLICY_NAME_2 = "policy-name-2";
    private static final String POLICY_NAME_3 = "policy-name-3";
    private static final String POLICY_NAME_4 = "policy-name-4";

    private static final int INT_POLICY = 42;
    private static final boolean BOOLEAN_POLICY = true;
    private static final String STRING_POLICY = "policy-value";
    private static final String DICT_POLICY = "{\"test\" : 3}";

    private static final int SOURCE = 0;

    @Mock private CombinedPolicyProvider mCombinedPolicyProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testPolicyRefresh() {
        ContextUtils.getApplicationContext()
                .getSharedPreferences(PolicyCache.POLICY_PREF, Context.MODE_PRIVATE)
                .edit()
                .putInt(POLICY_NAME_1, INT_POLICY)
                .putBoolean(POLICY_NAME_2, BOOLEAN_POLICY)
                .putString(POLICY_NAME_3, STRING_POLICY)
                .putString(POLICY_NAME_4, DICT_POLICY)
                .apply();

        PolicyCacheProvider provider = new PolicyCacheProvider();
        provider.setManagerAndSource(mCombinedPolicyProvider, SOURCE);

        provider.refresh();

        verify(mCombinedPolicyProvider)
                .onSettingsAvailable(
                        eq(SOURCE),
                        argThat(
                                bundle -> {
                                    return bundle.size() == 4
                                            && bundle.getInt(POLICY_NAME_1) == INT_POLICY
                                            && bundle.getBoolean(POLICY_NAME_2) == BOOLEAN_POLICY
                                            && STRING_POLICY.equals(bundle.getString(POLICY_NAME_3))
                                            && DICT_POLICY.equals(bundle.getString(POLICY_NAME_4));
                                }));
    }

    @Test
    public void testEmpty() {
        PolicyCacheProvider provider = new PolicyCacheProvider();
        provider.setManagerAndSource(mCombinedPolicyProvider, SOURCE);
        provider.refresh();
        verifyNoInteractions(mCombinedPolicyProvider);
    }
}
