// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to notify platform of flushing the captured content */
@RequiresApi(Build.VERSION_CODES.Q)
@NullMarked
class ContentCaptureFlushTask extends NotificationTask {
    private final ContentCaptureFrame mContentCaptureData;

    public ContentCaptureFlushTask(
            FrameSession session,
            ContentCaptureFrame contentCaptureData,
            PlatformSession platformSession) {
        super(session, platformSession);
        mContentCaptureData = contentCaptureData;
    }

    @Override
    protected void runTask() {
        notifyPlatform();
    }

    private void notifyPlatform() {
        PlatformSessionData parentPlatformSessionData = buildCurrentSession();
        if (parentPlatformSessionData == null) return;
        if (mContentCaptureData == null || mContentCaptureData.getUrl() == null) return;
        PlatformSessionData platformSessionData =
                createOrGetSession(parentPlatformSessionData, mContentCaptureData);
        if (platformSessionData == null) return;
        PlatformAPIWrapper.getInstance().flush(platformSessionData.contentCaptureSession);
    }
}
