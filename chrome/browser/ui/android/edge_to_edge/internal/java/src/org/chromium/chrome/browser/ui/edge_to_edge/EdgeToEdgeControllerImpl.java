// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.Color;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;

/**
 * Controls use of the Android Edge To Edge feature that allows an App to draw benieth the Status
 * and Navigation Bars. For Chrome, we intentend to sometimes draw under the Nav Bar but not the
 * Status Bar.
 */
public class EdgeToEdgeControllerImpl implements EdgeToEdgeController {
    /** The outermost view in our view hierarchy that is identified with a resource ID. */
    private static final int ROOT_UI_VIEW_ID = android.R.id.content;

    private final AppCompatActivity mActivity;

    public EdgeToEdgeControllerImpl(AppCompatActivity activity) {
        mActivity = activity;
    }

    @Override
    @RequiresApi(VERSION_CODES.R)
    public void drawUnderSystemBars() {
        if (!EdgeToEdgeControllerFactory.isEnabled()) return;

        // Pass the root View to help ensure that everything in the window gets adjusted back to
        // regular drawing (except for the edge that we're drawing under). If we use child view,
        // some views may not be adjusted back to normal and we'd draw inder all the edges.
        drawUnderSystemBars(ROOT_UI_VIEW_ID);

        // TODO(donnd): Conditionally enable on another arm when we've navigated to a new
        //  page so we can check the web platform notch setting for that page.
    }

    @VisibleForTesting
    @RequiresApi(VERSION_CODES.R)
    @SuppressWarnings("WrongConstant") // For WindowInsets.Type on U+
    void drawUnderSystemBars(int viewId) {
        // Setup the basic enabling of the Edge to Edge Android Feature.
        // Set up this window to open up all edges to be drawn underneath.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), false);
        // We only make the Nav Bar transparent because it's the only thing we want to draw
        // underneath.
        mActivity.getWindow().setNavigationBarColor(Color.TRANSPARENT);

        // Now fix all the edges other than the Nav/bottom by insetting with padding.
        // This keeps the setDecorFitsSystemWindows from actually drawing under any other edges.
        View rootView = mActivity.findViewById(viewId);
        assert rootView != null : "Root view for Edge To Edge not found!";
        ViewCompat.setOnApplyWindowInsetsListener(rootView, (view, windowInsets) -> {
            Insets systemInsets = windowInsets.getInsets(WindowInsets.Type.systemBars());
            // Restore the drawing to normal on all edges, except for the bottom (Nav Bar).
            view.setPadding(systemInsets.left, systemInsets.top, systemInsets.right, 0);
            return windowInsets;
        });
    }
}
