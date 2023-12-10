// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to update the favicon to plateform. */
public class FaviconUpdateTask extends NotificationTask {
    public FaviconUpdateTask(FrameSession session, PlatformSession platformSession) {
        super(session, platformSession);
    }

    @Override
    protected void runTask() {
        updateFavicon();
    }

    private void updateFavicon() {
        log("FaviconUpdateTask.updateFavicon");
        PlatformSessionData parentPlatformSessionData = buildCurrentSession();
        if (parentPlatformSessionData == null) return;
        PlatformAPIWrapper.getInstance()
                .notifyFaviconUpdated(
                        parentPlatformSessionData.contentCaptureSession,
                        mSession.get(0).getFavicon());
    }
}
