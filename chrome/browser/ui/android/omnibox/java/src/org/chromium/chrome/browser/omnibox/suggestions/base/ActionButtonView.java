// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;

import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;

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

    public ActionButtonView(Context context) {
        super(context);
        // No background should be set to allow seeing through its parent view.
        setClickable(true);
        setFocusable(true);
        setScaleType(ImageView.ScaleType.CENTER);
        setForeground(context.getDrawable(R.drawable.action_button_foreground_selector));
        setOnClickListener(
                v -> {
                    Action action = (Action) v.getTag();
                    if (action != null) {
                        action.callback.run();
                    }
                });
        setAccessibilityDelegate(
                new AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.addAction(AccessibilityAction.ACTION_CLICK);
                    }

                    @Override
                    public boolean performAccessibilityAction(
                            View host, int accessibilityAction, @Nullable Bundle arguments) {
                        Action action = (Action) host.getTag();
                        if (accessibilityAction == AccessibilityNodeInfo.ACTION_CLICK
                                && action != null
                                && action.onClickAnnouncement != null) {
                            host.setContentDescription(action.onClickAnnouncement);
                            host.sendAccessibilityEvent(
                                    AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
                        }
                        return super.performAccessibilityAction(
                                host, accessibilityAction, arguments);
                    }
                });
    }

    /**
     * Sets whether the action button should only be shown when the suggestion is focused.
     *
     * @param showOnlyOnFocus Whether the action button should only be shown on focus.
     */
    public void enableShowOnlyOnFocus(boolean showOnlyOnFocus) {
        mShowOnlyOnFocus = showOnlyOnFocus;
        if (mShowOnlyOnFocus) {
            updateVisibility();
        } else {
            setVisibility(View.VISIBLE);
        }
    }

    /**
     * Called when the hover state of parent view changes.
     *
     * @param hovered The current hover state.
     */
    public void onParentViewHoverChanged(boolean hovered) {
        mParentHovered = hovered;
        updateVisibility();
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
        setVisibility(
                mParentHovered || mParentSelected || isHovered() || isPressed()
                        ? View.VISIBLE
                        : View.INVISIBLE);
    }

    @Override
    public void setPressed(boolean pressed) {
        super.setPressed(pressed);
        updateVisibility();
    }

    @Override
    public void setHovered(boolean hovered) {
        super.setHovered(hovered);
        updateVisibility();
    }

    void dispatchHoverEventForTesting(MotionEvent event) {
        dispatchHoverEvent(event);
    }
}
