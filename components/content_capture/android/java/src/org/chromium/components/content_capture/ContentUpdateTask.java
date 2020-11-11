// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.view.autofill.AutofillId;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/**
 * The task to update the captured content in platform.
 */
class ContentUpdateTask extends ProcessContentCaptureDataTask {
    public ContentUpdateTask(FrameSession session, ContentCaptureData contentCaptureData,
            PlatformSession platformSession) {
        super(session, contentCaptureData, platformSession);
    }

    @Override
    protected AutofillId notifyPlatform(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data) {
        return notifyViewTextChanged(parentPlatformSessionData, data);
    }

    private AutofillId notifyViewTextChanged(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data) {
        AutofillId autofillId = PlatformAPIWrapper.getInstance().newAutofillId(
                parentPlatformSessionData.contentCaptureSession,
                mPlatformSession.getRootPlatformSessionData().autofillId, data.getId());
        PlatformAPIWrapper.getInstance().notifyViewTextChanged(
                parentPlatformSessionData.contentCaptureSession, autofillId, data.getValue());
        return autofillId;
    }
}
