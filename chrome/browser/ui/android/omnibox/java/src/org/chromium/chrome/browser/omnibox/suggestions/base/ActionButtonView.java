// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;

/**
 * A View that is displayed as an action button.
 *
 * <p>The view manages its visibility depending on whether the suggestion is focused.
 */
@NullMarked
public class ActionButtonView extends AppCompatImageView {
    private boolean mShowOnlyOnFocus;
    private boolean mParentHovered;
    private boolean mParentSelected;
    private boolean mHovered;
    private boolean mSelected;

    public ActionButtonView(Context context) {
        super(context);
    }

    /**
     * Sets whether the action button should only be shown when the suggestion is focused.
     *
     * @param showOnlyOnFocus Whether the action button should only be shown on focus.
     */
    public void enableShowOnlyOnFocus(boolean showOnlyOnFocus) {
        mShowOnlyOnFocus = showOnlyOnFocus;
        setVisibility(mShowOnlyOnFocus ? View.GONE : View.VISIBLE);
    }

    /**
     * Called when the hover state of parent view changes.
     *
     * @param hovered The current hover state.
     */
    public void onParentViewHoverChanged(boolean hovered) {
        mParentHovered = hovered;
        updateVisibility();
        setHovered(hovered);
    }

    /**
     * Called when the selection state of parent view changes.
     *
     * @param selected The current selection state.
     */
    public void onParentViewSelected(boolean selected) {
        mParentSelected = selected;
        updateVisibility();
    }

    private void updateVisibility() {
        if (!mShowOnlyOnFocus) {
            return;
        }
        // Use post to decouple the input event stream from the view hierarchy state update in order
        // to work around the timing problem that causes the system not to dispatch the touch event.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    setVisibility(
                            mParentHovered || mHovered || mParentSelected
                                    ? View.VISIBLE
                                    : View.GONE);
                });
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        boolean result = super.onHoverEvent(event);

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_HOVER_ENTER || action == MotionEvent.ACTION_HOVER_EXIT) {
            mHovered = action == MotionEvent.ACTION_HOVER_ENTER;
            updateVisibility();
            updateVisualStyle();
        }

        return result;
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        mSelected = selected;
        updateVisualStyle();
    }

    public boolean isActionButtonHovered() {
        return mHovered;
    }

    void dispatchHoverEventForTesting(MotionEvent event) {
        dispatchHoverEvent(event);
    }

    private void updateVisualStyle() {
        if (getVisibility() != View.VISIBLE) return;
        @DrawableRes
        int resId =
                mSelected
                        ? (mHovered
                                ? R.drawable.action_button_selected_hovered
                                : R.drawable.action_button_selected)
                        : (mHovered ? R.drawable.action_button_hovered : 0);
        setForeground(
                (resId != 0) ? OmniboxResourceProvider.getDrawable(getContext(), resId) : null);
    }
}
