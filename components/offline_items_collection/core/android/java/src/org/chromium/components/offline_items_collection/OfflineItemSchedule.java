// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

/**
 * Java counter part of OfflineItemSchedule in native, see
 * components/offline_items_collection/core/offline_item.h.
 */
public class OfflineItemSchedule {
    /**
     * Constructs a OfflineItemSchedule.
     * @param onlyOnWifi See {@link #onlyOnWifi}.
     * @param startTimeMs See {@link #startTimeMs}.
     */
    public OfflineItemSchedule(boolean onlyOnWifi, long startTimeMs) {
        this.onlyOnWifi = onlyOnWifi;
        this.startTimeMs = startTimeMs;
    }

    @Override
    public OfflineItemSchedule clone() {
        return new OfflineItemSchedule(onlyOnWifi, startTimeMs);
    }

    /**
     * Whether the offline item will be downloaded only on WIFI.
     */
    public final boolean onlyOnWifi;

    /**
     * The trigger time to download the offline item. Will not triggered at particular time if the
     * value is 0.
     */
    public final long startTimeMs;
}
