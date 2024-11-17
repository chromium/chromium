// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.graphics.Rect;
import android.os.Build;
import android.text.TextUtils;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureSession;

import androidx.annotation.RequiresApi;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The background task to talk to the ContentCapture Service. */
@RequiresApi(Build.VERSION_CODES.Q)
abstract class NotificationTask extends AsyncTask<Boolean> {
    private static final String TAG = "ContentCapture";
    private static Boolean sDump;

    protected final FrameSession mSession;
    protected final PlatformSession mPlatformSession;

    private boolean mHasPlatformExceptionForTesting;

    /**
     * A specific framework ContentCapture exception in Android Q and R shall be caught to prevent
     * the crash, the current NotificationTask can't be recovered from exception and has to
     * exit, the next task shall continue to run even it could cause the inconsistent state in
     * Android framework and aiai service who shall bear with it.
     *
     * Refer to crbug.com/1131430 for details.
     */
    private static boolean isMainContentCaptureSesionSentEventException(NullPointerException e) {
        for (StackTraceElement s : e.getStackTrace()) {
            if (s.getClassName().startsWith("android.view.contentcapture.MainContentCaptureSession")
                    && s.getMethodName().startsWith("sendEvent")) {
                return true;
            }
        }
        return false;
    }

    public NotificationTask(FrameSession session, PlatformSession platformSession) {
        mSession = session;
        mPlatformSession = platformSession;
        if (sDump == null) sDump = ContentCaptureFeatures.isDumpForTestingEnabled();
    }

    // Build up FrameIdToPlatformSessionData map of mSession, and return the current
    // session the task should run against.
    public PlatformSessionData buildCurrentSession() {
        if (mSession == null || mSession.isEmpty()) {
            return mPlatformSession.getRootPlatformSessionData();
        }
        // Build the session from root.
        PlatformSessionData platformSessionData = mPlatformSession.getRootPlatformSessionData();
        for (int i = mSession.size() - 1; i >= 0; i--) {
            platformSessionData = createOrGetSession(platformSessionData, mSession.get(i));
            if (platformSessionData == null) break;
        }
        return platformSessionData;
    }

    protected AutofillId notifyViewAppeared(
            PlatformSessionData parentPlatformSessionData, ContentCaptureDataBase data) {
        ViewStructure viewStructure =
                PlatformAPIWrapper.getInstance()
                        .newVirtualViewStructure(
                                parentPlatformSessionData.contentCaptureSession,
                                parentPlatformSessionData.autofillId,
                                data.getId());

        viewStructure.setText(data.getText());
        Rect rect = data.getBounds();
        // Always set scroll as (0, 0).
        viewStructure.setDimens(rect.left, rect.top, 0, 0, rect.width(), rect.height());
        PlatformAPIWrapper.getInstance()
                .notifyViewAppeared(parentPlatformSessionData.contentCaptureSession, viewStructure);
        return viewStructure.getAutofillId();
    }

    public PlatformSessionData createOrGetSession(
            PlatformSessionData parentPlatformSessionData, ContentCaptureFrame frame) {
        PlatformSessionData platformSessionData =
                mPlatformSession.getFrameIdToPlatformSessionData().get(frame.getId());
        if (platformSessionData == null && !TextUtils.isEmpty(frame.getUrl())) {
            ContentCaptureSession session =
                    PlatformAPIWrapper.getInstance()
                            .createContentCaptureSession(
                                    parentPlatformSessionData.contentCaptureSession,
                                    frame.getUrl(),
                                    frame.getFavicon());
            AutofillId autofillId = notifyViewAppeared(parentPlatformSessionData, frame);
            platformSessionData = new PlatformSessionData(session, autofillId);
            mPlatformSession
                    .getFrameIdToPlatformSessionData()
                    .put(frame.getId(), platformSessionData);
        }
        return platformSessionData;
    }

    public boolean hasPlatformExceptionForTesting() {
        return mHasPlatformExceptionForTesting;
    }

    protected void log(String message) {
        if (sDump.booleanValue()) Log.i(TAG, message);
    }

    @Override
    protected void onPostExecute(Boolean result) {}

    @Override
    public final Boolean doInBackground() {
        try {
            runTask();
        } catch (NullPointerException e) {
            if (isMainContentCaptureSesionSentEventException(e)) {
                mHasPlatformExceptionForTesting = true;
                Log.e(TAG, "PlatformException", e);
            } else {
                throw e;
            }
        }
        return true;
    }

    protected abstract void runTask();
}
