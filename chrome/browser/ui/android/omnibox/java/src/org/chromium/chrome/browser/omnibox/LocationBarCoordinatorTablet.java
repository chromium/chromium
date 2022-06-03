// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.graphics.drawable.Drawable;
import android.view.View;

/**
 * A supplement to {@link LocationBarCoordinator} with methods specific to larger devices.
 */
public class LocationBarCoordinatorTablet implements LocationBarCoordinator.SubCoordinator {
    private LocationBarTablet mLocationBarTablet;

    public LocationBarCoordinatorTablet(LocationBarTablet tabletLayout) {
        mLocationBarTablet = tabletLayout;
    }

    @Override
    public void destroy() {
        mLocationBarTablet = null;
    }

    /**
     * Gets the background drawable.
     *
     * <p>TODO(1133482): Hide this View interaction if possible.
     *
     * @see View#getBackground()
     */
    public Drawable getBackground() {
        return mLocationBarTablet.getBackground();
    }
}
