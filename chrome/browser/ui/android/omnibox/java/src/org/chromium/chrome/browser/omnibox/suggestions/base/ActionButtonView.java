// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.View;

import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.build.annotations.NullMarked;

/**
 * A View that is displayed as an action button.
 *
 * <p>The view manages its visibility depending on whether the suggestion is focused.
 */
@NullMarked
public class ActionButtonView extends AppCompatImageView {
    private boolean mShowOnlyOnFocus;
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
        mHovered = hovered;
        updateVisibility();
    }

    /**
     * Called when the selection state of parent view changes.
     *
     * @param selected The current selection state.
     */
    public void onParentViewSelected(boolean selected) {
        mSelected = selected;
        updateVisibility();
    }

    private void updateVisibility() {
        if (!mShowOnlyOnFocus) {
            return;
        }
        setVisibility(mHovered || mSelected ? View.VISIBLE : View.GONE);
    }
}
