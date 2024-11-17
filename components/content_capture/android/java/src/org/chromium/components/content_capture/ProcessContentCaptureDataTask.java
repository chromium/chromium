// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.view.autofill.AutofillId;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

import java.util.List;

/** The base class to process the ContentCaptureData. */
abstract class ProcessContentCaptureDataTask extends NotificationTask {
    private final ContentCaptureFrame mContentCaptureData;

    public ProcessContentCaptureDataTask(
            FrameSession session,
            ContentCaptureFrame contentCaptureData,
            PlatformSession platformSession) {
        super(session, platformSession);
        mContentCaptureData = contentCaptureData;
    }

    @Override
    protected void runTask() {
        processContent();
    }

    private void processContent() {
        log("ProcessContentTaskBase.processContent");
        PlatformSessionData platformSessionData = buildCurrentSession();
        if (platformSessionData == null) return;
        processCaptureFrame(platformSessionData, mContentCaptureData);
    }

    private boolean processCaptureFrame(
            PlatformSessionData parentPlatformSessionData, ContentCaptureFrame data) {
        if (data == null || data.getUrl() == null) return false;
        PlatformSessionData platformSessionData =
                createOrGetSession(parentPlatformSessionData, data);
        if (platformSessionData == null) return false;
        List<ContentCaptureDataBase> children = data.getChildren();
        for (ContentCaptureDataBase child : children) {
            if (!processCaptureData(platformSessionData, (ContentCaptureData) child)) return false;
        }
        return true;
    }

    private boolean processCaptureData(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data) {
        if (data == null) return false;
        if (data.hasChildren()) {
            // This is scrollable area.
            AutofillId autofillId = notifyPlatform(parentPlatformSessionData, data);
            // To add children below scrollable area in frame, the ContentCaptureSession
            // of the scrollable area is the frame the scrollable area belong to, AutofillId
            // is scrollable area's AutofillId.
            if (autofillId == null) return false;
            PlatformSessionData platformSessionData =
                    new PlatformSessionData(
                            parentPlatformSessionData.contentCaptureSession, autofillId);

            List<ContentCaptureDataBase> children = data.getChildren();
            for (ContentCaptureDataBase child : children) {
                if (!processCaptureData(platformSessionData, (ContentCaptureData) child)) {
                    return false;
                }
            }
            return true;
        } else {
            // This is text.
            return null != notifyPlatform(parentPlatformSessionData, data);
        }
    }

    protected abstract AutofillId notifyPlatform(
            PlatformSessionData parentPlatformSessionData, ContentCaptureDataBase data);
}
