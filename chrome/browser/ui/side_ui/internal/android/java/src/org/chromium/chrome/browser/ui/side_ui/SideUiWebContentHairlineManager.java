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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

import java.util.Set;

/** Manages the visibility and placement of the WebContent hairline for SideUI. */
@NullMarked
@DoNotMock
/* package */ final class SideUiWebContentHairlineManager {

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final SideUiStateProvider mSideUiStateProvider;
    private final IncognitoStateProvider mIncognitoStateProvider;

    private final WebContentHairlineControlsObserver mWebContentHairlineControlsObserver;
    private final WebContentHairlineAdjuster mWebContentHairlineAdjuster;
    private final WebContentHairlineIncognitoObserver mWebContentHairlineIncognitoObserver;

    /**
     * Creates a {@link SideUiWebContentHairlineManager}.
     *
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to observe top
     *     controls changes.
     * @param sideUiStateProvider The {@link SideUiStateProvider} to observe SideUI changes.
     * @param sideUiWebContentHairlineContainer The group that contains the WebContent hairlines.
     * @param incognitoStateProvider The {@link IncognitoStateProvider} to observe incognito state.
     */
    /* package */ SideUiWebContentHairlineManager(
            BrowserControlsStateProvider browserControlsStateProvider,
            SideUiStateProvider sideUiStateProvider,
            SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer,
            IncognitoStateProvider incognitoStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mSideUiStateProvider = sideUiStateProvider;
        mIncognitoStateProvider = incognitoStateProvider;

        mWebContentHairlineControlsObserver =
                new WebContentHairlineControlsObserver(
                        browserControlsStateProvider,
                        sideUiStateProvider,
                        sideUiWebContentHairlineContainer);
        browserControlsStateProvider.addObserver(mWebContentHairlineControlsObserver);
        mWebContentHairlineControlsObserver.updateWebContentHairlineContainer();

        mWebContentHairlineAdjuster =
                new WebContentHairlineAdjuster(
                        sideUiStateProvider, sideUiWebContentHairlineContainer);
        sideUiStateProvider.addObserver(mWebContentHairlineAdjuster);

        mWebContentHairlineIncognitoObserver =
                new WebContentHairlineIncognitoObserver(sideUiWebContentHairlineContainer);
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(
                mWebContentHairlineIncognitoObserver);
    }

    /** Destroys all owned objects. */
    /* package */ void destroy() {
        mBrowserControlsStateProvider.removeObserver(mWebContentHairlineControlsObserver);
        mSideUiStateProvider.removeObserver(mWebContentHairlineAdjuster);
        mIncognitoStateProvider.removeObserver(mWebContentHairlineIncognitoObserver);
    }

    /** Updates the WebContent hairline container. */
    /* package */ void update() {
        mWebContentHairlineControlsObserver.updateWebContentHairlineContainer();
    }

    /**
     * Implementation of {@link BrowserControlsStateProvider.Observer} that updates the height of
     * the side hairlines and the visibility of the top hairline based on top controls changes.
     */
    private static final class WebContentHairlineControlsObserver
            implements BrowserControlsStateProvider.Observer {

        private final BrowserControlsStateProvider mBrowserControlsStateProvider;
        private final SideUiStateProvider mSideUiStateProvider;
        private final SideUiWebContentHairlineContainer mSideUiWebContentHairlineContainer;

        WebContentHairlineControlsObserver(
                BrowserControlsStateProvider browserControlsStateProvider,
                SideUiStateProvider sideUiStateProvider,
                SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mSideUiStateProvider = sideUiStateProvider;
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

        /* package */ void updateWebContentHairlineContainer() {
            // Hides the top hairline, if needed.
            int topVisibleContentOffset =
                    (int) mBrowserControlsStateProvider.getTopVisibleContentOffset();
            boolean hideTopHairline =
                    !ChromeFeatureList.sSidePanelTopHairlineRefactorAndroid.isEnabled()
                            || topVisibleContentOffset == 0
                            || !mSideUiStateProvider.isAnySideUiShowing();
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

        private final SideUiStateProvider mSideUiStateProvider;
        private final SideUiWebContentHairlineContainer mSideUiWebContentHairlineContainer;

        public WebContentHairlineAdjuster(
                SideUiStateProvider sideUiStateProvider,
                SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
            super(sideUiWebContentHairlineContainer);

            mSideUiStateProvider = sideUiStateProvider;
            mSideUiWebContentHairlineContainer = sideUiWebContentHairlineContainer;
        }

        @Override
        public Set<Transition> createTransitions() {
            return Set.of(new ChangeBounds(), new Fade());
        }

        @Override
        public void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs) {
            // TODO(crbug.com/525353575): Determine the innermost side UI to figure out which
            //  corner to show when supporting VT and SP on the same side.
            boolean isLeftShowing = sideUiSpecs.getWidth(AnchorSide.LEFT) != 0;
            boolean isVtShowing = mSideUiStateProvider.isSideUiShowing(SideUiId.VERTICAL_TABS);

            int leftHairlineVisibility =
                    (isLeftShowing && !isVtShowing) ? View.VISIBLE : View.INVISIBLE;
            mSideUiWebContentHairlineContainer
                    .getLeftHairline()
                    .setVisibility(leftHairlineVisibility);
            mSideUiWebContentHairlineContainer
                    .getTopLeftRoundedCorner()
                    .setVisibility(leftHairlineVisibility);

            int leftBottomCornerVisibility =
                    (isLeftShowing && isVtShowing) ? View.VISIBLE : View.INVISIBLE;
            mSideUiWebContentHairlineContainer
                    .getBottomLeftRoundedCorner()
                    .setVisibility(leftBottomCornerVisibility);

            int rightHairlineVisibility =
                    sideUiSpecs.getWidth(AnchorSide.RIGHT) == 0 ? View.INVISIBLE : View.VISIBLE;
            mSideUiWebContentHairlineContainer
                    .getRightHairline()
                    .setVisibility(rightHairlineVisibility);
            mSideUiWebContentHairlineContainer
                    .getTopRightRoundedCorner()
                    .setVisibility(rightHairlineVisibility);

            super.onSideUiSpecsChanged(sideUiSpecs);
        }
    }

    /**
     * Implementation of {@link IncognitoStateObserver} that updates the colors of the hairlines and
     * rounded corners when incognito mode changes.
     */
    private static final class WebContentHairlineIncognitoObserver
            implements IncognitoStateObserver {
        private final SideUiWebContentHairlineContainer mSideUiWebContentHairlineContainer;

        WebContentHairlineIncognitoObserver(
                SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
            mSideUiWebContentHairlineContainer = sideUiWebContentHairlineContainer;
        }

        @Override
        public void onIncognitoStateChanged(boolean isIncognito) {
            mSideUiWebContentHairlineContainer.setIncognitoState(isIncognito);
        }
    }
}
