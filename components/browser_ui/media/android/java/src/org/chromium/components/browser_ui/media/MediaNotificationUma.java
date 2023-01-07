// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.content.Intent;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to record which kind of media notifications does the user click to go back to
 * Chrome.
 */
public class MediaNotificationUma {
    @IntDef({Source.INVALID, Source.MEDIA, Source.PRESENTATION, Source.MEDIA_FLING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Source {
        int INVALID = -1;
        int MEDIA = 0;
        int PRESENTATION = 1;
        int MEDIA_FLING = 2;
        int NUM_ENTRIES = 3;
    }

    public static final String INTENT_EXTRA_NAME =
            "org.chromium.chrome.browser.metrics.MediaNotificationUma.EXTRA_CLICK_SOURCE";

    /**
     * Record the UMA as specified by {@link intent}. The {@link intent} should contain intent extra
     * of name {@link INTENT_EXTRA_NAME} indicating the type.
     * @param intent The intent starting the activity.
     */
    public static void recordClickSource(Intent intent) {
        if (intent == null) return;
        @Source
        int source = intent.getIntExtra(INTENT_EXTRA_NAME, Source.INVALID);
        if (source == Source.INVALID || source >= Source.NUM_ENTRIES) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Media.Notification.Click", source, Source.NUM_ENTRIES);
    }
}
