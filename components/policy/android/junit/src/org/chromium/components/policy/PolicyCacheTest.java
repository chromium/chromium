// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric test for PolicyCache.  */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class PolicyCacheTest {
    private static final String POLICY_NAME = "policy-name";

    private SharedPreferences mSharedPreferences;

    @Before
    public void setUp() {
        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                PolicyCache.POLICY_PREF, Context.MODE_PRIVATE);
        PolicyCache.resetSharedPreferencesForTests();
    }

    @After
    public void tearDown() {}

    @Test
    public void testGetInt() {
        Assert.assertNull(PolicyCache.getIntValue(POLICY_NAME));
        int expectedPolicyValue = 42;
        mSharedPreferences.edit().putInt(POLICY_NAME, expectedPolicyValue).apply();
        Assert.assertEquals(expectedPolicyValue, PolicyCache.getIntValue(POLICY_NAME).intValue());
    }

    @Test
    public void testGetBoolean() {
        Assert.assertNull(PolicyCache.getBooleanValue(POLICY_NAME));
        boolean expectedPolicyValue = true;
        mSharedPreferences.edit().putBoolean(POLICY_NAME, expectedPolicyValue).apply();
        Assert.assertEquals(
                expectedPolicyValue, PolicyCache.getBooleanValue(POLICY_NAME).booleanValue());
    }

    @Test
    public void testGetString() {
        Assert.assertNull(PolicyCache.getStringValue(POLICY_NAME));
        String expectedPolicyValue = "test-value";
        mSharedPreferences.edit().putString(POLICY_NAME, expectedPolicyValue).apply();
        Assert.assertEquals(expectedPolicyValue, PolicyCache.getStringValue(POLICY_NAME));
    }

    @Test
    public void testGetList() throws JSONException {
        Assert.assertNull(PolicyCache.getListValue(POLICY_NAME));
        String policyValue = "[42, \"test\", true]";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        JSONArray actualPolicyValue = PolicyCache.getListValue(POLICY_NAME);
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
        Assert.assertNull(PolicyCache.getListValue(POLICY_NAME));
    }

    @Test
    public void testGetDict() throws JSONException {
        Assert.assertNull(PolicyCache.getDictValue(POLICY_NAME));
        String policyValue = "{\"key1\":\"value1\", \"key2\":{\"a\":1, \"b\":2}}";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        JSONObject actualPolicyValue = PolicyCache.getDictValue(POLICY_NAME);
        Assert.assertNotNull(actualPolicyValue);
        Assert.assertEquals(2, actualPolicyValue.length());
        Assert.assertEquals("value1", actualPolicyValue.getString("key1"));
        Assert.assertEquals(1, actualPolicyValue.getJSONObject("key2").getInt("a"));
        Assert.assertEquals(2, actualPolicyValue.getJSONObject("key2").getInt("b"));
    }

    @Test
    public void testGetInvalidDict() throws JSONException {
        Assert.assertNull(PolicyCache.getDictValue(POLICY_NAME));
        String policyValue = "{\"key1\":\"value1\", \"key2\":{\"a\":1, \"b\":2}";
        mSharedPreferences.edit().putString(POLICY_NAME, policyValue).apply();
        Assert.assertNull(PolicyCache.getListValue(POLICY_NAME));
    }
}
