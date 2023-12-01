// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link TabStripTransitionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w600dp", shadows = ShadowLooper.class)
public class TabStripTransitionCoordinatorUnitTest {
    private static final int TEST_TAB_STRIP_HEIGHT = 40;
    private static final int TEST_TOOLBAR_HEIGHT = 56;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Captor private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserver;

    private TabStripTransitionCoordinator mCoordinator;
    private TestActivity mActivity;
    private TestControlContainer mSpyControlContainer;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mSpyControlContainer = TestControlContainer.createSpy(mActivity);
        mActivity.setContentView(mSpyControlContainer);

        mCoordinator =
                new TabStripTransitionCoordinator(
                        mBrowserControlsVisibilityManager,
                        mSpyControlContainer,
                        mSpyControlContainer.toolbarLayout,
                        TEST_TAB_STRIP_HEIGHT);
        doNothing()
                .when(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserver.capture());
        doReturn(View.VISIBLE)
                .when(mBrowserControlsVisibilityManager)
                .getAndroidControlsVisibility();
    }

    @Test
    public void updateTabStripHeight_WideWindow() {
        simulateConfigurationChanged(null);
        Assert.assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        setDeviceWidthDp(480);
        Assert.assertEquals("Tab strip height is wrong.", 0, mCoordinator.getTabStripHeight());
    }

    @Test
    @Config(qualifiers = "w480dp")
    public void updateTabStripHeight_NarrowWindow() {
        simulateConfigurationChanged(null);
        Assert.assertEquals("Tab strip height is wrong.", 0, mCoordinator.getTabStripHeight());

        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());
    }

    @Test
    public void updateTabStripHeight_DuringLayout() {
        simulateConfigurationChanged(null);
        Assert.assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        doReturn(true).when(mSpyControlContainer).isInLayout();
        setDeviceWidthDp(480);
        Assert.assertEquals(
                "Tab strip height is not updated yet during layout.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());
        simulateLayoutChange();
        Assert.assertEquals(
                "Tab strip height should be updated after layout.",
                0,
                mCoordinator.getTabStripHeight());
    }

    @Test
    public void updateTabStripHeight_BrowserControlsHidden() {
        doReturn(true).when(mSpyControlContainer).isInLayout();
        doReturn(View.GONE).when(mBrowserControlsVisibilityManager).getAndroidControlsVisibility();
        simulateConfigurationChanged(null);
        Assert.assertEquals(
                "Tab strip update should be instant.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        setDeviceWidthDp(480);
        Assert.assertEquals(
                "Tab strip update should be instant.", 0, mCoordinator.getTabStripHeight());
    }

    @Test
    public void hideTabStrip() {
        simulateConfigurationChanged(null);

        setDeviceWidthDp(480);

        // Simulate top controls size change from browser. Input values doesn't matter in this call.
        mBrowserControlsObserver.getValue().onControlsOffsetChanged(0, 0, 0, 0, false);
        assertTabStripHeightForMargins(0);
    }

    @Test
    public void hideTabStripWithOffsetOverride() {
        simulateConfigurationChanged(null);

        setDeviceWidthDp(480);

        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        mBrowserControlsObserver.getValue().onTopControlsHeightChanged(TEST_TOOLBAR_HEIGHT, 0);
        assertTabStripHeightForMargins(0);
    }

    @Test
    @Config(qualifiers = "w480dp")
    public void showTabStrip() {
        simulateConfigurationChanged(null);

        setDeviceWidthDp(600);

        // Simulate top controls size change from browser. Input values doesn't matter in this call.
        mBrowserControlsObserver.getValue().onControlsOffsetChanged(0, 0, 0, 0, false);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w480dp")
    public void showTabStripWithOffsetOverride() {
        simulateConfigurationChanged(null);

        setDeviceWidthDp(600);
        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        mBrowserControlsObserver.getValue().onTopControlsHeightChanged(TEST_TOOLBAR_HEIGHT, 0);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void configurationChangedDuringDelayedTask() {
        setConfigurationWithNewWidth(480);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void destroyDuringDelayedTask() {
        setConfigurationWithNewWidth(480);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        // Destroy the coordinator so the transition task is canceled.
        mCoordinator.destroy();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    private void setDeviceWidthDp(int widthDp) {
        Configuration configuration = setConfigurationWithNewWidth(widthDp);
        simulateConfigurationChanged(configuration);
    }

    private Configuration setConfigurationWithNewWidth(int widthDp) {
        Resources res = mActivity.getResources();
        DisplayMetrics displayMetrics = res.getDisplayMetrics();
        displayMetrics.widthPixels = (int) displayMetrics.density * widthDp;

        Configuration configuration = res.getConfiguration();
        configuration.screenWidthDp = widthDp;
        mActivity.createConfigurationContext(configuration);
        return configuration;
    }

    private void assertTabStripHeightForMargins(int tabStripHeight) {
        Assert.assertEquals(
                "Top margin is wrong for toolbarLayout.",
                tabStripHeight,
                mSpyControlContainer.toolbarLayout.getTopMargin());
        Assert.assertEquals(
                "Top margin is wrong for findToolbar.",
                tabStripHeight,
                mSpyControlContainer.findToolbar.getTopMargin());
        Assert.assertEquals(
                "Top margin is wrong for toolbarHairline.",
                tabStripHeight + TEST_TOOLBAR_HEIGHT,
                mSpyControlContainer.toolbarHairline.getTopMargin());
    }

    private void simulateLayoutChange() {
        Assert.assertNotNull(mSpyControlContainer.onLayoutChangeListener);
        mSpyControlContainer.onLayoutChangeListener.onLayoutChange(
                mSpyControlContainer, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    private void simulateConfigurationChanged(Configuration newConfig) {
        mCoordinator.onConfigurationChanged(newConfig != null ? newConfig : new Configuration());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    // Due to the complexity to use the real views for top toolbar in robolectric tests, use view
    // mocks for the sake of unit tests.
    static class TestControlContainer extends FrameLayout {
        public TestView toolbarLayout;
        public TestView toolbarHairline;
        public TestView findToolbar;

        @Nullable public View.OnLayoutChangeListener onLayoutChangeListener;

        static TestControlContainer createSpy(Context context) {
            TestControlContainer controlContainer =
                    Mockito.spy(new TestControlContainer(context, null));

            doReturn(controlContainer.toolbarLayout)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar);
            doReturn(controlContainer.toolbarHairline)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar_hairline);
            doReturn(controlContainer.findToolbar)
                    .when(controlContainer)
                    .findViewById(R.id.find_toolbar);

            doAnswer(
                            args -> {
                                controlContainer.onLayoutChangeListener = args.getArgument(0);
                                return null;
                            })
                    .when(controlContainer)
                    .addOnLayoutChangeListener(any());

            return controlContainer;
        }

        public TestControlContainer(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);

            toolbarLayout = Mockito.spy(new TestView(context, attrs));
            findToolbar = Mockito.spy(new TestView(context, attrs));
            when(toolbarLayout.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            when(findToolbar.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            toolbarHairline = new TestView(context, attrs);

            MarginLayoutParams sourceParams =
                    new MarginLayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT);
            sourceParams.topMargin = TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT;
            addView(toolbarHairline, new MarginLayoutParams(sourceParams));

            sourceParams.topMargin = TEST_TAB_STRIP_HEIGHT;
            sourceParams.height = TEST_TOOLBAR_HEIGHT;
            addView(toolbarLayout, new MarginLayoutParams(sourceParams));
            addView(findToolbar, new MarginLayoutParams(sourceParams));
        }
    }

    static class TestView extends View {
        public TestView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);
        }

        public int getTopMargin() {
            return ((MarginLayoutParams) getLayoutParams()).topMargin;
        }
    }
}
