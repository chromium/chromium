// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.displaystyle;

import android.content.Context;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Exposes general configuration info about the display style for a given reference View. */
public class UiConfig {
    public static final int NARROW_DISPLAY_STYLE_MAX_WIDTH_DP = 320;
    public static final int WIDE_DISPLAY_STYLE_MIN_WIDTH_DP = 600;
    public static final int FLAT_DISPLAY_STYLE_MAX_HEIGHT_DP = 320;

    private static final String TAG = "DisplayStyle";
    private static final boolean DEBUG = false;

    private DisplayStyle mCurrentDisplayStyle;

    private final List<DisplayStyleObserver> mObservers = new ArrayList<>();
    private final Context mContext;

    /** @param referenceView the View we observe to deduce the configuration from. */
    public UiConfig(View referenceView) {
        mContext = referenceView.getContext();
        mCurrentDisplayStyle = computeDisplayStyleForCurrentConfig();

        referenceView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        updateDisplayStyle();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {}
                });
    }

    /**
     * Registers a {@link DisplayStyleObserver}. It will be notified right away with the current
     * display style.
     */
    public void addObserver(DisplayStyleObserver observer) {
        assert !mObservers.contains(observer);
        mObservers.add(observer);
        observer.onDisplayStyleChanged(mCurrentDisplayStyle);
    }

    /**
     * Unregisters a previously registered {@link DisplayStyleObserver}.
     * @param observer The {@link DisplayStyleObserver} to be unregistered.
     */
    public void removeObserver(DisplayStyleObserver observer) {
        boolean success = mObservers.remove(observer);
        assert success;
    }

    /** @return The context for the view associated with this UiConfig. */
    public Context getContext() {
        return mContext;
    }

    /** Refresh the display style, notify observers of changes. */
    public void updateDisplayStyle() {
        updateDisplayStyle(computeDisplayStyleForCurrentConfig());
    }

    /** Returns the currently used display style. */
    public DisplayStyle getCurrentDisplayStyle() {
        return mCurrentDisplayStyle;
    }

    /** Sets the display style, notifying observers of changes. Should only be used in testing. */
    public void setDisplayStyleForTesting(DisplayStyle displayStyle) {
        updateDisplayStyle(displayStyle);
    }

    private void updateDisplayStyle(DisplayStyle displayStyle) {
        mCurrentDisplayStyle = displayStyle;
        for (DisplayStyleObserver observer : mObservers) {
            observer.onDisplayStyleChanged(displayStyle);
        }
    }

    private DisplayStyle computeDisplayStyleForCurrentConfig() {
        int widthDp = mContext.getResources().getConfiguration().screenWidthDp;
        int heightDp = mContext.getResources().getConfiguration().screenHeightDp;

        @HorizontalDisplayStyle int newHorizontalDisplayStyle;
        if (widthDp <= NARROW_DISPLAY_STYLE_MAX_WIDTH_DP) {
            newHorizontalDisplayStyle = HorizontalDisplayStyle.NARROW;
        } else if (widthDp >= WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) {
            newHorizontalDisplayStyle = HorizontalDisplayStyle.WIDE;
        } else {
            newHorizontalDisplayStyle = HorizontalDisplayStyle.REGULAR;
        }

        @VerticalDisplayStyle
        int newVerticalDisplayStyle =
                heightDp <= FLAT_DISPLAY_STYLE_MAX_HEIGHT_DP
                        ? VerticalDisplayStyle.FLAT
                        : VerticalDisplayStyle.REGULAR;

        final DisplayStyle displayStyle =
                new DisplayStyle(newHorizontalDisplayStyle, newVerticalDisplayStyle);
        if (DEBUG) debug(displayStyle, widthDp, heightDp);

        return displayStyle;
    }

    private void debug(DisplayStyle displayStyle, int widthDp, int heightDp) {
        String horizontalStyleName;
        String verticalStyleName;

        switch (displayStyle.horizontal) {
            case HorizontalDisplayStyle.NARROW:
                horizontalStyleName = "NARROW";
                break;
            case HorizontalDisplayStyle.REGULAR:
                horizontalStyleName = "REGULAR";
                break;
            case HorizontalDisplayStyle.WIDE:
                horizontalStyleName = "WIDE";
                break;
            default:
                throw new IllegalStateException();
        }

        switch (displayStyle.vertical) {
            case VerticalDisplayStyle.FLAT:
                verticalStyleName = "FLAT";
                break;
            case VerticalDisplayStyle.REGULAR:
                verticalStyleName = "REGULAR";
                break;
            default:
                throw new IllegalStateException();
        }

        String debugString =
                String.format(
                        Locale.US,
                        "%s | %s (w=%ddp, h=%ddp)",
                        horizontalStyleName,
                        verticalStyleName,
                        widthDp,
                        heightDp);
        Log.d(TAG, debugString);
        Toast.makeText(mContext, debugString, Toast.LENGTH_SHORT).show();
    }

    /**
     * The different supported UI setups. {@link DisplayStyleObserver} can register to be notified
     * of changes.
     * @see HorizontalDisplayStyle
     * @see VerticalDisplayStyle
     */
    public static final class DisplayStyle {
        @HorizontalDisplayStyle public final int horizontal;
        @VerticalDisplayStyle public final int vertical;

        public DisplayStyle(
                @HorizontalDisplayStyle int horizontal, @VerticalDisplayStyle int vertical) {
            this.horizontal = horizontal;
            this.vertical = vertical;
        }

        /**
         * @return whether the display is small enough to be considered below the regular size in
         * any of the 2 dimensions.
         */
        public boolean isSmall() {
            return horizontal == HorizontalDisplayStyle.NARROW
                    || vertical == VerticalDisplayStyle.FLAT;
        }

        /** @return whether the display is horizontally wide. */
        public boolean isWide() {
            return horizontal == HorizontalDisplayStyle.WIDE;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;

            DisplayStyle that = (DisplayStyle) o;
            return horizontal == that.horizontal && vertical == that.vertical;
        }

        @Override
        public int hashCode() {
            return 31 * horizontal + vertical;
        }
    }
}
