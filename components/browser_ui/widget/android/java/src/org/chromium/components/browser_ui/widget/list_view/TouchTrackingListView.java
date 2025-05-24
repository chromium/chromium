// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/**
 * A custom {@link ListView} that tracks touch events.
 *
 * <p>This class is useful when we need information about touch events on individual items in a
 * {@link ListView}, since it's not straightforward to gain access to an individual item and
 * register a {@link android.view.View.OnTouchListener}.
 *
 * <p>This class won't consume any event; it only watches them.
 *
 * <p>Usage example:
 *
 * <pre>
 *     TouchTrackingListView touchTrackingListView = findViewById(...);
 *     touchTrackingListView.setAdapter(...);
 *     touchTrackingListView.setOnItemClickListener(
 *       (parent, view, position, id) ->
 *         // Note: "touchTrackingListView" is also a "ListViewTouchTracker".
 *         onItemClicked(touchTrackingListView));
 *
 *     ...
 *
 *     // Tip: to make onItemClicked() easier to test, prefer to pass the ListViewTouchTracker
 *     // interface instead of the TouchTrackingListView.
 *     void onItemClicked(ListViewTouchTracker touchTracker) {
 *       if (touchTracker.getLastSingleTapUp() != null) {
 *         // Do something with the touch event.
 *       }
 *     }
 * </pre>
 */
@NullMarked
public final class TouchTrackingListView extends ListView implements ListViewTouchTracker {

    private final GestureDetector mGestureDetector;

    private final GestureDetector.SimpleOnGestureListener mGestureListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onSingleTapUp(MotionEvent e) {
                    mLastSingleTapUpInfo = MotionEventInfo.fromMotionEvent(e);
                    return true;
                }
            };

    private @Nullable MotionEventInfo mLastSingleTapUpInfo;

    public TouchTrackingListView(Context context) {
        this(context, /* attrs= */ null);
    }

    public TouchTrackingListView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, /* defStyleAttr= */ 0);
    }

    public TouchTrackingListView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, /* defStyleRes= */ 0);
    }

    public TouchTrackingListView(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        mGestureDetector = new GestureDetector(context, mGestureListener);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        mGestureDetector.onTouchEvent(ev);
        return false;
    }

    @Override
    public @Nullable MotionEventInfo getLastSingleTapUp() {
        return mLastSingleTapUpInfo;
    }
}
