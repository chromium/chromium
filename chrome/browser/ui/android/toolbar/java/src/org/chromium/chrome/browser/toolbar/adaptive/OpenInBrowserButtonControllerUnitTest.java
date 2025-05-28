// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link OpenInBrowserButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class OpenInBrowserButtonControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Context mContext;

    @Mock private Tab mTab;
    @Mock private Drawable mDrawable;
    @Mock private Supplier<Tab> mTabSupplier;
    @Mock private Runnable mOpenInBrowserRunnable;
    @Mock private Tracker mTracker;

    private OpenInBrowserButtonController mOpenInBrowserButtonController;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
        doReturn(mContext).when(mTab).getContext();

        doReturn(mTab).when(mTabSupplier).get();

        mOpenInBrowserButtonController =
                new OpenInBrowserButtonController(
                        mContext, mDrawable, mTabSupplier, mOpenInBrowserRunnable, () -> mTracker);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testIphCommandHelper() {
        assertNull(
                mOpenInBrowserButtonController
                        .get(/* tab= */ null)
                        .getButtonSpec()
                        .getIphCommandBuilder());

        // Verify that IphCommandBuilder is set just once;
        IphCommandBuilder builder =
                mOpenInBrowserButtonController.get(mTab).getButtonSpec().getIphCommandBuilder();

        assertNotNull(
                mOpenInBrowserButtonController.get(mTab).getButtonSpec().getIphCommandBuilder());

        // Verify that IphCommandBuilder is same as before, get(Tab) did not create a new one.
        assertEquals(
                builder,
                mOpenInBrowserButtonController.get(mTab).getButtonSpec().getIphCommandBuilder());
    }

    @Test
    public void testIphEvent() {
        var feature =
                FeatureConstants
                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_OPEN_IN_BROWSER_FEATURE;
        doReturn(true).when(mTracker).shouldTriggerHelpUi(feature);

        View view = Mockito.mock(View.class);
        mOpenInBrowserButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_OPEN_IN_BROWSER_OPENED);
    }
}
