// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import org.chromium.chrome.browser.xsurface.SurfaceScope;

// TODO(b/269234249): Don't use this in Chromium yet, it's not implemented.
/**
 * Implemented internally.
 *
 * Provides multiple types of renderers to surfaces that want to render an
 * external surface. Each renderer will reuse the same dependencies (hence
 * "Scope") but each call to provideFoo will return a new renderer, so that a
 * single surface can support multiple rendered views.
 */
public interface FeedSurfaceScope extends SurfaceScope {
    /**
     * Return the FeedLaunchReliabilityLogger associated with the surface, creating it if it
     * doesn't exist.
     * @return The surface's FeedLaunchReliabilityLogger instance.
     */
    default FeedLaunchReliabilityLogger getLaunchReliabilityLogger() {
        return new FeedLaunchReliabilityLogger() {};
    }

    default FeedUserInteractionReliabilityLogger getUserInteractionReliabilityLogger() {
        return new FeedUserInteractionReliabilityLogger() {};
    }

    default FeedCardOpeningReliabilityLogger getCardOpeningReliabilityLogger() {
        return new FeedCardOpeningReliabilityLogger() {};
    }
}
