// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;
import android.util.Size;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.core.graphics.Insets;
import androidx.core.os.BuildCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type.InsetsType;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.widget.InsetsRectProvider.BoundingRectHelper;
import org.chromium.components.browser_ui.widget.InsetsRectProviderTest.ShadowBoundingRectHelper;
import org.chromium.components.browser_ui.widget.InsetsRectProviderTest.ShadowBuildCompat;
import org.chromium.components.browser_ui.widget.InsetsRectProviderTest.ShadowWindowInsetsUtils;
import org.chromium.ui.util.WindowInsetsUtils;

import java.util.List;

/**
 * Unit test for {@link InsetsRectProvider}. Since most of the calculations were done in {@link
 * WindowInsetsUtils}, this test is mostly used to test if rects are up-to-date for observation when
 * certain window insets has an update.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        sdk = 30,
        shadows = {
            ShadowBuildCompat.class,
            ShadowWindowInsetsUtils.class,
            ShadowBoundingRectHelper.class
        })
public class InsetsRectProviderTest {
    private static final int WINDOW_WIDTH = 600;
    private static final int WINDOW_HEIGHT = 800;

    private InsetsRectProvider mInsetsRectProvider;

    @Mock private View mView;
    @Mock private InsetObserver mInsetObserver;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
    }

    @After
    public void tearDown() {
        resetShadows();
    }

    @Test
    public void testInitialization() {
        // Assume a top insets
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 10, 0, 0);
        List<Rect> blockingRects =
                List.of(new Rect(0, 0, 10, 10), new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 10));
        Rect availableArea = new Rect(10, 0, WINDOW_WIDTH - 20, 10);

        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(type, insets, availableArea, blockingRects);
        mInsetsRectProvider = new InsetsRectProvider(mInsetObserver, type, windowInsets);

        assertSuppliedValues(insets, availableArea, blockingRects);
    }

    @Test
    public void testInitializationEmpty() {
        int type = WindowInsetsCompat.Type.captionBar();
        mInsetsRectProvider = new InsetsRectProvider(mInsetObserver, type, null);

        assertSuppliedValues(Insets.NONE, new Rect(), List.of());
    }

    @Test
    public void testObservation() {
        // Assume inset is at the bottom for this test.
        int type = WindowInsetsCompat.Type.navigationBars();
        Insets insets = Insets.of(0, 0, 0, 10);
        List<Rect> blockingRects =
                List.of(
                        new Rect(0, WINDOW_HEIGHT - 10, 10, WINDOW_HEIGHT),
                        new Rect(
                                WINDOW_WIDTH - 20,
                                WINDOW_HEIGHT - 10,
                                WINDOW_WIDTH,
                                WINDOW_HEIGHT));
        Rect availableArea = new Rect(10, WINDOW_HEIGHT - 10, WINDOW_WIDTH - 20, WINDOW_HEIGHT);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider = new InsetsRectProvider(mInsetObserver, type, emptyWindowInsets);
        assertSuppliedValues(Insets.NONE, new Rect(), List.of());

        // Attach an observer and supply a new window insets.
        CallbackHelper observer = new CallbackHelper();
        mInsetsRectProvider.addObserver(rect -> observer.notifyCalled());
        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(type, insets, availableArea, blockingRects);
        mInsetsRectProvider.onApplyWindowInsets(mView, windowInsets);

        assertEquals("Observer not called.", 1, observer.getCallCount());
        assertSuppliedValues(insets, availableArea, blockingRects);
    }

    @Test
    public void testInsetRemoved() {
        // Assume inset is at the top for this test.
        int type = WindowInsetsCompat.Type.statusBars();
        Insets insets = Insets.of(0, 10, 0, 0);
        List<Rect> blockingRects =
                List.of(
                        new Rect(0, WINDOW_HEIGHT - 10, 10, WINDOW_HEIGHT),
                        new Rect(
                                WINDOW_WIDTH - 20,
                                WINDOW_HEIGHT - 10,
                                WINDOW_WIDTH,
                                WINDOW_HEIGHT));
        Rect availableArea = new Rect(10, WINDOW_HEIGHT - 10, WINDOW_WIDTH - 20, WINDOW_HEIGHT);

        // Initialize with valid insets.
        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(type, insets, availableArea, blockingRects);
        mInsetsRectProvider = new InsetsRectProvider(mInsetObserver, type, windowInsets);
        assertSuppliedValues(insets, availableArea, blockingRects);

        // Attach an observer and supply a new window insets.
        CallbackHelper observer = new CallbackHelper();
        mInsetsRectProvider.addObserver(rect -> observer.notifyCalled());

        // Create an insets with a different type so it removes the exists insets.
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        WindowInsetsCompat.Type.systemBars(), Insets.NONE, new Rect(), List.of());
        mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);

        assertEquals("Observer not called.", 1, observer.getCallCount());
        assertSuppliedValues(Insets.NONE, new Rect(), List.of());
    }

    private static WindowInsetsCompat buildTestWindowInsets(
            @InsetsType int type, Insets insets, Rect availableArea, List<Rect> blockingRects) {
        resetShadows();
        ShadowWindowInsetsUtils.sWidestUnoccludedRect = availableArea;
        ShadowBoundingRectHelper.sTestRects = blockingRects;

        // WindowInsetsCompat.Builder does not work in robolectric (always yield an empty Inset).
        WindowInsetsCompat windowInsets = Mockito.mock(WindowInsetsCompat.class);
        doReturn(insets).when(windowInsets).getInsets(eq(type));
        doReturn(Insets.NONE).when(windowInsets).getInsets(not(eq(type)));
        return windowInsets;
    }

    private void assertSuppliedValues(Insets insets, Rect availableArea, List<Rect> blockingRects) {
        assertEquals(
                "Supplied #getBoundingRects is different.",
                blockingRects,
                mInsetsRectProvider.getBoundingRects());
        assertEquals(
                "Supplied #getWidestUnoccludedRect is different.",
                availableArea,
                mInsetsRectProvider.getWidestUnoccludedRect());
        assertEquals(
                "Supplied #getCachedInset is different.",
                insets,
                mInsetsRectProvider.getCachedInset());
    }

    private static void resetShadows() {
        ShadowBoundingRectHelper.sTestRects = null;
        ShadowWindowInsetsUtils.sWidestUnoccludedRect = null;
    }

    // Test only class for bounding rect helper.
    @Implements(BoundingRectHelper.class)
    public static class ShadowBoundingRectHelper {
        static List<Rect> sTestRects;

        @Implementation
        protected static @NonNull List<Rect> getBoundingRects(
                WindowInsetsCompat windowInsetsCompact,
                @WindowInsetsCompat.Type.InsetsType int typeMask) {
            return sTestRects != null ? sTestRects : List.of();
        }

        @Implementation
        protected static Size getFrame(WindowInsetsCompat windowInsetsCompact) {
            return new Size(WINDOW_WIDTH, WINDOW_HEIGHT);
        }
    }

    /** Helper class to get the class tested on V. */
    @Implements(BuildCompat.class)
    public static class ShadowBuildCompat {
        @Implementation
        protected static boolean isAtLeastV() {
            return true;
        }
    }

    /**
     * Helper class to get the result from {@link #getWidestUnoccludedRect} without trigger the
     * actual calculation.
     */
    @Implements(WindowInsetsUtils.class)
    public static class ShadowWindowInsetsUtils {
        static Rect sWidestUnoccludedRect;

        @Implementation
        protected static Rect getWidestUnoccludedRect(Rect regionRect, List<Rect> blockRects) {
            return sWidestUnoccludedRect != null ? sWidestUnoccludedRect : new Rect();
        }
    }
}
