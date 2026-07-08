// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.view.View;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

import java.util.Set;

/** Manages the visibility and placement of the WebContent hairline for SideUI. */
@NullMarked
@DoNotMock
/* package */ final class SideUiWebContentHairlineManager {

    private final SideUiStateProvider mSideUiStateProvider;
    private final WebContentHairlineAdjuster mWebContentHairlineAdjuster;

    /**
     * Creates a {@link SideUiWebContentHairlineManager}.
     *
     * @param sideUiStateProvider The {@link SideUiStateProvider} to observe SideUI changes.
     * @param sideUiWebContentHairlineContainer The group that contains the WebContent hairlines.
     */
    /* package */ SideUiWebContentHairlineManager(
            SideUiStateProvider sideUiStateProvider,
            SideUiWebContentHairlineContainer sideUiWebContentHairlineContainer) {
        mSideUiStateProvider = sideUiStateProvider;
        mWebContentHairlineAdjuster =
                new WebContentHairlineAdjuster(sideUiWebContentHairlineContainer);
        sideUiStateProvider.addObserver(mWebContentHairlineAdjuster);
    }

    /** Destroys all owned objects. */
    /* package */ void destroy() {
        mSideUiStateProvider.removeObserver(mWebContentHairlineAdjuster);
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
