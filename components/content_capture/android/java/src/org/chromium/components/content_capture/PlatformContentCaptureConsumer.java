// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.ViewStructure;

import androidx.annotation.RequiresApi;

import org.chromium.base.task.AsyncTask;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;

/**
 * This class receive captured content and send it to framework in non-UI
 * thread.
 */
@RequiresApi(Build.VERSION_CODES.Q)
public class PlatformContentCaptureConsumer implements ContentCaptureConsumer {
    private PlatformSession mPlatformSession;
    // This is the WebView itself when used in WebView; it must not be strongly referenced as this
    // object is ultimately owned by the native OnscreenContentProvider and will make the WebView
    // uncollectable.
    private final WeakReference<View> mView;

    /**
     * This method is used when ViewStructure is available.
     *
     * @return ContentCaptureConsumer or null if ContentCapture service isn't available, disabled or
     *     isn't AiAi service.
     */
    public static ContentCaptureConsumer create(
            Context context, View view, ViewStructure structure, WebContents unused_webContents) {
        if (PlatformContentCaptureController.getInstance() == null) {
            PlatformContentCaptureController.init(context.getApplicationContext());
        }

        if (!PlatformContentCaptureController.getInstance().shouldStartCapture()) return null;
        return new PlatformContentCaptureConsumer(view, structure);
    }

    private PlatformContentCaptureConsumer(View view, ViewStructure viewStructure) {
        mView = new WeakReference(view);
        if (viewStructure != null) {
            mPlatformSession =
                    new PlatformSession(
                            view.getContentCaptureSession(), viewStructure.getAutofillId());
        }
    }

    @Override
    public void onContentCaptured(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame) {
        if (mPlatformSession == null) {
            View view = mView.get();
            if (view == null) return;
            mPlatformSession = PlatformSession.fromView(view);
            if (mPlatformSession == null) return;
        }
        new ContentCapturedTask(parentFrame, contentCaptureFrame, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onContentUpdated(
            FrameSession parentFrame, ContentCaptureFrame contentCaptureFrame) {
        if (mPlatformSession == null) return;
        new ContentUpdateTask(parentFrame, contentCaptureFrame, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onSessionRemoved(FrameSession frame) {
        if (frame.isEmpty() || mPlatformSession == null) return;
        new SessionRemovedTask(frame, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onTitleUpdated(ContentCaptureFrame contentCaptureFrame) {
        if (mPlatformSession == null) return;
        new TitleUpdateTask(contentCaptureFrame, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onFaviconUpdated(ContentCaptureFrame mainFrame) {
        if (mPlatformSession == null) return;
        FrameSession session = new FrameSession(1);
        session.add(mainFrame);
        new FaviconUpdateTask(session, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public void onContentRemoved(FrameSession frame, long[] removedIds) {
        if (frame.isEmpty() || mPlatformSession == null) return;
        new ContentRemovedTask(frame, removedIds, mPlatformSession)
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @Override
    public boolean shouldCapture(String[] urls) {
        return PlatformContentCaptureController.getInstance().shouldCapture(urls);
    }
}
