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
import org.chromium.chrome.browser.toolbar.top.TabStripTransitionCoordinator.TabStripHeightObserver;
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

    private int mObservedOnHeightRequested = -1;
    private int mObservedOnHeightChanged = -1;

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
        TabStripHeightObserver observer =
                new TabStripHeightObserver() {
                    @Override
                    public void onHeightTransitionRequested(int newHeight) {
                        mObservedOnHeightRequested = newHeight;
                    }

                    @Override
                    public void onHeightChanged(int newHeight) {
                        mObservedOnHeightChanged = newHeight;
                    }
                };
        mCoordinator.addObserver(observer);

        doNothing()
                .when(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserver.capture());
        doReturn(View.VISIBLE)
                .when(mBrowserControlsVisibilityManager)
                .getAndroidControlsVisibility();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void initWithWideWindow() {
        Assert.assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        setDeviceWidthDp(480);
        Assert.assertEquals("Tab strip height is wrong.", 0, mObservedOnHeightRequested);
    }

    @Test
    @Config(qualifiers = "w480dp")
    public void initWithNarrowWindow() {
        Assert.assertEquals(
                "Init will not change the tab strip height.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());
        Assert.assertEquals(
                "Tab strip height requested changing to 0.", 0, mObservedOnHeightRequested);

        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Changing the window to wide will request for full-size tab strip.",
                TEST_TAB_STRIP_HEIGHT,
                mObservedOnHeightRequested);
    }

    @Test
    public void hideTabStrip() {
        setDeviceWidthDp(480);

        // Simulate top controls size change from browser. Input values doesn't matter in this call.
        mBrowserControlsObserver.getValue().onControlsOffsetChanged(0, 0, 0, 0, false);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWithOffsetOverride() {
        setDeviceWidthDp(480);

        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        mBrowserControlsObserver.getValue().onTopControlsHeightChanged(TEST_TOOLBAR_HEIGHT, 0);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    @Config(qualifiers = "w480dp")
    public void showTabStrip() {
        setDeviceWidthDp(600);

        // Simulate top controls size change from browser. Input values doesn't matter in this call.
        mBrowserControlsObserver.getValue().onControlsOffsetChanged(0, 0, 0, 0, false);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w480dp")
    public void showTabStripWithOffsetOverride() {
        setDeviceWidthDp(600);
        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        mBrowserControlsObserver.getValue().onTopControlsHeightChanged(TEST_TOOLBAR_HEIGHT, 0);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void configurationChangedDuringDelayedTask() {
        setConfigurationWithNewWidth(480);
        simulateLayoutChange(480);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void destroyDuringDelayedTask() {
        setConfigurationWithNewWidth(480);
        simulateLayoutChange(480);
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
        simulateLayoutChange(widthDp);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
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

    private void assertObservedHeight(int tabStripHeight) {
        Assert.assertEquals(
                "#getHeight has a different value.",
                tabStripHeight,
                mCoordinator.getTabStripHeight());

        Assert.assertEquals(
                "Observer#onHeightChanged received a different value.",
                tabStripHeight,
                mObservedOnHeightChanged);
    }

    private void simulateLayoutChange(int width) {
        Assert.assertNotNull(mSpyControlContainer.onLayoutChangeListener);
        mSpyControlContainer.onLayoutChangeListener.onLayoutChange(
                mSpyControlContainer,
                /* left= */ 0,
                /* top= */ 0,
                /* right= */ width,
                /* bottom= */ 0,
                0,
                0,
                0,
                0);
    }

    private void simulateConfigurationChanged(Configuration newConfig) {
        mCoordinator.onConfigurationChanged(newConfig != null ? newConfig : new Configuration());
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
            doReturn(context.getResources().getDisplayMetrics().widthPixels)
                    .when(controlContainer)
                    .getWidth();
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
