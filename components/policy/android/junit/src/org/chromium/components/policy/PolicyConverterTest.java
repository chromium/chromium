// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import static org.mockito.Mockito.verify;

import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Robolectric test for AbstractAppRestrictionsProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PolicyConverterTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock private PolicyConverter.Natives mPolicyConverterJniMock;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(PolicyConverterJni.TEST_HOOKS, mPolicyConverterJniMock);
    }

    /**
     * Test method for {@link
     * org.chromium.components.policy.PolicyConverter#setPolicy(java.lang.String,
     * java.lang.Object)}.
     */
    @Test
    public void testSetPolicy() {
        // Stub out the native methods.
        PolicyConverter policyConverter = PolicyConverter.create(1234);

        policyConverter.setPolicy("p1", true);
        verify(mPolicyConverterJniMock).setPolicyBoolean(1234, policyConverter, "p1", true);
        policyConverter.setPolicy("p1", 5678);
        verify(mPolicyConverterJniMock).setPolicyInteger(1234, policyConverter, "p1", 5678);
        policyConverter.setPolicy("p1", "hello");
        verify(mPolicyConverterJniMock).setPolicyString(1234, policyConverter, "p1", "hello");
        policyConverter.setPolicy("p1", new String[] {"hello", "goodbye"});
        verify(mPolicyConverterJniMock)
                .setPolicyStringArray(
                        1234, policyConverter, "p1", new String[] {"hello", "goodbye"});
        Bundle b1 = new Bundle();
        b1.putInt("i1", 23);
        b1.putString("s1", "a string");
        Bundle[] ba = new Bundle[1];
        ba[0] = new Bundle();
        ba[0].putBoolean("ba1b", true);
        ba[0].putString("ba1s", "another string");
        b1.putParcelableArray("b1b", ba);
        policyConverter.setPolicy("p1", b1);
        verify(mPolicyConverterJniMock)
                .setPolicyString(
                        1234,
                        policyConverter,
                        "p1",
                        "{\"i1\":23,\"s1\":\"a string\","
                                + "\"b1b\":[{\"ba1b\":true,\"ba1s\":\"another string\"}]}");
        policyConverter.setPolicy("p1", ba);
        verify(mPolicyConverterJniMock)
                .setPolicyString(
                        1234,
                        policyConverter,
                        "p1",
                        "[{\"ba1b\":true,\"ba1s\":\"another string\"}]");
    }
}
