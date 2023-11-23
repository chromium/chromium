// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.view.autofill.AutofillId;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to update the captured content in platform. */
class ContentUpdateTask extends ProcessContentCaptureDataTask {
    public ContentUpdateTask(
            FrameSession session,
            ContentCaptureFrame contentCaptureFrame,
            PlatformSession platformSession) {
        super(session, contentCaptureFrame, platformSession);
    }

    @Override
    protected AutofillId notifyPlatform(
            PlatformSessionData parentPlatformSessionData, ContentCaptureDataBase data) {
        return notifyViewTextChanged(parentPlatformSessionData, (ContentCaptureData) data);
    }

    private AutofillId notifyViewTextChanged(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data) {
        AutofillId autofillId =
                PlatformAPIWrapper.getInstance()
                        .newAutofillId(
                                parentPlatformSessionData.contentCaptureSession,
                                mPlatformSession.getRootPlatformSessionData().autofillId,
                                data.getId());
        PlatformAPIWrapper.getInstance()
                .notifyViewTextChanged(
                        parentPlatformSessionData.contentCaptureSession,
                        autofillId,
                        data.getValue());
        return autofillId;
    }
}
