// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This interface allows {@link StylusWritingController} to abstract over
 * {@link AndroidStylusWritingHandler}, {@link DirectWritingTrigger} and
 * {@link DisabledStylusWritingHandler}.
 *
 * We can't just add the methods here to
 * {@link org.chromium.content_public.browser.StylusWritingHandler}, because content_public should
 * only contain functionality calling between the contents and the embedder.
 */
public interface StylusApiOption {
    // This should be kept in sync with the definition |StylusHandwritingApi|
    // in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({Api.ANDROID, Api.DIRECT_WRITING, Api.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface Api {
        int ANDROID = 0;
        int DIRECT_WRITING = 1;
        int COUNT = 2;
    }

    static void recordStylusHandwritingTriggered(@Api int api) {
        RecordHistogram.recordEnumeratedHistogram(
                "InputMethod.StylusHandwriting.Triggered", api, Api.COUNT);
    }

    void onWebContentsChanged(Context context, WebContents webContents);

    default void onWindowFocusChanged(Context context, boolean hasFocus) {}

    /**
     * @return the type of pointer icon that should be shown when hovering over editable elements
     * with a stylus.
     */
    int getStylusPointerIcon();
}
