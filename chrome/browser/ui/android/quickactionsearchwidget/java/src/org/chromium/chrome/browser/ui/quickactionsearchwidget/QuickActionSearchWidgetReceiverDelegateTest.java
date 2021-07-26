// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for the QuickActionSearchWidgetReceiverDelegate.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures({ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET})
public class QuickActionSearchWidgetReceiverDelegateTest {
    private static final class TestContext extends AdvancedMockContext {
        public TestContext() {
            super(InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getApplicationContext());
        }
    }

    private QuickActionSearchWidgetReceiverDelegate mDelegate;
    private TestContext mContext;

    @Rule
    public BaseActivityTestRule<Activity> mActivityTestRule =
            new BaseActivityTestRule<>(Activity.class);

    @Before
    public void setUp() {
        ChromeApplicationTestUtils.setUp(InstrumentationRegistry.getTargetContext());

        mContext = new TestContext();

        ComponentName searchComponent = new ComponentName(mContext, SearchActivity.class);
        ComponentName chromeLauncherComponent =
                new ComponentName(mContext, ChromeLauncherActivity.class);

        mDelegate = new QuickActionSearchWidgetReceiverDelegate(
                searchComponent, chromeLauncherComponent);
    }

    @After
    public void tearDown() {
        ChromeApplicationTestUtils.tearDown(InstrumentationRegistry.getTargetContext());
    }

    @Test
    @SmallTest
    public void testHandleStartTextQueryAction() {
        Intent startTextQueryIntent =
                new Intent(QuickActionSearchWidgetReceiverDelegate.ACTION_START_TEXT_QUERY);

        QuickActionSearchWidgetTestUtils.assertSearchActivityLaunchedAfterAction(
                mActivityTestRule, () -> {
                    mDelegate.handleAction(mContext, startTextQueryIntent);
                }, /*shouldActivityLaunchVoiceMode=*/false);
    }

    @Test
    @SmallTest
    public void testHandleStartVoiceQueryAction() {
        Intent startVoiceQueryIntent =
                new Intent(QuickActionSearchWidgetReceiverDelegate.ACTION_START_VOICE_QUERY);

        QuickActionSearchWidgetTestUtils.assertSearchActivityLaunchedAfterAction(
                mActivityTestRule, () -> {
                    mDelegate.handleAction(mContext, startVoiceQueryIntent);
                }, /*shouldActivityLaunchVoiceMode=*/true);
    }

    @Test
    @SmallTest
    public void testHandleStartDinoGameAction() {
        Intent startDinoGameIntent =
                new Intent(QuickActionSearchWidgetReceiverDelegate.ACTION_START_DINO_GAME);

        QuickActionSearchWidgetTestUtils.assertDinoGameLaunchedAfterAction(
                mActivityTestRule, () -> mDelegate.handleAction(mContext, startDinoGameIntent));
    }
}
