// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.view.autofill.AutofillId;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

import java.util.List;

/**
 * The base class to process the ContentCaptureData.
 */
abstract class ProcessContentCaptureDataTask extends NotificationTask {
    private final ContentCaptureData mContentCaptureData;
    /**
     * @param session
     * @param contentCaptureData
     * @param platformSession
     */
    public ProcessContentCaptureDataTask(FrameSession session,
            ContentCaptureData contentCaptureData, PlatformSession platformSession) {
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
        processCaptureData(platformSessionData, mContentCaptureData);
    }

    private boolean processCaptureData(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data) {
        if (data == null) return false;
        if (data.hasChildren()) {
            PlatformSessionData platformSessionData;
            if (data.getValue() != null) {
                // This is frame.
                platformSessionData = createOrGetSession(parentPlatformSessionData, data);
                if (platformSessionData == null) return false;
            } else {
                // This is scrollable area.
                AutofillId autofillId = notifyPlatform(parentPlatformSessionData, data);
                // To add children below scrollable area in frame, the ContentCaptureSession
                // of the scrollable area is the frame the scrollable area belong to, AutofillId
                // is scrollable area's AutofillId.
                if (autofillId == null) return false;
                platformSessionData = new PlatformSessionData(
                        parentPlatformSessionData.contentCaptureSession, autofillId);
            }
            List<ContentCaptureData> children = data.getChildren();
            for (ContentCaptureData child : children) {
                if (!processCaptureData(platformSessionData, child)) return false;
            }
            return true;
        } else {
            // This is text.
            return null != notifyPlatform(parentPlatformSessionData, data);
        }
    }

    protected abstract AutofillId notifyPlatform(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data);
}
