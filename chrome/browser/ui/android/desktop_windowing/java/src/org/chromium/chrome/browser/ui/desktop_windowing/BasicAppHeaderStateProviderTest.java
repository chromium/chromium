// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;

import android.app.Activity;
import android.os.Build.VERSION_CODES;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderTestUtils.MockCaptionBarInsetsSetter;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.insets.CaptionBarInsetsRectProvider;
import org.chromium.ui.insets.InsetsRectProvider;

/** Unit tests for {@link BasicAppHeaderStateProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.R)
public class BasicAppHeaderStateProviderTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private CaptionBarInsetsRectProvider mCaptionBarInsetsRectProvider;

    private Activity mActivity;
    private BasicAppHeaderStateProvider mProvider;

    @Captor private ArgumentCaptor<InsetsRectProvider.Consumer> mConsumerCaptor;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        doNothing().when(mCaptionBarInsetsRectProvider).setConsumer(mConsumerCaptor.capture());
        mProvider = new BasicAppHeaderStateProvider(mActivity, mCaptionBarInsetsRectProvider);
    }

    @Test
    public void testAppHeaderState_inDesktopWindow() {
        var testInsets = MockCaptionBarInsetsSetter.standardDesktopWindow();
        testInsets.apply(mCaptionBarInsetsRectProvider);
        mConsumerCaptor.getValue().onWidestUnoccludedRectUpdated(testInsets.widestUnoccludedRect);

        AppHeaderState state = mProvider.getAppHeaderState();
        assertNotNull(state);
        assertTrue(state.isInDesktopWindow());
    }

    @Test
    public void testAppHeaderState_notInDesktopWindow() {
        var testInsets = MockCaptionBarInsetsSetter.empty();
        testInsets.apply(mCaptionBarInsetsRectProvider);
        mConsumerCaptor.getValue().onWidestUnoccludedRectUpdated(testInsets.widestUnoccludedRect);

        AppHeaderState state = mProvider.getAppHeaderState();
        assertNotNull(state);
        assertFalse(state.isInDesktopWindow());
    }
}
