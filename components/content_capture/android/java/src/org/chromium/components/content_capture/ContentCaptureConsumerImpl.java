// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.ViewStructure;

import org.chromium.base.annotations.VerifiesOnQ;
import org.chromium.base.task.AsyncTask;
import org.chromium.content_public.browser.WebContents;

/**
 * This class receive captured content and send it to framework in non-UI
 * thread.
 */
@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
public class ContentCaptureConsumerImpl extends ContentCaptureConsumer {
    private PlatformSession mPlatformSession;
    private final View mView;

    /**
     * This method is used when ViewStructure is available.
     *
     * @Return ContentCaptureConsumer or null if ContentCapture service isn't
     *         available, disabled or isn't AiAi service.
     */
    public static ContentCaptureConsumer create(
            Context context, View view, ViewStructure structure, WebContents webContents) {
        if (ContentCaptureController.getInstance() == null) {
            ContentCaptureControllerImpl.init(context.getApplicationContext());
        }

        if (!ContentCaptureController.getInstance().shouldStartCapture()) return null;
        return new ContentCaptureConsumerImpl(view, structure, webContents);
    }

    /**
     * This method is used when ViewStructure isn't available and needs to be
     * created.
     */
    public static ContentCaptureConsumer create(
            Context context, View view, WebContents webContents) {
        return create(context, view, null, webContents);
    }

    private ContentCaptureConsumerImpl(
            View view, ViewStructure viewStructure, WebContents webContents) {
        super(webContents);
        mView = view;
        if (viewStructure != null) {
            mPlatformSession = new PlatformSession(
                    view.getContentCaptureSession(), viewStructure.getAutofillId());
        }
    }

    @Override
    public void onContentCaptured(FrameSession parentFrame, ContentCaptureData contentCaptureData) {
        if (mPlatformSession == null) {
            mPlatformSession = PlatformSession.fromView(mView);
            if (mPlatformSession == null) return;
        }
        new ContentCapturedTask(parentFrame, contentCaptureData, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onContentUpdated(FrameSession parentFrame, ContentCaptureData contentCaptureData) {
        if (mPlatformSession == null) return;
        new ContentUpdateTask(parentFrame, contentCaptureData, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onSessionRemoved(FrameSession frame) {
        if (frame.isEmpty() || mPlatformSession == null) return;
        new SessionRemovedTask(frame, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onContentRemoved(FrameSession frame, long[] removedIds) {
        if (frame.isEmpty() || mPlatformSession == null) return;
        new ContentRemovedTask(frame, removedIds, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }
}
