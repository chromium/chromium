// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Map;

/**
 * Unit test for {@link SurveyClientBridge}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SurveyClientBridgeUnitTest {
    private static final long TEST_NATIVE_POINTER = 45312L;
    private static final String TEST_TRIGGER = "trigger";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    Activity mActivity;
    @Mock
    SurveyClientFactory mFactory;
    @Mock
    SurveyClient mDelegateSurveyClient;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        SurveyClientFactory.setInstanceForTesting(mFactory);

        doReturn(mDelegateSurveyClient).when(mFactory).createClient(any(), any());
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    public void showSurveyFromJava() {
        TestSurveyUtils.TestSurveyUiDelegate testDelegate =
                new TestSurveyUtils.TestSurveyUiDelegate();
        TestSurveyUtils.setTestSurveyConfigForTrigger(
                TEST_TRIGGER, new String[] {}, new String[] {});
        SurveyClientBridge bridge =
                SurveyClientBridge.create(TEST_NATIVE_POINTER, TEST_TRIGGER, testDelegate);
        assertNotNull(bridge);

        bridge.showSurvey(mActivity, mActivityLifecycleDispatcher);
        verify(mDelegateSurveyClient).showSurvey(mActivity, mActivityLifecycleDispatcher);
    }

    @Test
    public void showSurveyFromJavaWithPsd() {
        TestSurveyUtils.TestSurveyUiDelegate testDelegate =
                new TestSurveyUtils.TestSurveyUiDelegate();
        TestSurveyUtils.setTestSurveyConfigForTrigger(
                TEST_TRIGGER, new String[] {"bit1", "bit2"}, new String[] {"string1", "string2"});
        SurveyClientBridge bridge =
                SurveyClientBridge.create(TEST_NATIVE_POINTER, TEST_TRIGGER, testDelegate);
        assertNotNull(bridge);

        Map<String, Boolean> bitValues = Map.of("bit1", true, "bit2", false);
        Map<String, String> stringValues = Map.of("string1", "stringVal1", "string2", "stringVal2");
        bridge.showSurvey(mActivity, mActivityLifecycleDispatcher, bitValues, stringValues);
        verify(mDelegateSurveyClient)
                .showSurvey(mActivity, mActivityLifecycleDispatcher, bitValues, stringValues);
    }

    @Test
    public void showSurveyFromNativeWithPsd() {
        String[] bitFields = new String[] {"fieldTrue", "fieldFalse"};
        String[] stringFields = new String[] {"string1", "string2"};
        TestSurveyUtils.TestSurveyUiDelegate testDelegate =
                new TestSurveyUtils.TestSurveyUiDelegate();
        TestSurveyUtils.setTestSurveyConfigForTrigger(TEST_TRIGGER, bitFields, stringFields);
        SurveyClientBridge bridge =
                SurveyClientBridge.create(TEST_NATIVE_POINTER, TEST_TRIGGER, testDelegate);
        assertNotNull(bridge);

        WindowAndroid window = mock(WindowAndroid.class);
        doReturn(new WeakReference<>(mActivity)).when(window).getActivity();

        bridge.showSurvey(window, bitFields, new boolean[] {true, false}, stringFields,
                new String[] {"stringVal1", "stringVal2"});

        ArgumentCaptor<Map<String, Boolean>> bitValueCaptor = ArgumentCaptor.forClass(Map.class);
        ArgumentCaptor<Map<String, String>> stringValueCaptor = ArgumentCaptor.forClass(Map.class);

        verify(mDelegateSurveyClient)
                .showSurvey(eq(mActivity), isNull(), bitValueCaptor.capture(),
                        stringValueCaptor.capture());

        // Check bit values
        assertEquals("Bit PSD value mismatch.", true, bitValueCaptor.getValue().get("fieldTrue"));
        assertEquals("Bit PSD value mismatch.", false, bitValueCaptor.getValue().get("fieldFalse"));
        assertEquals("String PSD value mismatch.", "stringVal1",
                stringValueCaptor.getValue().get("string1"));
        assertEquals("String PSD value mismatch.", "stringVal2",
                stringValueCaptor.getValue().get("string2"));
    }
}
