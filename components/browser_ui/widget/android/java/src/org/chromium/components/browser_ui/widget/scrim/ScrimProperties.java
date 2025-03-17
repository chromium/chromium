// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.graphics.Color;
import android.view.GestureDetector;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.TouchEventDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties that can be used to describe the behavior of the scrim widget. */
@NullMarked
public class ScrimProperties {
    /**
     * An invalid color that can be specified for {@link #BACKGROUND_COLOR}. This will trigger the
     * use of the default color set when the {@link ScrimManager} was constructed.
     */
    public static final @ColorInt int INVALID_COLOR = Color.TRANSPARENT;

    /**
     * The top margin of the scrim. This can be used to shrink the scrim to show items at the top of
     * the screen.
     */
    public static final WritableIntPropertyKey TOP_MARGIN = new WritableIntPropertyKey();

    /**
     * The bottom margin of the scrim. This can be used to shrink the scrim to show items, such as
     * the bottom toolbar, at the bottom of the screen.
     */
    public static final WritableIntPropertyKey BOTTOM_MARGIN = new WritableIntPropertyKey();

    /** Whether the scrim should affect the status bar color. */
    public static final ReadableBooleanPropertyKey AFFECTS_STATUS_BAR =
            new ReadableBooleanPropertyKey();

    /** The view that the scrim is using to place itself in the hierarchy. */
    public static final ReadableObjectPropertyKey<View> ANCHOR_VIEW =
            new ReadableObjectPropertyKey<>();

    /** Whether the scrim should show in front of the anchor view. */
    public static final ReadableBooleanPropertyKey SHOW_IN_FRONT_OF_ANCHOR_VIEW =
            new ReadableBooleanPropertyKey();

    /** A callback for updates to the scrim's visibility. */
    public static final ReadableObjectPropertyKey<Callback<Boolean>> VISIBILITY_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** A callback to be run when the scrim is clicked. */
    public static final ReadableObjectPropertyKey<Runnable> CLICK_DELEGATE =
            new ReadableObjectPropertyKey<>();

    /**
     * The transparency of the scrim. This is an internal property that only the scrim knows about.
     */
    /* package */ static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();

    /**
     * The background color for the scrim. If not set a default color will be set as the background.
     */
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    /**
     * A filter for touch event that happen on this view.
     *
     * <p>The filter intercepts click events, which means that {@link #CLICK_DELEGATE} will not be
     * called when an event filter is set.
     */
    public static final WritableObjectPropertyKey<GestureDetector> GESTURE_DETECTOR =
            new WritableObjectPropertyKey<>();

    /** Whether the scrim should affect the navigation bar color. */
    public static final WritableBooleanPropertyKey AFFECTS_NAVIGATION_BAR =
            new WritableBooleanPropertyKey();

    /** Used to decide if touch events should be handled or not. */
    /* package */ static final WritableObjectPropertyKey<TouchEventDelegate> TOUCH_EVENT_DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Property keys used to control the behavior of the scrim. */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TOP_MARGIN,
                BOTTOM_MARGIN,
                AFFECTS_STATUS_BAR,
                ANCHOR_VIEW,
                SHOW_IN_FRONT_OF_ANCHOR_VIEW,
                VISIBILITY_CALLBACK,
                CLICK_DELEGATE,
                ALPHA,
                TOUCH_EVENT_DELEGATE,
                BACKGROUND_COLOR,
                GESTURE_DETECTOR,
                AFFECTS_NAVIGATION_BAR,
            };
}
