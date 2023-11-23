// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Centralizes UMA data collection for Android-specific MediaSession features. */
public class MediaSessionUma {
    // MediaSessionAction defined in tools/metrics/histograms/histograms.xml.
    @IntDef({
        MediaSessionActionSource.MEDIA_NOTIFICATION,
        MediaSessionActionSource.MEDIA_SESSION,
        MediaSessionActionSource.HEADSET_UNPLUG
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaSessionActionSource {
        int MEDIA_NOTIFICATION = 0;
        int MEDIA_SESSION = 1;
        int HEADSET_UNPLUG = 2;

        int NUM_ENTRIES = 3;
    }

    public static void recordPlay(@Nullable @MediaSessionActionSource Integer action) {
        if (action != null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Media.Session.Play", action, MediaSessionActionSource.NUM_ENTRIES);
        }
    }
}
