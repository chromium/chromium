// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to remove the platform session */
class SessionRemovedTask extends NotificationTask {
    public SessionRemovedTask(FrameSession session, PlatformSession platformSession) {
        super(session, platformSession);
    }

    @Override
    protected void runTask() {
        removeSession();
    }

    private void removeSession() {
        log("SessionRemovedTask.removeSession");
        PlatformSessionData removedPlatformSessionData =
                mPlatformSession.getFrameIdToPlatformSessionData().remove(mSession.get(0).getId());
        if (removedPlatformSessionData == null) return;
        PlatformAPIWrapper.getInstance()
                .destroyContentCaptureSession(removedPlatformSessionData.contentCaptureSession);
        PlatformSessionData parentPlatformSessionData =
                mPlatformSession.getRootPlatformSessionData();
        // We need to notify the view disappeared through the removed session's parent,
        // if there are more than one session in mSession, the removed session is child
        // frame, otherwise, is main frame.
        if (mSession.size() > 2) {
            parentPlatformSessionData =
                    mPlatformSession.getFrameIdToPlatformSessionData().get(mSession.get(1).getId());
        }
        if (parentPlatformSessionData == null) return;
        PlatformAPIWrapper.getInstance()
                .notifyViewDisappeared(
                        parentPlatformSessionData.contentCaptureSession,
                        removedPlatformSessionData.autofillId);
    }
}
