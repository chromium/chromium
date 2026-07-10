// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

import java.util.Set;

/** Manages the visibility and placement of the WebContent hairline for SideUI. */
@NullMarked
@DoNotMock
/* package */ final class SideUiWebContentHairlineManager {

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final SideUiStateProvider mSideUiStateProvider;

    private final WebContentHairlineControlsObserver mWebContentHairlineControlsObserver;
    private final WebContentHairlineAdjuster mWebContentHairlineAdjuster;

    /**
     * Creates a {@link SideUiWebContentHairlineManager}.
     *
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to observe top
     *     controls changes.
     * @param sideUiStateProvider The {@link SideUiStateProvider} to observe SideUI changes.
     * @param sideUiWebContentHairlineContainer The group that contains the WebContent hairlines.
     */
    /* package */ SideUiWebContentHairlineManager(
            BrowserControlsStateProvider browserControlsStateProvider,
            SideUiStateProvider sideUiStateProvider,
            SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mSideUiStateProvider = sideUiStateProvider;

        mWebContentHairlineControlsObserver =
                new WebContentHairlineControlsObserver(
                        browserControlsStateProvider, sideUiWebContentHairlineContainer);
        browserControlsStateProvider.addObserver(mWebContentHairlineControlsObserver);

        mWebContentHairlineAdjuster =
                new WebContentHairlineAdjuster(sideUiWebContentHairlineContainer);
        sideUiStateProvider.addObserver(mWebContentHairlineAdjuster);
    }

    /** Destroys all owned objects. */
    /* package */ void destroy() {
        mBrowserControlsStateProvider.removeObserver(mWebContentHairlineControlsObserver);
        mSideUiStateProvider.removeObserver(mWebContentHairlineAdjuster);
    }

    /**
     * Implementation of {@link BrowserControlsStateProvider.Observer} that updates the height of
     * the side hairlines and the visibility of the top hairline based on top controls changes.
     */
    private static final class WebContentHairlineControlsObserver
            implements BrowserControlsStateProvider.Observer {

        private final BrowserControlsStateProvider mBrowserControlsStateProvider;
        private final SideUiWebContentHairlineContainer mSideUiWebContentHairlineContainer;

        WebContentHairlineControlsObserver(
                BrowserControlsStateProvider browserControlsStateProvider,
                SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mSideUiWebContentHairlineContainer = sideUiWebContentHairlineContainer;
        }

        @Override
        public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
            updateWebContentHairlineContainer();
        }

        @Override
        public void onControlsOffsetChanged(
                int topOffset,
                int topControlsMinHeightOffset,
                boolean topControlsMinHeightChanged,
                int bottomOffset,
                int bottomControlsMinHeightOffset,
                boolean bottomControlsMinHeightChanged,
                boolean requestNewFrame,
                boolean isVisibilityForced) {
            updateWebContentHairlineContainer();
        }

        private void updateWebContentHairlineContainer() {
            // Hides the top hairline, if needed.
            int topVisibleContentOffset =
                    (int) mBrowserControlsStateProvider.getTopVisibleContentOffset();
            boolean hideTopHairline = topVisibleContentOffset == 0;
            mSideUiWebContentHairlineContainer
                    .getTopHairline()
                    .setVisibility(hideTopHairline ? View.INVISIBLE : View.VISIBLE);

            // Adjusts the top margin based on how far the content is offset.
            MarginLayoutParams layoutParams =
                    (MarginLayoutParams) mSideUiWebContentHairlineContainer.getLayoutParams();
            layoutParams.topMargin = topVisibleContentOffset;
            mSideUiWebContentHairlineContainer.setLayoutParams(layoutParams);
        }
    }

    /**
     * Extension of {@link ViewMarginAdjusterForSideUi} that also sets the {@link
     * SideUiWebContentHairlineContainer}'s hairline visibility.
     */
    private static final class WebContentHairlineAdjuster extends ViewMarginAdjusterForSideUi {

        private final SideUiWebContentHairlineContainer mSideUiWebContentHairlineContainer;

        public WebContentHairlineAdjuster(
                SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
            super(sideUiWebContentHairlineContainer);

            mSideUiWebContentHairlineContainer = sideUiWebContentHairlineContainer;
        }

        @Override
        public Set<Transition> createTransitions() {
            return Set.of(new ChangeBounds(), new Fade());
        }

        @Override
        public void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs) {
            int leftHairlineVisibility =
                    sideUiSpecs.getWidth(AnchorSide.LEFT) == 0 ? View.INVISIBLE : View.VISIBLE;
            mSideUiWebContentHairlineContainer
                    .getLeftHairline()
                    .setVisibility(leftHairlineVisibility);

            int rightHairlineVisibility =
                    sideUiSpecs.getWidth(AnchorSide.RIGHT) == 0 ? View.INVISIBLE : View.VISIBLE;
            mSideUiWebContentHairlineContainer
                    .getRightHairline()
                    .setVisibility(rightHairlineVisibility);

            super.onSideUiSpecsChanged(sideUiSpecs);
        }
    }
}
