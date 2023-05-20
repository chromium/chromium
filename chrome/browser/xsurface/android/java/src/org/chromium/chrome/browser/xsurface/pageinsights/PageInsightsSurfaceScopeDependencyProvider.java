// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.pageinsights;

import android.content.Context;

import androidx.annotation.Nullable;

/**
 * Provides dependencies for PageInsightsSurfaceScope. Should only be called on the UI thread.
 *
 * Implemented in Chromium.
 */
public interface PageInsightsSurfaceScopeDependencyProvider {
    /** Returns the activity context hosting the surface. */
    @Nullable
    default Context getActivityContext() {
        return null;
    }
}
