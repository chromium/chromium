// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Unit tests for {@link OptionalNewTabButtonController}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class OptionalNewTabButtonControllerUnitTest {
    private static final int WIDTH_DELTA = 50;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Context mContext;
    @Mock
    private Resources mResources;
    @Mock
    private Tab mTab;
    @Mock
    private Drawable mDrawable;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreator mTabCreator;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private Tracker mTracker;

    private Configuration mConfiguration = new Configuration();
    private OptionalNewTabButtonController mOptionalNewTabButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mContext).when(mTab).getContext();
        doReturn(mResources).when(mContext).getResources();

        mConfiguration.screenWidthDp = OptionalNewTabButtonController.MIN_WIDTH_DP + WIDTH_DELTA;
        doReturn(mConfiguration).when(mResources).getConfiguration();

        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());

        AdaptiveToolbarFeatures.clearParsedParamsForTesting();

        mOptionalNewTabButtonController = new OptionalNewTabButtonController(mContext, mDrawable,
                mActivityLifecycleDispatcher,
                () -> mTabCreatorManager, () -> mTabModelSelector, () -> mTracker);

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @Test
    public void testIPHCommandHelper() {
        assertNull(mOptionalNewTabButtonController.get(/*tab*/ null)
                           .getButtonSpec()
                           .getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is set just once;
        IPHCommandBuilder builder =
                mOptionalNewTabButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder();

        assertNotNull(
                mOptionalNewTabButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is same as before, get(Tab) did not create a new one.
        assertEquals(builder,
                mOptionalNewTabButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testIPHEvent() {
        doReturn(true).when(mTracker).shouldTriggerHelpUI(
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_NEW_TAB_FEATURE);

        View view = Mockito.mock(View.class);
        mOptionalNewTabButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(
                view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_NEW_TAB_OPENED);
    }
}
