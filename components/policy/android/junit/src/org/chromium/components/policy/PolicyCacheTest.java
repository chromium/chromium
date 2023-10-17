// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Pair;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Robolectric test for PolicyCache. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class PolicyCacheTest {
    private static final String POLICY_NAME = "policy-name";
    private static final String POLICY_NAME_2 = "policy-name-2";
    private static final String POLICY_NAME_3 = "policy-name-3";
    private static final String POLICY_NAME_4 = "policy-name-4";
    private static final String POLICY_NAME_5 = "policy-name-5";

    private SharedPreferences mSharedPreferences;

    private PolicyCache mPolicyCache;

    @Mock private PolicyMap mPolicyMap;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPolicyCache = PolicyCache.get();
        mSharedPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(PolicyCache.POLICY_PREF, Context.MODE_PRIVATE);

        initPolicyMap();
    }

    private void initPolicyMap() {
        when(mPolicyMap.getIntValue(anyString())).thenReturn(null);
        when(mPolicyMap.getBooleanValue(anyString())).thenReturn(null);
        when(mPolicyMap.getStringValue(anyString())).thenReturn(null);
        when(mPolicyMap.getListValueAsString(anyString())).thenReturn(null);
        when(mPolicyMap.getDictValueAsString(anyString())).thenReturn(null);
    }

    @Test
    public void testGetInt() {
        Assert.assertNull(mPolicyCache.getIntValue(POLICY_NAME));
        int expectedPolicyValue = 42;
        mSharedPreferences.edit().putInt(POLICY_NAME, expectedPolicyValue).apply();
        Assert.assertEquals(expectedPolicyValue, mPolicyCache.getIntValue(POLICY_NAME).intValue());
    }

    @Test
    public void testGetBoolean() {
        Assert.assertNull(mPolicyCache.getBooleanValue(POLICY_NAME));
        boolean expectedPolicyValue = true;
        mSharedPreferences.edit().putBoolean(POLICY_NAME, expectedPolicyValue).apply();
        Assert.assertEquals(
                expectedPolicyValue, mPolicyCache.getBooleanValue(POLICY_NAME).booleanValue());
    }

    @Test
    public void testGetString() {
        Assert.assertNull(mPolicyCache.getStringValue(POLICY_NAME));
        String expectedPolicyValue = "test-value";
        mSharedPreferences.edit().putString(POLICY_NAME, expectedPolicyValue).apply();
        Assert.assertEquals(expectedPolicyValue, mPolicyCache.getStringValue(POLICY_NAME));
    }

    @Test
    public void testGetList() throws JSONException {
        Assert.assertNull(mPolicyCache.getListValue(POLICY_NAME));
        String policyValue = "[42, \"test\", true]";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        JSONArray actualPolicyValue = mPolicyCache.getListValue(POLICY_NAME);
        Assert.assertNotNull(actualPolicyValue);
        Assert.assertEquals(3, actualPolicyValue.length());
        Assert.assertEquals(42, actualPolicyValue.getInt(0));
        Assert.assertEquals("test", actualPolicyValue.getString(1));
        Assert.assertEquals(true, actualPolicyValue.getBoolean(2));
    }

    @Test
    public void testGetInvalidList() throws JSONException {
        String policyValue = "[42, \"test\"";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        Assert.assertNull(mPolicyCache.getListValue(POLICY_NAME));
    }

    @Test
    public void testGetDict() throws JSONException {
        Assert.assertNull(mPolicyCache.getDictValue(POLICY_NAME));
        String policyValue = "{\"key1\":\"value1\", \"key2\":{\"a\":1, \"b\":2}}";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        JSONObject actualPolicyValue = mPolicyCache.getDictValue(POLICY_NAME);
        Assert.assertNotNull(actualPolicyValue);
        Assert.assertEquals(2, actualPolicyValue.length());
        Assert.assertEquals("value1", actualPolicyValue.getString("key1"));
        Assert.assertEquals(1, actualPolicyValue.getJSONObject("key2").getInt("a"));
        Assert.assertEquals(2, actualPolicyValue.getJSONObject("key2").getInt("b"));
    }

    @Test
    public void testGetInvalidDict() throws JSONException {
        Assert.assertNull(mPolicyCache.getDictValue(POLICY_NAME));
        String policyValue = "{\"key1\":\"value1\", \"key2\":{\"a\":1, \"b\":2}";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        Assert.assertNull(mPolicyCache.getListValue(POLICY_NAME));
    }

    @Test
    public void testCachePolicies() {
        cachePolicies(
                CollectionUtil.newHashMap(
                        Pair.create(POLICY_NAME, Pair.create(PolicyCache.Type.Integer, 1)),
                        Pair.create(POLICY_NAME_2, Pair.create(PolicyCache.Type.Boolean, true)),
                        Pair.create(POLICY_NAME_3, Pair.create(PolicyCache.Type.String, "2")),
                        Pair.create(POLICY_NAME_4, Pair.create(PolicyCache.Type.List, "[1]")),
                        Pair.create(POLICY_NAME_5, Pair.create(PolicyCache.Type.Dict, "{1:2}"))));

        Assert.assertEquals(1, mSharedPreferences.getInt(POLICY_NAME, 0));
        Assert.assertEquals(true, mSharedPreferences.getBoolean(POLICY_NAME_2, false));
        Assert.assertEquals("2", mSharedPreferences.getString(POLICY_NAME_3, null));
        Assert.assertEquals("[1]", mSharedPreferences.getString(POLICY_NAME_4, null));
        Assert.assertEquals("{1:2}", mSharedPreferences.getString(POLICY_NAME_5, null));
    }

    @Test
    public void testCacheUpdated() {
        cachePolicies(
                CollectionUtil.newHashMap(
                        Pair.create(POLICY_NAME, Pair.create(PolicyCache.Type.Integer, 1))));
        cachePolicies(
                CollectionUtil.newHashMap(
                        Pair.create(POLICY_NAME_2, Pair.create(PolicyCache.Type.Boolean, true))));

        Assert.assertFalse(mSharedPreferences.contains(POLICY_NAME));
        Assert.assertEquals(true, mSharedPreferences.getBoolean(POLICY_NAME_2, false));
    }

    @Test
    public void testNotCachingUnnecessaryPolicy() {
        when(mPolicyMap.getIntValue(eq(POLICY_NAME))).thenReturn(1);
        when(mPolicyMap.getBooleanValue(eq(POLICY_NAME_2))).thenReturn(true);

        mPolicyCache.cachePolicies(
                mPolicyMap, Arrays.asList(Pair.create(POLICY_NAME_2, PolicyCache.Type.Boolean)));

        Assert.assertFalse(mSharedPreferences.contains(POLICY_NAME));
        Assert.assertEquals(true, mSharedPreferences.getBoolean(POLICY_NAME_2, false));
    }

    @Test
    public void testNotCachingUnavailablePolicy() {
        when(mPolicyMap.getBooleanValue(eq(POLICY_NAME_2))).thenReturn(true);

        mPolicyCache.cachePolicies(
                mPolicyMap,
                Arrays.asList(
                        Pair.create(POLICY_NAME, PolicyCache.Type.Integer),
                        Pair.create(POLICY_NAME_2, PolicyCache.Type.Boolean)));

        Assert.assertFalse(mSharedPreferences.contains(POLICY_NAME));
        Assert.assertEquals(true, mSharedPreferences.getBoolean(POLICY_NAME_2, false));
    }

    @Test
    public void testWriteOnlyAfterCacheUpdate() {
        mSharedPreferences
                .edit()
                .putInt(POLICY_NAME, 1)
                .putBoolean(POLICY_NAME_2, true)
                .putString(POLICY_NAME_3, "a")
                .putString(POLICY_NAME_4, "[1]")
                .putString(POLICY_NAME_5, "{1:2}")
                .apply();
        Assert.assertTrue(mPolicyCache.isReadable());

        cachePolicies(
                CollectionUtil.newHashMap(
                        Pair.create(POLICY_NAME, Pair.create(PolicyCache.Type.Integer, 1)),
                        Pair.create(POLICY_NAME_2, Pair.create(PolicyCache.Type.Boolean, true)),
                        Pair.create(POLICY_NAME_3, Pair.create(PolicyCache.Type.String, "2")),
                        Pair.create(POLICY_NAME_4, Pair.create(PolicyCache.Type.List, "[1]")),
                        Pair.create(POLICY_NAME_5, Pair.create(PolicyCache.Type.Dict, "{1:2}"))));

        Assert.assertFalse(mPolicyCache.isReadable());
        if (BuildConfig.ENABLE_ASSERTS) {
            assertAssertionError(() -> mPolicyCache.getIntValue(POLICY_NAME));
            assertAssertionError(() -> mPolicyCache.getBooleanValue(POLICY_NAME_2));
            assertAssertionError(() -> mPolicyCache.getStringValue(POLICY_NAME_3));
            assertAssertionError(() -> mPolicyCache.getListValue(POLICY_NAME_4));
            assertAssertionError(() -> mPolicyCache.getDictValue(POLICY_NAME_5));
        }
    }

    /**
     * @param policies A Map for policies that needs to be cached. Each policy name is mapped to a
     *     pair of policy type and policy value. Setting up {@link #mPolicyCache} mock and call
     *     {@link PolicyCache#cachePolicies}.
     */
    private void cachePolicies(Map<String, Pair<PolicyCache.Type, Object>> policies) {
        List<Pair<String, PolicyCache.Type>> cachedPolicies = new ArrayList();
        for (var entry : policies.entrySet()) {
            String policyName = entry.getKey();
            PolicyCache.Type policyType = entry.getValue().first;
            Object policyValue = entry.getValue().second;
            switch (policyType) {
                case Integer:
                    when(mPolicyMap.getIntValue(eq(policyName))).thenReturn((Integer) policyValue);
                    break;
                case Boolean:
                    when(mPolicyMap.getBooleanValue(eq(policyName)))
                            .thenReturn((Boolean) policyValue);
                    break;
                case String:
                    when(mPolicyMap.getStringValue(eq(policyName)))
                            .thenReturn((String) policyValue);
                    break;
                case List:
                    when(mPolicyMap.getListValueAsString(eq(policyName)))
                            .thenReturn((String) policyValue);
                    break;
                case Dict:
                    when(mPolicyMap.getDictValueAsString(eq(policyName)))
                            .thenReturn((String) policyValue);
                    break;
            }
            cachedPolicies.add(Pair.create(policyName, policyType));
        }
        mPolicyCache.cachePolicies(mPolicyMap, cachedPolicies);
    }

    private void assertAssertionError(Runnable runnable) {
        AssertionError assertionError = null;
        try {
            runnable.run();
        } catch (AssertionError e) {
            assertionError = e;
        }

        Assert.assertNotNull("AssertionError not thrown", assertionError);
    }
}
