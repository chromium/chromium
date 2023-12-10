// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.view.autofill.AutofillId;

import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to update the title change to plateform */
public class TitleUpdateTask extends NotificationTask {
    private ContentCaptureFrame mMainFrame;

    public TitleUpdateTask(ContentCaptureFrame mainFrame, PlatformSession platformSession) {
        super(null, platformSession);
        mMainFrame = mainFrame;
    }

    @Override
    protected void runTask() {
        updateTitle();
    }

    private void updateTitle() {
        log("TitleUpdateTask.updateTitle");
        // To notify the text change, the parent ContentCaptureSession and this view's autofill id
        // are needed.
        PlatformSessionData parentPlatformSessionData = buildCurrentSession();
        AutofillId autofillId =
                PlatformAPIWrapper.getInstance()
                        .newAutofillId(
                                parentPlatformSessionData.contentCaptureSession,
                                mPlatformSession.getRootPlatformSessionData().autofillId,
                                mMainFrame.getId());
        PlatformAPIWrapper.getInstance()
                .notifyViewTextChanged(
                        parentPlatformSessionData.contentCaptureSession,
                        autofillId,
                        mMainFrame.getText());
    }
}
