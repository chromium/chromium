// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import androidx.annotation.Nullable;

/**
 * This interface is a Java counterpart to the C++ offline_items_collection::ShareCallback meant to
 * be used in response to {@link OfflineItemShareInfo} requests.
 */
public interface ShareCallback {
    /**
     * @param id        The {@link ContentId} that {@code shareInfo} is associated with.
     * @param shareInfo The {@link OfflineItemShareInfo}, if any, associated with {@code id}.
     */
    void onShareInfoAvailable(ContentId id, @Nullable OfflineItemShareInfo shareInfo);
}
