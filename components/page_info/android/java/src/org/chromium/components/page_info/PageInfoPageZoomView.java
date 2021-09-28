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

/**
 * View that allows a user to change the page zoom factor for web contents.
 */
// TODO(mschillaci): This is a placeholder page visible only behind a flag, not finalized UI.
public class PageInfoPageZoomView {
    // Available zoom factors for any page.
    private static final float[] AVAILABLE_ZOOM_FACTORS =
            new float[] {0.10f, 0.25f, 0.33f, 0.50f, 0.67f, 0.75f, 0.90f, 1.00f, 1.10f, 1.25f,
                    1.33f, 1.50f, 1.75f, 2.00f, 2.50f, 3.00f, 4.00f, 5.00f};

    // Default index for zoom factor, set to be 100%.
    private static final int DEFAULT_ZOOM_FACTOR_INDEX = 7;

    // Current zoom factor set by the user.
    private int mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;

    private final View mMainView;
    private final Context mContext;

    private final TextView mPageZoomText;
    private final ImageButton mDecreaseZoomButton;
    private final ImageButton mIncreaseZoomButton;
    private final Button mResetZoomButton;

    public PageInfoPageZoomView(Context context) {
        mContext = context;
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
            updateTextAndButtonStates();
        });

        mDecreaseZoomButton.setOnClickListener(view -> {
            assert canDecreaseZoom();
            --mZoomIndex;
            updateTextAndButtonStates();
        });

        mResetZoomButton.setOnClickListener(view -> {
            mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;
            updateTextAndButtonStates();
        });

        // Set text on first load.
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

    // Helper method to construct text of the zoom factor after user actions.
    private String generateTextFromZoomFactor() {
        return mContext.getResources().getString(
                R.string.page_zoom_factor, (int) (100 * AVAILABLE_ZOOM_FACTORS[mZoomIndex]));
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