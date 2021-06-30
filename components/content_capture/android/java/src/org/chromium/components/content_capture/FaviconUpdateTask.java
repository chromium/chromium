// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/**
 * The task to update the favicon to plateform.
 */
public class FaviconUpdateTask extends NotificationTask {
    private ContentCaptureFrame mMainFrame;
    public FaviconUpdateTask(ContentCaptureFrame mainFrame, PlatformSession platformSession) {
        super(null, platformSession);
        mMainFrame = mainFrame;
    }

    @Override
    protected void runTask() {
        updateFavicon();
    }

    private void updateFavicon() {
        log("FaviconUpdateTask.updateTitle");
        PlatformSessionData parentPlatformSessionData = buildCurrentSession();
        PlatformAPIWrapper.getInstance().notifyFaviconUpdated(
                parentPlatformSessionData.contentCaptureSession, mMainFrame.getFavicon());
    }
}
