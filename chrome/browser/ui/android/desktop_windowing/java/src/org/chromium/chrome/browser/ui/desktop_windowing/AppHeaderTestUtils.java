// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;

import androidx.core.graphics.Insets;

import org.chromium.ui.insets.InsetsRectProvider;

import java.util.List;

/** Util class for app header related tests. */
public final class AppHeaderTestUtils {

    /**
     * Helper class that sets up mock caption bar insets related values to achieve a desired {@link
     * DesktopWindowHeuristicResult} for a given mock {@link InsetsRectProvider}.
     */
    // TODO(wenyufu): Make AppHeaderCoordinatorUnitTest use this.
    public static class MockCaptionBarInsetsSetter {
        public final Rect windowRect = new Rect(0, 0, 600, 800);
        public final Insets captionInsets;
        public final List<Rect> boundingRects;
        public final Rect widestUnoccludedRect;

        private MockCaptionBarInsetsSetter(
                Insets captionInsets, List<Rect> boundingRects, Rect widestUnoccludedRect) {
            this.captionInsets = captionInsets;
            this.boundingRects = boundingRects;
            this.widestUnoccludedRect = widestUnoccludedRect;
        }

        /**
         * Create mock insets that matches {@link DesktopWindowHeuristicResult#IN_DESKTOP_WINDOW}.
         */
        public static MockCaptionBarInsetsSetter standardDesktopWindow() {
            return new MockCaptionBarInsetsSetter(
                    /* captionInsets= */ Insets.of(0, 30, 0, 0),
                    /* boundingRects= */ List.of(new Rect(0, 0, 10, 30), new Rect(580, 0, 600, 30)),
                    /* widestUnoccludedRect= */ new Rect(10, 0, 580, 30));
        }

        /**
         * Create mock insets that matches {@link
         * DesktopWindowHeuristicResult#CAPTION_BAR_TOP_INSETS_ABSENT}.
         */
        public static MockCaptionBarInsetsSetter empty() {
            return new MockCaptionBarInsetsSetter(
                    /* captionInsets= */ Insets.NONE,
                    /* boundingRects= */ List.of(),
                    /* widestUnoccludedRect= */ new Rect());
        }

        /** Apply the mock insets values to the mock {@link InsetsRectProvider}. */
        public void apply(InsetsRectProvider mockProvider) {
            doReturn(windowRect).when(mockProvider).getWindowRect();
            doReturn(widestUnoccludedRect).when(mockProvider).getWidestUnoccludedRect();
            doReturn(captionInsets).when(mockProvider).getCachedInset();
            doReturn(boundingRects).when(mockProvider).getBoundingRects();
        }
    }
}
