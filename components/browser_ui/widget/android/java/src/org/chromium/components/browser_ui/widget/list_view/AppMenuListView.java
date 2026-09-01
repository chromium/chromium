// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.KeyNavigationUtil;

/**
 * A custom {@link ListView} used by AppMenu that extends {@link TouchTrackingListView} to handle
 * keyboard navigation (Tab and Shift+Tab) between focusable sibling views inside composite list
 * item rows (e.g. Page Zoom menu item) before standard list row selection navigation takes over.
 */
@NullMarked
public class AppMenuListView extends TouchTrackingListView {

    public AppMenuListView(Context context) {
        this(context, null);
    }

    public AppMenuListView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (KeyNavigationUtil.isTabNavigation(event)) {
            View focusedRow = getFocusedChild();
            if (focusedRow != null) {
                View focusedChild = focusedRow.findFocus();
                if (focusedChild != null) {
                    int direction =
                            KeyNavigationUtil.isTabBackward(event)
                                    ? View.FOCUS_BACKWARD
                                    : View.FOCUS_FORWARD;
                    @SuppressWarnings("WrongConstant")
                    View nextFocus = focusedChild.focusSearch(direction);

                    if (nextFocus != null && nextFocus != focusedChild) {
                        int currentRowPosition = getPositionForView(focusedRow);
                        int nextRowPosition = getPositionForView(nextFocus);

                        if (currentRowPosition != AdapterView.INVALID_POSITION
                                && nextRowPosition != AdapterView.INVALID_POSITION) {
                            // If the target focus view is deeply nested inside a row (e.g. Zoom In
                            // button), we intercept and manually sync ListView state. If the target
                            // is a raw monolithic row (direct child of ListView), we let the native
                            // Android ListView handle the standard focus jump.
                            boolean shouldIntercept = nextFocus.getParent() != AppMenuListView.this;

                            if (shouldIntercept) {
                                setSelection(nextRowPosition);
                                if (nextFocus.requestFocus()) {
                                    return true;
                                }
                            }
                        } else if (currentRowPosition != AdapterView.INVALID_POSITION
                                && nextRowPosition == AdapterView.INVALID_POSITION) {
                            // Reached the boundary of the menu (e.g. tabbing forward past the last
                            // item or backward past the first item).
                            // Consume the key event instead of falling through to super, which
                            // returns false and triggers Android's window-level fallback focus
                            // navigation (ViewRootImpl.performFocusNavigation), causing focus to
                            // snap back to an earlier row or loop. See https://crbug.com/555764820.
                            return true;
                        }
                    } else if (nextFocus == null) {
                        // Reached the boundary of the focus hierarchy with no focusable target
                        // found in this direction. Consume the event to prevent fallback focus
                        // looping.
                        return true;
                    }
                }
            }
        }
        return super.dispatchKeyEvent(event);
    }
}
