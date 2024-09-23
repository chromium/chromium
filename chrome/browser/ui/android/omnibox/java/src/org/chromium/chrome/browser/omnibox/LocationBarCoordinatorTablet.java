// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.ColorInt;

/** A supplement to {@link LocationBarCoordinator} with methods specific to larger devices. */
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
     * Sets the color of the background.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     */
    public void tintBackground(@ColorInt int color) {
        mLocationBarTablet.getBackground().mutate().setTint(color);
    }
}
