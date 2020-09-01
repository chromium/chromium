// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;

/**
 * This class is used to trigger ContentCapture unconditionally for the experiment. It doesn't
 * consume any content, but is necessary to keep capturing content.
 */
public class ExperimentContentCaptureConsumer extends ContentCaptureConsumer {
    private static final String TAG = "ContentCapture";
    private static boolean sDump;

    public static ContentCaptureConsumer create(WebContents webContents) {
        if (ContentCaptureFeatures.shouldTriggerContentCaptureForExperiment()) {
            return new ExperimentContentCaptureConsumer(webContents);
        }
        return null;
    }

    private ExperimentContentCaptureConsumer(WebContents webContents) {
        super(webContents);
    }

    @Override
    public void onContentCaptured(FrameSession parentFrame, ContentCaptureData contentCaptureData) {
        if (sDump) Log.d(TAG, "onContentCaptured " + contentCaptureData.toString());
    }

    @Override
    public void onSessionRemoved(FrameSession session) {
        if (sDump) Log.d(TAG, "onSessionRemoved");
    }

    @Override
    public void onContentRemoved(FrameSession session, long[] removedIds) {
        if (sDump) Log.d(TAG, "onContentRemoved");
    }

    @Override
    public void onContentUpdated(FrameSession parentFrame, ContentCaptureData contentCaptureData) {
        if (sDump) Log.d(TAG, "onContentUpdated");
    }
}
