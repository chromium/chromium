// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static java.util.Collections.emptySet;

import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.ui.base.ViewUtils;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Set;

/**
 * Observer for side UI changes for containers that account for the side UI by using margins. This
 * observer accounts for pre-existing margins applied to the View before it's constructed (e.g. via
 * XML) but will not account for padding applied programmatically after construction.
 */
@NullMarked
public class ViewMarginAdjusterForSideUi implements SideUiObserver {
    private final View mView;
    private final int mBaseLeftMargin;
    private final int mBaseRightMargin;
    private final boolean mForToolbarElement;

    /**
     * Constructs an observer to adjust a View's margins to account for all {@link
     * SideUiContainer}s. Always respects the containers' width, and shrinks View's width if needed.
     *
     * @param view The view to which margins should be applied.
     */
    public ViewMarginAdjusterForSideUi(View view) {
        this(view, /* forToolbarElement= */ false);
    }

    /**
     * Constructs an observer to adjust a View's margins to account for all {@link
     * SideUiContainer}s.
     *
     * @param view The view to which margins should be applied.
     * @param forToolbarElement Whether this adjuster is for a toolbar element. If true, it takes
     *     effect only if the {@link SideUiContainer} has height type {@code TOOLBAR}. For other
     *     type {@code WEB_CONTENTS}, this is a no-op.
     */
    public ViewMarginAdjusterForSideUi(View view, boolean forToolbarElement) {
        mView = view;
        mForToolbarElement = forToolbarElement;

        // Save the existing, base margins for the container view. Margins added to account for side
        // UI will be added onto these base margins to avoid overwriting pre-existing values.
        assert mView.getLayoutParams() instanceof MarginLayoutParams;
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        mBaseLeftMargin = layoutParams.leftMargin;
        mBaseRightMargin = layoutParams.rightMargin;
    }

    /**
     * Returns a list of Transitions that should target the view and all of its descendants. If not
     * specified, only {@link ChangeBounds} will be applied.
     */
    public Set<Transition> createTransitions() {
        return Set.of(new ChangeBounds());
    }

    @Override
    public @Nullable Transition onPreSideUiSpecsChange(SideUiCoordinator.SideUiSpecs sideUiSpecs) {
        TransitionSet transitionSet = new TransitionSet();
        Collection<View> descendants = new ArrayList<>();
        ViewUtils.getAllDescendants(mView, descendants, emptySet());

        for (Transition transition : createTransitions()) {
            transition.addTarget(mView);
            for (View view : descendants) {
                transition.addTarget(view);
            }
            transitionSet.addTransition(transition);
        }
        return transitionSet;
    }

    @Override
    public void onSideUiSpecsChanged(SideUiCoordinator.SideUiSpecs sideUiSpecs) {
        MarginLayoutParams params = (MarginLayoutParams) mView.getLayoutParams();
        int leftMargin = 0;
        int rightMargin = 0;
        if (!mForToolbarElement
                || sideUiSpecs.getHeightType(AnchorSide.LEFT) == HeightType.TOOLBAR) {
            leftMargin = sideUiSpecs.getWidth(AnchorSide.LEFT);
        }
        if (!mForToolbarElement
                || sideUiSpecs.getHeightType(AnchorSide.RIGHT) == HeightType.TOOLBAR) {
            rightMargin = sideUiSpecs.getWidth(AnchorSide.RIGHT);
        }

        params.leftMargin = mBaseLeftMargin + leftMargin;
        params.rightMargin = mBaseRightMargin + rightMargin;
        mView.setLayoutParams(params);
    }

    /**
     * Trigger a synchronous measure and layout pass for the View to ensure the layout is properly
     * updated for any pre-transition changes.
     */
    public void triggerSynchronousMeasureAndLayout() {
        ViewUtils.triggerSynchronousMeasureAndLayout(mView);
    }
}
