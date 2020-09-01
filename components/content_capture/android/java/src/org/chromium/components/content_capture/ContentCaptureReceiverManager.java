// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;

/**
 * This class receives captured content from native and forwards to ContetnCaptureConsumer.
 */
public class ContentCaptureReceiverManager {
    private static final String TAG = "ContentCapture";
    private static Boolean sDump;

    private ContentCaptureConsumer mContentCaptureConsumer;

    public static ContentCaptureReceiverManager createOrGet(WebContents webContents) {
        return ContentCaptureReceiverManagerJni.get().createOrGet(webContents);
    }

    @CalledByNative
    private ContentCaptureReceiverManager() {
        if (sDump == null) sDump = ContentCaptureFeatures.isDumpForTestingEnabled();
    }

    public void setContentCaptureConsumer(ContentCaptureConsumer consumer) {
        mContentCaptureConsumer = consumer;
    }

    @CalledByNative
    private void didCaptureContent(Object[] session, ContentCaptureData data) {
        if (mContentCaptureConsumer != null) {
            mContentCaptureConsumer.onContentCaptured(toFrameSession(session), data);
        }
        if (sDump.booleanValue()) Log.i(TAG, "Captured Content: %s", data);
    }

    @CalledByNative
    private void didUpdateContent(Object[] session, ContentCaptureData data) {
        if (mContentCaptureConsumer != null) {
            mContentCaptureConsumer.onContentUpdated(toFrameSession(session), data);
        }
        if (sDump.booleanValue()) Log.i(TAG, "Updated Content: %s", data);
    }

    @CalledByNative
    private void didRemoveContent(Object[] session, long[] data) {
        FrameSession frameSession = toFrameSession(session);
        if (mContentCaptureConsumer != null) {
            mContentCaptureConsumer.onContentRemoved(frameSession, data);
        }
        if (sDump.booleanValue()) {
            Log.i(TAG, "Removed Content: %s", frameSession.get(0) + " " + Arrays.toString(data));
        }
    }

    @CalledByNative
    private void didRemoveSession(Object[] session) {
        FrameSession frameSession = toFrameSession(session);
        if (mContentCaptureConsumer != null) mContentCaptureConsumer.onSessionRemoved(frameSession);
        if (sDump.booleanValue()) Log.i(TAG, "Removed Session: %s", frameSession.get(0));
    }

    private FrameSession toFrameSession(Object[] session) {
        FrameSession frameSession = new FrameSession(session.length);
        for (Object s : session) frameSession.add((ContentCaptureData) s);
        return frameSession;
    }

    @NativeMethods
    interface Natives {
        ContentCaptureReceiverManager createOrGet(WebContents webContents);
    }
}
