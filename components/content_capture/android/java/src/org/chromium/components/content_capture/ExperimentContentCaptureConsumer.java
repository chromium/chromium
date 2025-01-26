// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.base.Log;

/**
 * This class is used to trigger ContentCapture unconditionally for the experiment. It doesn't
 * consume any content, but is necessary to keep capturing content.
 */
public class ExperimentContentCaptureConsumer implements ContentCaptureConsumer {
    private static final String TAG = "ContentCapture";
    private static boolean sDump;

    public ExperimentContentCaptureConsumer() {}

    @Override
    public void onContentCaptured(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame) {
        if (sDump) Log.d(TAG, "onContentCaptured " + contentCaptureFrame.toString());
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
    public void onContentUpdated(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame) {
        if (sDump) Log.d(TAG, "onContentUpdated");
    }

    @Override
    public void onTitleUpdated(ContentCaptureFrame contentCaptureFrame) {
        if (sDump) Log.d(TAG, "onTitleUpdated");
    }

    @Override
    public void onFaviconUpdated(ContentCaptureFrame mainFrame) {
        if (sDump) Log.d(TAG, "onFaviconUpdated");
    }

    @Override
    public boolean shouldCapture(String[] urls) {
        return true;
    }
}
