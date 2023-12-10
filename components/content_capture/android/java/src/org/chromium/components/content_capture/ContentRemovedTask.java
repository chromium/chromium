// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to remove the captured content from the platform. */
class ContentRemovedTask extends NotificationTask {
    private final long[] mRemovedIds;

    public ContentRemovedTask(
            FrameSession session, long[] removedIds, PlatformSession platformSession) {
        super(session, platformSession);
        mRemovedIds = removedIds;
    }

    @Override
    protected void runTask() {
        removeContent();
    }

    private void removeContent() {
        log("ContentRemovedTask.removeContent");
        PlatformSessionData platformSessionData = buildCurrentSession();
        if (platformSessionData == null) return;
        PlatformAPIWrapper.getInstance()
                .notifyViewsDisappeared(
                        platformSessionData.contentCaptureSession,
                        mPlatformSession.getRootPlatformSessionData().autofillId,
                        mRemovedIds);
    }
}
