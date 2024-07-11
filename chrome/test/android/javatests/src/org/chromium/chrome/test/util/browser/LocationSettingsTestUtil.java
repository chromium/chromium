// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import org.chromium.base.ThreadUtils;
import org.chromium.components.location.LocationUtils;

/** Methods for testing location-related features. */
public class LocationSettingsTestUtil {

    /**
     * Mocks the system location setting as either enabled or disabled. Can be called on any thread.
     */
    public static void setSystemLocationSettingEnabled(final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationUtils.setFactory(
                            new LocationUtils.Factory() {
                                @Override
                                public LocationUtils create() {
                                    return new LocationUtils() {
                                        @Override
                                        public boolean isSystemLocationSettingEnabled() {
                                            return enabled;
                                        }
                                    };
                                }
                            });
                });
    }
}
