// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Instrumentation tests for the Paint Preview player.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaintPreviewPlayerTest extends DummyUiActivityTestCase {
    private static final long TIMEOUT_MS = ScalableTimeout.scaleTimeout(5000);

    private static final String TEST_DIRECTORY_KEY = "test_dir";
    private static final String TEST_URL = "https://www.chromium.org";
    private static final String TEST_IN_VIEWPORT_LINK_URL = "http://www.google.com/";
    private static final String TEST_OUT_OF_VIEWPORT_LINK_URL = "http://example.com/";
    private final Rect mInViewportLinkRect = new Rect(700, 650, 900, 700);
    private final Rect mOutOfViewportLinkRect = new Rect(300, 4900, 450, 5000);

    private static final int TEST_PAGE_WIDTH = 1082;
    private static final int TEST_PAGE_HEIGHT = 5019;

    @Rule
    public PaintPreviewTestRule mPaintPreviewTestRule = new PaintPreviewTestRule();

    @Rule
    public TemporaryFolder mTempFolder = new TemporaryFolder();

    private PlayerManager mPlayerManager;
    private TestLinkClickHandler mLinkClickHandler;
    private CallbackHelper mRefreshedCallback;
    private boolean mInitializationFailed;

    /**
     * LinkClickHandler implementation for caching the last URL that was clicked.
     */
    public class TestLinkClickHandler implements LinkClickHandler {
        GURL mUrl;

        @Override
        public void onLinkClicked(GURL url) {
            mUrl = url;
        }
    }

    @Override
    public void tearDownTest() throws Exception {
        super.tearDownTest();
        CallbackHelper destroyed = new CallbackHelper();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            mPlayerManager.destroy();
            destroyed.notifyCalled();
        });
        destroyed.waitForFirst();
    }

    private void displayTest(boolean multipleFrames) {
        initPlayerManager(multipleFrames);
        final View playerHostView = mPlayerManager.getView();
        final View activityContentView = getActivity().findViewById(android.R.id.content);

        // Assert that the player view has the same dimensions as the content view.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activityContentView.getWidth(), Matchers.greaterThan(0));
            Criteria.checkThat(activityContentView.getHeight(), Matchers.greaterThan(0));
            Criteria.checkThat(
                    activityContentView.getWidth(), Matchers.is(playerHostView.getWidth()));
            Criteria.checkThat(
                    activityContentView.getHeight(), Matchers.is(playerHostView.getHeight()));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Tests the the player correctly initializes and displays a sample paint preview with 1 frame.
     */
    @Test
    @MediumTest
    public void singleFrameDisplayTest() {
        displayTest(false);
    }

    /**
     * Tests the player correctly initializes and displays a sample paint preview with multiple
     * frames.
     */
    @Test
    @MediumTest
    public void multiFrameDisplayTest() {
        displayTest(true);
    }

    /**
     * Tests that link clicks in the player work correctly.
     */
    @Test
    @MediumTest
    @DisableIf.Build(message = "Test is failing on Android P, see crbug.com/1110939.",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void linkClickTest() {
        initPlayerManager(false);
        final View playerHostView = mPlayerManager.getView();

        // Click on a link that is visible in the default viewport.
        assertLinkUrl(playerHostView, 720, 670, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 880, 675, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 800, 680, TEST_IN_VIEWPORT_LINK_URL);

        // Scroll to the bottom, and click on a link.
        scrollToBottom();
        assertLinkUrl(playerHostView, 320, 4920, TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 375, 4950, TEST_OUT_OF_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 430, 4980, TEST_OUT_OF_VIEWPORT_LINK_URL);
    }

    @Test
    @MediumTest
    public void nestedLinkClickTest() throws Exception {
        initPlayerManager(true);
        final View playerHostView = mPlayerManager.getView();
        assertLinkUrl(playerHostView, 220, 220, TEST_IN_VIEWPORT_LINK_URL);
        assertLinkUrl(playerHostView, 300, 270, TEST_IN_VIEWPORT_LINK_URL);

        // Temporarily commenting out as this is flaky on P.

        // UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        // int deviceHeight = device.getDisplayHeight();
        // int statusBarHeight = statusBarHeight();
        // int navigationBarHeight = navigationBarHeight();
        // int padding = 20;
        // int fromY = deviceHeight - navigationBarHeight - padding;
        // int toY = statusBarHeight + padding;
        // mLinkClickHandler.mUrl = null;
        // device.swipe(300, fromY, 300, toY, 10);

        // Manually click as assertLinkUrl() doesn't handle subframe scrolls well.
        // assertLinkUrl(playerHostView, 200, 1500, TEST_OUT_OF_VIEWPORT_LINK_URL);
    }

    @Test
    @MediumTest
    public void overscrollRefreshTest() throws Exception {
        initPlayerManager(true);
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        int deviceHeight = uiDevice.getDisplayHeight();
        int statusBarHeight = statusBarHeight();
        int navigationBarHeight = navigationBarHeight();
        int padding = 20;
        int toY = deviceHeight - navigationBarHeight - padding;
        int fromY = statusBarHeight + padding;
        uiDevice.swipe(50, fromY, 50, toY, 5);

        mRefreshedCallback.waitForFirst();
    }

    /**
     * Tests that an initialization failure is reported properly.
     */
    @Test
    @MediumTest
    public void initializationCallbackErrorReported() throws Exception {
        CallbackHelper compositorErrorCallback = new CallbackHelper();
        mLinkClickHandler = new TestLinkClickHandler();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(mTempFolder.getRoot().getPath());
            // Use the wrong URL to simulate a failure.
            mPlayerManager = new PlayerManager(new GURL("about:blank"), getActivity(), service,
                    TEST_DIRECTORY_KEY, new PlayerManager.Listener() {
                        @Override
                        public void onCompositorError(int status) {
                            compositorErrorCallback.notifyCalled();
                        }

                        @Override
                        public void onViewReady() {
                            Assert.fail("View Ready callback occurred, but expected a failure.");
                        }

                        @Override
                        public void onFirstPaint() {}

                        @Override
                        public void onUserInteraction() {}

                        @Override
                        public void onUserFrustration() {}

                        @Override
                        public void onPullToRefresh() {
                            Assert.fail("Unexpected overscroll refresh attempted.");
                        }

                        @Override
                        public void onLinkClick(GURL url) {
                            mLinkClickHandler.onLinkClicked(url);
                        }
                    }, 0xffffffff, false);
            mPlayerManager.setCompressOnClose(false);
        });
        compositorErrorCallback.waitForFirst();
    }

    private void scaleSmokeTest(boolean multiFrame) throws Exception {
        initPlayerManager(multiFrame);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        // Query all FrameLayout objects as the PlayerFrameView isn't recognized.
        List<UiObject2> objects = device.findObjects(By.clazz("android.widget.FrameLayout"));

        int viewAxHashCode = mPlayerManager.getView().createAccessibilityNodeInfo().hashCode();
        boolean didPinch = false;
        for (UiObject2 object : objects) {
            // To ensure we only apply the gesture to the right FrameLayout we compare the hash
            // codes of the underlying accessibility nodes which are equivalent for the same
            // view. Hence we can avoid the lack of direct access to View objects from UiAutomator.
            if (object.hashCode() != viewAxHashCode) continue;

            // Just zoom in and out. The goal here is to just exercise the zoom pathway and ensure
            // it doesn't smoke when driven by gestures. There are more comprehensive tests for this
            // in PlayerFrameMediatorTest and PlayerFrameScaleController.
            object.pinchOpen(0.3f);
            object.pinchClose(0.2f);
            object.pinchClose(0.1f);
            didPinch = true;
        }
        Assert.assertTrue("Failed to pinch player view.", didPinch);
    }

    /**
     * Tests that scaling works and doesn't crash.
     */
    @Test
    @MediumTest
    public void singleFrameScaleSmokeTest() throws Exception {
        scaleSmokeTest(false);
    }

    /**
     * Tests that scaling works and doesn't crash with multiple frames.
     */
    @Test
    @MediumTest
    public void multiFrameScaleSmokeTest() throws Exception {
        scaleSmokeTest(true);
    }

    private int statusBarHeight() {
        Rect visibleContentRect = new Rect();
        getActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleContentRect);
        return visibleContentRect.top;
    }

    private int navigationBarHeight() {
        int navigationBarHeight = 100;
        int resourceId = getActivity().getResources().getIdentifier(
                "navigation_bar_height", "dimen", "android");
        if (resourceId > 0) {
            navigationBarHeight = getActivity().getResources().getDimensionPixelSize(resourceId);
        }
        return navigationBarHeight;
    }

    /**
     * Scrolls to the bottom fo the paint preview.
     */
    private void scrollToBottom() {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        int deviceHeight = uiDevice.getDisplayHeight();

        int statusBarHeight = statusBarHeight();
        int navigationBarHeight = navigationBarHeight();

        int padding = 20;
        int swipeSteps = 5;
        int viewPortBottom = deviceHeight - statusBarHeight - navigationBarHeight;
        int fromY = deviceHeight - navigationBarHeight - padding;
        int toY = statusBarHeight + padding;
        int delta = fromY - toY;
        while (viewPortBottom < scaleAbsoluteCoordinateToViewCoordinate(TEST_PAGE_HEIGHT)) {
            uiDevice.swipe(50, fromY, 50, toY, swipeSteps);
            viewPortBottom += delta;
        }
        // Repeat an addition time to avoid flakiness.
        uiDevice.swipe(50, fromY, 50, toY, swipeSteps);
    }

    private void initSingleSkp(PaintPreviewTestService service) {
        FrameData singleFrame = new FrameData(new Size(TEST_PAGE_WIDTH, TEST_PAGE_HEIGHT),
                new Rect[] {mInViewportLinkRect, mOutOfViewportLinkRect},
                new String[] {TEST_IN_VIEWPORT_LINK_URL, TEST_OUT_OF_VIEWPORT_LINK_URL},
                new Rect[] {}, new FrameData[] {});
        Assert.assertTrue(service.createFramesForKey(TEST_DIRECTORY_KEY, TEST_URL, singleFrame));
    }

    private void initMultiSkp(PaintPreviewTestService service) {
        // This creates a frame tree of the form
        //
        //    Main
        //    /  \
        //   A    B
        //   |    |
        //   C    D
        //
        // A: Doesn't scroll contains a nested c
        // B: Scrolls contains a nested d out of frame
        // C: Doesn't scroll
        // D: Scrolls

        FrameData childD = new FrameData(new Size(300, 500), new Rect[] {}, new String[] {},
                new Rect[] {}, new FrameData[] {});
        FrameData childB =
                new FrameData(new Size(900, 3000), new Rect[] {new Rect(50, 2300, 250, 2800)},
                        new String[] {TEST_OUT_OF_VIEWPORT_LINK_URL},
                        new Rect[] {new Rect(50, 2000, 150, 2100)}, new FrameData[] {childD});

        // Link is located at 200, 200.
        FrameData childC = new FrameData(new Size(400, 200),
                new Rect[] {new Rect(50, 50, 300, 200)}, new String[] {TEST_IN_VIEWPORT_LINK_URL},
                new Rect[] {}, new FrameData[] {});
        FrameData childA = new FrameData(new Size(500, 300), new Rect[] {}, new String[] {},
                new Rect[] {new Rect(50, 50, 450, 250)}, new FrameData[] {childC});

        FrameData rootFrame = new FrameData(new Size(TEST_PAGE_WIDTH, TEST_PAGE_HEIGHT),
                new Rect[] {mInViewportLinkRect, mOutOfViewportLinkRect},
                new String[] {TEST_IN_VIEWPORT_LINK_URL, TEST_OUT_OF_VIEWPORT_LINK_URL},
                new Rect[] {new Rect(100, 100, 600, 400), new Rect(50, 1000, 900, 2000)},
                new FrameData[] {childA, childB});
        Assert.assertTrue(service.createFramesForKey(TEST_DIRECTORY_KEY, TEST_URL, rootFrame));
    }

    private void initPlayerManager(boolean multiSkp) {
        mLinkClickHandler = new TestLinkClickHandler();
        mRefreshedCallback = new CallbackHelper();
        CallbackHelper viewReady = new CallbackHelper();
        mInitializationFailed = false;

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            PaintPreviewTestService service =
                    new PaintPreviewTestService(mTempFolder.getRoot().getPath());
            if (multiSkp) {
                initMultiSkp(service);
            } else {
                initSingleSkp(service);
            }

            mPlayerManager = new PlayerManager(new GURL(TEST_URL), getActivity(), service,
                    TEST_DIRECTORY_KEY, new PlayerManager.Listener() {
                        @Override
                        public void onCompositorError(int status) {
                            mInitializationFailed = true;
                        }

                        @Override
                        public void onViewReady() {
                            viewReady.notifyCalled();
                        }

                        @Override
                        public void onFirstPaint() {}

                        @Override
                        public void onUserInteraction() {}

                        @Override
                        public void onUserFrustration() {}

                        @Override
                        public void onPullToRefresh() {
                            mRefreshedCallback.notifyCalled();
                        }

                        @Override
                        public void onLinkClick(GURL url) {
                            mLinkClickHandler.onLinkClicked(url);
                        }
                    }, 0xffffffff, false);
            mPlayerManager.setCompressOnClose(false);
            getActivity().setContentView(mPlayerManager.getView());
        });

        // Wait until PlayerManager is initialized.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    "PlayerManager was not initialized.", mPlayerManager, Matchers.notNullValue());
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        try {
            viewReady.waitForFirst();
        } catch (Exception e) {
            if (mInitializationFailed) {
                Assert.fail("Compositor intialization failed.");
            } else {
                Assert.fail("View ready was not called.");
            }
        }

        // Assert that the player view is added to the player host view.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Player view is not added to the host view.",
                    ((ViewGroup) mPlayerManager.getView()).getChildCount(),
                    Matchers.greaterThan(0));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Required bitmaps were not loaded.",
                    mPlayerManager.checkRequiredBitmapsLoadedForTest(), Matchers.is(true));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        if (mInitializationFailed) {
            Assert.fail("Compositor may have crashed.");
        }
    }

    /*
     * Scales the provided coordinate to be view relative
     */
    private int scaleAbsoluteCoordinateToViewCoordinate(int coordinate) {
        float scaleFactor = (float) mPlayerManager.getView().getWidth() / (float) TEST_PAGE_WIDTH;
        return Math.round((float) coordinate * scaleFactor);
    }

    /*
     * Asserts that the expectedUrl is found in the view at absolute coordinates x and y.
     */
    private void assertLinkUrl(View view, int x, int y, String expectedUrl) {
        int scaledX = scaleAbsoluteCoordinateToViewCoordinate(x);
        int scaledY = scaleAbsoluteCoordinateToViewCoordinate(y);

        // In this test scaledY will only exceed the view height if scrolled to the bottom of a
        // page.
        if (scaledY > view.getHeight()) {
            scaledY = view.getHeight()
                    - (scaleAbsoluteCoordinateToViewCoordinate(TEST_PAGE_HEIGHT) - scaledY);
        }

        mLinkClickHandler.mUrl = null;

        int[] locationXY = new int[2];
        view.getLocationOnScreen(locationXY);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.click(scaledX + locationXY[0], scaledY + locationXY[1]);

        CriteriaHelper.pollUiThread(() -> {
            GURL url = mLinkClickHandler.mUrl;
            String msg = "Link press on abs (" + x + ", " + y + ") failed.";
            Criteria.checkThat(msg, url, Matchers.notNullValue());
            Criteria.checkThat(msg, url.getSpec(), Matchers.is(expectedUrl));
        }, TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
