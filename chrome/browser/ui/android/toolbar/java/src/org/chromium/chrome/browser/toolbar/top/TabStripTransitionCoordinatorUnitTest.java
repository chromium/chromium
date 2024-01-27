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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObscuringHandler.Target;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.top.TabStripTransitionCoordinator.TabStripHeightObserver;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link TabStripTransitionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w600dp", shadows = ShadowLooper.class)
public class TabStripTransitionCoordinatorUnitTest {
    private static final int TEST_TAB_STRIP_HEIGHT = 40;
    private static final int TEST_TOOLBAR_HEIGHT = 56;
    private static final int NOTHING_OBSERVED = -1;
    private static final int NARROW_WINDOW_WIDTH = 411;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private ControlContainer mControlContainer;
    @Mock private ViewResourceAdapter mViewResourceAdapter;
    @Captor private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserver;
    @Captor private ArgumentCaptor<Callback<Resource>> mOnCaptureReadyCallback;

    private TestControlContainerView mSpyControlContainer;
    private TabStripTransitionCoordinator mCoordinator;
    private TestActivity mActivity;
    private TabObscuringHandler mTabObscuringHandler = new TabObscuringHandler();

    private int mTopControlsContentOffset;

    private TestObserver mObserver;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mSpyControlContainer = TestControlContainerView.createSpy(mActivity);
        mActivity.setContentView(mSpyControlContainer);

        // Set the mocks for control container and its view resource adapter.
        doReturn(mSpyControlContainer).when(mControlContainer).getView();
        doReturn(mViewResourceAdapter).when(mControlContainer).getToolbarResourceAdapter();
        doNothing()
                .when(mViewResourceAdapter)
                .addOnResourceReadyCallback(mOnCaptureReadyCallback.capture());
        doAnswer(inv -> triggerCapture()).when(mViewResourceAdapter).triggerBitmapCapture();

        mCoordinator =
                new TabStripTransitionCoordinator(
                        mBrowserControlsVisibilityManager,
                        mControlContainer,
                        mSpyControlContainer.toolbarLayout,
                        TEST_TAB_STRIP_HEIGHT,
                        mTabObscuringHandler);
        mObserver = new TestObserver();
        mCoordinator.addObserver(mObserver);

        mTopControlsContentOffset = TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT;
        doNothing()
                .when(mBrowserControlsVisibilityManager)
                .addObserver(mBrowserControlsObserver.capture());
        doReturn(View.VISIBLE)
                .when(mBrowserControlsVisibilityManager)
                .getAndroidControlsVisibility();
        doAnswer(invocationOnMock -> mTopControlsContentOffset)
                .when(mBrowserControlsVisibilityManager)
                .getContentOffset();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void initWithWideWindow() {
        Assert.assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        Assert.assertEquals("Tab strip height is wrong.", 0, mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void initWithNarrowWindow() {
        Assert.assertEquals(
                "Init will not change the tab strip height.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());
        Assert.assertEquals(
                "Tab strip height requested changing to 0.", 0, mObserver.heightRequested);

        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Changing the window to wide will request for full-size tab strip.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStrip() {
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);

        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        runOffsetTransitionForBrowserControlManager(
                /* beginOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT,
                /* endOffset= */ TEST_TOOLBAR_HEIGHT);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWithOffsetOverride() {
        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWhileTopControlsHidden() {
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);

        // Assume the top control is hidden and content is at the top.
        doReturn(0).when(mBrowserControlsVisibilityManager).getContentOffset();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false);

        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
        assertObservedTransitionFinished(true);
    }

    @Test
    public void hideTabStripWhileUrlBarFocused() {
        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should be blocked by the url bar focus.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Url focus animation finished to unblock the transition.
        mCoordinator.onUrlAnimationFinished(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the url bar focus.",
                0,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStripWhileTabObscured() {
        TabObscuringHandler.Token token = mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should be blocked after tab obscured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Url focus animation finished to unblock the transition
        mTabObscuringHandler.unobscure(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after tab unobscured.",
                0,
                mObserver.heightRequested);
    }

    @Test
    public void hideTabStripWhileTabAndToolbarObscured() {
        mTabObscuringHandler.obscure(Target.ALL_TABS_AND_TOOLBAR);
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        Assert.assertEquals(
                "Height request should go through when tab and toolbar are obscured.",
                0,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip() {
        settleTransitionDuringInitForNarrowWindow();
        setDeviceWidthDp(600);

        doReturn(TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        runOffsetTransitionForBrowserControlManager(
                /* beginOffset= */ TEST_TOOLBAR_HEIGHT,
                /* endOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWithOffsetOverride() {
        settleTransitionDuringInitForNarrowWindow();
        // Simulate top controls size change from browser.
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTopControlsHidden() {
        settleTransitionDuringInitForNarrowWindow();
        setDeviceWidthDp(600);

        // Assume the top control is hidden and content is at the top.
        doReturn(0).when(mBrowserControlsVisibilityManager).getContentOffset();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false);

        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
        assertObservedTransitionFinished(true);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileUrlBarFocused() {
        settleTransitionDuringInitForNarrowWindow();
        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be blocked by the url bar focus.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Url focus animation finished to unblock the transition
        mCoordinator.onUrlAnimationFinished(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the url bar focus.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTabObscured() {
        settleTransitionDuringInitForNarrowWindow();
        TabObscuringHandler.Token token = mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be blocked after tab obscured.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        // Url focus animation finished to unblock the transition
        mTabObscuringHandler.unobscure(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the tab unobscured.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTabAndToolbarObscured() {
        settleTransitionDuringInitForNarrowWindow();
        mTabObscuringHandler.obscure(Target.ALL_TABS_AND_TOOLBAR);
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should go through if both the tab and toolbar are obscured.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenBeforeLayout() {
        settleTransitionDuringInitForNarrowWindow();
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        setDeviceWidthDp(600);
        Assert.assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        mCoordinator.releaseTabStripToken(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenDuringLayout() {
        settleTransitionDuringInitForNarrowWindow();
        setConfigurationWithNewWidth(600);

        // Layout pass will trigger the delayed task for layout transition.
        simulateLayoutChange(600);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        Assert.assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        mCoordinator.releaseTabStripToken(token);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenReleaseEarly() {
        settleTransitionDuringInitForNarrowWindow();
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        setConfigurationWithNewWidth(600);
        simulateLayoutChange(600);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        mCoordinator.releaseTabStripToken(token);
        Assert.assertEquals(
                "Height request should be blocked by the delayed layout request.",
                NOTHING_OBSERVED,
                mObserver.heightRequested);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mObserver.heightRequested);
    }

    @Test
    public void configurationChangedDuringDelayedTask() {
        setConfigurationWithNewWidth(NARROW_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void destroyDuringDelayedTask() {
        setConfigurationWithNewWidth(NARROW_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        // Destroy the coordinator so the transition task is canceled.
        mCoordinator.destroy();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void viewStubInflated() {
        doReturn(mSpyControlContainer.findToolbar)
                .when(mSpyControlContainer)
                .findViewById(R.id.find_toolbar);
        doReturn(mSpyControlContainer.dropTargetView)
                .when(mSpyControlContainer)
                .findViewById(R.id.toolbar_drag_drop_target_view);

        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        runOffsetTransitionForBrowserControlManager(
                /* beginOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT,
                /* endOffset= */ TEST_TOOLBAR_HEIGHT);
        assertTabStripHeightForMargins(0);
    }

    @Test
    public void transitionFinishedUMASuccess() {
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.TabStripTransition.Finished", true)) {
            runOffsetTransitionForBrowserControlManager(
                    /* beginOffset= */ TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT,
                    /* endOffset= */ TEST_TOOLBAR_HEIGHT);
        }
    }

    @Test
    public void transitionFinishedUMAInterrupted() {
        setDeviceWidthDp(NARROW_WINDOW_WIDTH);
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();

        int midOffset = TEST_TOOLBAR_HEIGHT + TEST_TAB_STRIP_HEIGHT / 2;
        mTopControlsContentOffset = midOffset;
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.TabStripTransition.Finished", false)) {
            setDeviceWidthDp(600);
        }
    }

    /** Run #onControlsOffsetChanged, changing content offset from |beginOffset| to |endOffset|. */
    private void runOffsetTransitionForBrowserControlManager(int beginOffset, int endOffset) {
        mTopControlsContentOffset = beginOffset;

        final int step = (beginOffset - endOffset) / 10;
        for (int turns = 0; turns <= 10; turns++) {
            // Simulate top controls size change from browser. Input values doesn't matter in this
            // call.
            getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false);
            if (mTopControlsContentOffset == endOffset) break;

            assertObservedTransitionFinished(false);
            if (step > 0) {
                mTopControlsContentOffset = Math.max(endOffset, mTopControlsContentOffset - step);
            } else {
                mTopControlsContentOffset = Math.min(endOffset, mTopControlsContentOffset - step);
            }
        }
        assertObservedTransitionFinished(true);
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
                tabStripHeight + TEST_TOOLBAR_HEIGHT,
                mSpyControlContainer.findToolbar.getTopMargin());
        Assert.assertEquals(
                "Top margin is wrong for dropTargetView.",
                tabStripHeight,
                mSpyControlContainer.dropTargetView.getTopMargin());
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
                mObserver.heightChanged);
    }

    private void assertObservedTransitionFinished(boolean finished) {
        Assert.assertEquals(
                "Transition finished signal not dispatched. Current contentOffset: "
                        + mTopControlsContentOffset,
                finished,
                mObserver.transitionFinished);
    }

    private BrowserControlsStateProvider.Observer getBrowserControlsObserver() {
        var observer = mBrowserControlsObserver.getValue();
        Assert.assertNotNull("Browser controls observer not attached.", observer);
        return observer;
    }

    private Void triggerCapture() {
        var callback = mOnCaptureReadyCallback.getValue();
        Assert.assertNotNull("Capture callback is null.", callback);
        callback.onResult(null);
        return null;
    }

    // For test cases init with narrow width, the initialization will create an transition request.
    private void settleTransitionDuringInitForNarrowWindow() {
        mTopControlsContentOffset = TEST_TOOLBAR_HEIGHT;
        doReturn(TEST_TOOLBAR_HEIGHT)
                .when(mBrowserControlsVisibilityManager)
                .getTopControlsHeight();
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false);
        getBrowserControlsObserver().onControlsOffsetChanged(0, 0, 0, 0, false);
        mObserver = new TestObserver();
        mCoordinator.addObserver(mObserver);
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
    static class TestControlContainerView extends FrameLayout {
        public TestView toolbarLayout;
        public TestView toolbarHairline;
        public TestView findToolbar;
        public TestView dropTargetView;

        @Nullable public View.OnLayoutChangeListener onLayoutChangeListener;

        static TestControlContainerView createSpy(Context context) {
            TestControlContainerView controlContainer =
                    Mockito.spy(new TestControlContainerView(context, null));

            doReturn(controlContainer.toolbarLayout)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar);
            doReturn(controlContainer.toolbarHairline)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar_hairline);
            doReturn(controlContainer.findToolbar)
                    .when(controlContainer)
                    .findViewById(R.id.find_toolbar_tablet_stub);
            doReturn(controlContainer.dropTargetView)
                    .when(controlContainer)
                    .findViewById(R.id.target_view_stub);

            doAnswer(args -> context.getResources().getDisplayMetrics().widthPixels)
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

        public TestControlContainerView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);

            toolbarLayout = Mockito.spy(new TestView(context, attrs));
            findToolbar = Mockito.spy(new TestView(context, attrs));
            dropTargetView = Mockito.spy(new TestView(context, attrs));
            when(toolbarLayout.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            when(findToolbar.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            when(dropTargetView.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
            toolbarHairline = new TestView(context, attrs);

            MarginLayoutParams sourceParams =
                    new MarginLayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT);
            sourceParams.topMargin = TEST_TAB_STRIP_HEIGHT + TEST_TOOLBAR_HEIGHT;
            addView(toolbarHairline, new MarginLayoutParams(sourceParams));
            addView(findToolbar, new MarginLayoutParams(sourceParams));

            sourceParams.topMargin = TEST_TAB_STRIP_HEIGHT;
            sourceParams.height = TEST_TOOLBAR_HEIGHT;
            addView(toolbarLayout, new MarginLayoutParams(sourceParams));
            addView(dropTargetView, new MarginLayoutParams(sourceParams));
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

    static class TestObserver implements TabStripHeightObserver {
        public int heightRequested = NOTHING_OBSERVED;
        public int heightChanged = NOTHING_OBSERVED;
        public boolean transitionFinished;

        @Override
        public void onTransitionRequested(int newHeight) {
            heightRequested = newHeight;
        }

        @Override
        public void onHeightChanged(int newHeight) {
            heightChanged = newHeight;
        }

        @Override
        public void onTransitionFinished() {
            transitionFinished = true;
        }
    }
}
