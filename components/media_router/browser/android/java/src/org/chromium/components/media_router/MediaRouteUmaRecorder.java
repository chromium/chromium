// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import org.chromium.base.metrics.RecordHistogram;

/** Record statistics on interesting cast events and actions. */
public class MediaRouteUmaRecorder {
    /**
     * Record the number of devices shown in the media route chooser dialog. The function is called
     * three seconds after the dialog is created. The delay time is consistent with how the device
     * count is recorded on Chrome desktop.
     *
     * @param count the number of devices shown.
     */
    public static void recordDeviceCountWithDelay(int count) {
        RecordHistogram.recordCount100Histogram("MediaRouter.Ui.Device.Count", count);
    }
}
