// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.mediarouter.media.MediaRouteSelector;

/** Abstracts parsing the Cast application id and other parameters from the source URN. */
public interface MediaSource {
    /**
     * Returns a new {@link MediaRouteSelector} to use for Cast device filtering for this
     * particular media source or null if the application id is invalid.
     *
     * @return an initialized route selector or null.
     */
    public MediaRouteSelector buildRouteSelector();

    /** @return the Cast application id corresponding to the source. */
    public String getApplicationId();

    /** @return the id identifying the media source */
    public String getSourceId();
}
