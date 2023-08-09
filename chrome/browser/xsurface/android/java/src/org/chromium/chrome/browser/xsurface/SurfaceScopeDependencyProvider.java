// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

/**
 * Implemented in Chromium.
 *
 * Provides dependencies for xsurface at the surface level.
 *
 * Should only be called on the UI thread.
 */
public interface SurfaceScopeDependencyProvider {
    /** Returns the activity. */
    default @Nullable Activity getActivity() {
        return null;
    }

    /** Returns the activity context hosting the surface. */
    default @Nullable Context getActivityContext() {
        return null;
    }

    /** Returns whether the activity is in darkmode or not */
    default boolean isDarkModeEnabled() {
        return false;
    }
}
