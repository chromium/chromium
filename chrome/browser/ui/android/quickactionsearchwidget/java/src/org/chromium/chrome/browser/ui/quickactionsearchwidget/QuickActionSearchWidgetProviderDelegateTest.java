// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.RemoteViews;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.quickactionsearchwidget.QuickActionSearchWidgetProvider;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the QuickActionSearchWidgetProviderDelegate.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures({ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET})
public class QuickActionSearchWidgetProviderDelegateTest {
    private static final class TestContext extends AdvancedMockContext {
        public TestContext() {
            super(InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getApplicationContext());
        }
    }

    private static final class TestDelegate extends QuickActionSearchWidgetProviderDelegate {
        public TestDelegate(ComponentName searchComponent, ComponentName widgetComponent) {
            super(searchComponent, widgetComponent);
        }

        public final List<RemoteViews> mRemoteViews = new ArrayList<>();

        @Override
        protected void updateWidget(
                AppWidgetManager manager, int widgetId, RemoteViews remoteViews) {
            super.updateWidget(manager, widgetId, remoteViews);
            mRemoteViews.add(remoteViews);
        }
    }

    // These are random unique identifiers, the value of these numbers have no special meaning.
    // The number of identifiers has no particular meaning either.
    private static final int[] WIDGET_IDS = {1, 2};

    private final List<View> mWidgetViews = new ArrayList<>();

    private TestDelegate mDelegate;
    private TestContext mContext;

    @Before
    public void setUp() {
        ChromeApplicationTestUtils.setUp(InstrumentationRegistry.getTargetContext());

        mContext = new TestContext();

        ComponentName searchComponent = new ComponentName(mContext, SearchActivity.class);
        ComponentName widgetComponent =
                new ComponentName(mContext, QuickActionSearchWidgetProvider.class);
        mDelegate = new TestDelegate(searchComponent, widgetComponent);

        QuickActionSearchWidgetProvider.setWidgetEnabled(true);

        setUpViews();
    }

    @After
    public void tearDown() {
        ChromeApplicationTestUtils.tearDown(InstrumentationRegistry.getTargetContext());
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1225218")
    public void testHandleStartTextQueryAction() {
        Intent startTextQueryIntent =
                new Intent(QuickActionSearchWidgetProviderDelegate.ACTION_START_TEXT_QUERY);

        assertSearchActivityLaunchedAfterAction(
                () -> mDelegate.handleAction(mContext, startTextQueryIntent));
    }

    @Test
    @SmallTest
    public void testSearchBarClick() {
        for (View view : mWidgetViews) {
            // clang-format off
            assertSearchActivityLaunchedAfterAction(() -> clickOnView(view,
                    R.id.quick_action_search_widget_search_bar_container));
            // clang-format on
        }
    }

    private void setUpViews() {
        FrameLayout parentView = new FrameLayout(mContext);

        AppWidgetManager widgetManager = AppWidgetManager.getInstance(mContext);
        mDelegate.updateWidgets(mContext, widgetManager, WIDGET_IDS);

        for (int i = 0; i < mDelegate.mRemoteViews.size(); i++) {
            RemoteViews views = mDelegate.mRemoteViews.get(i);
            View view = views.apply(mContext, parentView);

            parentView.addView(view);
            mWidgetViews.add(view);
        }
    }

    private void assertSearchActivityLaunchedAfterAction(Runnable action) {
        Activity activity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class, action);
        Assert.assertNotNull(activity);
    }

    private void clickOnView(final View view, final int clickTarget) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { view.findViewById(clickTarget).performClick(); });
    }
}
