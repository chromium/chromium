// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import androidx.annotation.Nullable;

/**
 * This interface is a Java counterpart to the C++ base::Callback meant to be used in response
 * to {@link OfflineItemVisuals} requests.
 */
public interface VisualsCallback {
    /**
     * @param id      The {@link ContentId} that {@code visuals} is associated with.
     * @param visuals The {@link OfflineItemVisuals}, if any, associated with {@code id}.
     */
    void onVisualsAvailable(ContentId id, @Nullable OfflineItemVisuals visuals);
}