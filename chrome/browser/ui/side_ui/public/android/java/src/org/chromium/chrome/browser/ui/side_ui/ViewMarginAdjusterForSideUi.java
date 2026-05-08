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
    private final int mBaseStartMargin;
    private final int mBaseEndMargin;

    /**
     * Constructs an observer to adjust a View's margins to account for side UI.
     *
     * @param view The view to which margins should be applied.
     */
    public ViewMarginAdjusterForSideUi(View view) {
        mView = view;

        // Save the existing, base margins for the container view. Margins added to account for side
        // UI will be added onto these base margins to avoid overwriting pre-existing values.
        assert mView.getLayoutParams() instanceof MarginLayoutParams;
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        mBaseStartMargin = layoutParams.getMarginStart();
        mBaseEndMargin = layoutParams.getMarginEnd();
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
        params.setMarginStart(mBaseStartMargin + sideUiSpecs.mStartContainerWidth);
        params.setMarginEnd(mBaseEndMargin + sideUiSpecs.mEndContainerWidth);
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
