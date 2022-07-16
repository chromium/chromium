// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;

import java.util.Arrays;

/**
 * View that allows a user to change the page zoom factor for web contents.
 */
// TODO(mschillaci): This is a placeholder page visible only behind a flag, not finalized UI.
public class PageInfoPageZoomView {
    /**
     * Available zoom levels as they would be presented to a user. These match the currently
     * used levels on Chrome Desktop. See: components/zoom/page_zoom_constants.cc
     */
    private static final double[] AVAILABLE_ZOOM_LEVELS = new double[] {0.25, 0.33, 0.50, 0.67,
            0.75, 0.80, 0.90, 1.00, 1.10, 1.25, 1.33, 1.50, 1.75, 2.00, 2.50, 3.00, 4.00, 5.00};

    /**
     * Available zoom factors that correspond to the zoom levels above. These numbers are used
     * internally to give the above zoom levels and are not presented to the user. These become
     * the exponent that |kTextSizeMultiplierRatio| = 1.2 is raised to for the above numbers,
     * e.g. 1.2^-7.6 = 0.25, or 1.2^3.8 = 2.0. See: third_party/blink/common/page/page_zoom.cc
     */
    private static final double[] AVAILABLE_ZOOM_FACTORS = new double[] {-7.60, -6.08, -3.80, -2.20,
            -1.58, -1.22, -0.58, 0.00, 0.52, 1.22, 1.56, 2.22, 3.07, 3.80, 5.03, 6.03, 7.60, 8.83};

    // Default index for zoom factor, set to be 100%.
    private static final int DEFAULT_ZOOM_FACTOR_INDEX = 7;

    // Current zoom factor set by the user.
    private int mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;

    private final View mMainView;
    private final Context mContext;
    private final PageZoomViewDelegate mDelegate;

    private final TextView mPageZoomText;
    private final ImageButton mDecreaseZoomButton;
    private final ImageButton mIncreaseZoomButton;
    private final Button mResetZoomButton;

    /**
     * Interface to delegate control of this view to another class.
     */
    interface PageZoomViewDelegate {
        void setZoomLevel(double newZoomLevel);
        double getZoomLevel();
    }

    public PageInfoPageZoomView(@NonNull Context context, @NonNull PageZoomViewDelegate delegate) {
        mContext = context;
        mDelegate = delegate;
        mMainView = LayoutInflater.from(mContext).inflate(R.layout.page_zoom_view, null);

        // Bind views and set click listeners.
        mPageZoomText = (TextView) mMainView.findViewById(R.id.page_zoom_current_zoom_level_text);
        mDecreaseZoomButton =
                (ImageButton) mMainView.findViewById(R.id.page_zoom_decrease_zoom_button);
        mIncreaseZoomButton =
                (ImageButton) mMainView.findViewById(R.id.page_zoom_increase_zoom_button);
        mResetZoomButton = (Button) mMainView.findViewById(R.id.page_zoom_reset_zoom_button);

        mIncreaseZoomButton.setOnClickListener(view -> {
            assert canIncreaseZoom();
            ++mZoomIndex;
            updateZoomFactor();
            updateTextAndButtonStates();
        });

        mDecreaseZoomButton.setOnClickListener(view -> {
            assert canDecreaseZoom();
            --mZoomIndex;
            updateZoomFactor();
            updateTextAndButtonStates();
        });

        mResetZoomButton.setOnClickListener(view -> {
            mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;
            updateZoomFactor();
            updateTextAndButtonStates();
        });

        // Set text on first load.
        mZoomIndex = Arrays.binarySearch(AVAILABLE_ZOOM_FACTORS, mDelegate.getZoomLevel());
        updateTextAndButtonStates();
    }

    /**
     * Return the view for Page Zoom
     * @return  View - Main view that contains the page zoom layout.
     */
    public View getMainView() {
        return mMainView;
    }

    // Helper method to update the text of the zoom factor and button states after user actions.
    private void updateTextAndButtonStates() {
        mPageZoomText.setText(generateTextFromZoomFactor());
        mIncreaseZoomButton.setEnabled(canIncreaseZoom());
        mDecreaseZoomButton.setEnabled(canDecreaseZoom());
    }

    // Helper method to update the zoom factor after user actions.
    private void updateZoomFactor() {
        // Apply the new zoom factor to the web contents through JNI.
        mDelegate.setZoomLevel(AVAILABLE_ZOOM_FACTORS[mZoomIndex]);
    }

    // Helper method to construct text of the zoom factor after user actions.
    private String generateTextFromZoomFactor() {
        return mContext.getResources().getString(
                R.string.page_zoom_factor, (int) (100 * AVAILABLE_ZOOM_LEVELS[mZoomIndex]));
    }

    // Helper method to determine if user can increase zoom.
    private boolean canIncreaseZoom() {
        return mZoomIndex < AVAILABLE_ZOOM_FACTORS.length - 1;
    }

    // Helper method to determine if user can decrease zoom.
    private boolean canDecreaseZoom() {
        return mZoomIndex > 0;
    }
}