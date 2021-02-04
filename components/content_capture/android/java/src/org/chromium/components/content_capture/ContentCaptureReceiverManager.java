// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * This class receives captured content from native and forwards to ContetnCaptureConsumer.
 */
public class ContentCaptureReceiverManager {
    private static final String TAG = "ContentCapture";
    private static Boolean sDump;

    private ArrayList<ContentCaptureConsumer> mContentCaptureConsumers =
            new ArrayList<ContentCaptureConsumer>();

    public static ContentCaptureReceiverManager createOrGet(WebContents webContents) {
        return ContentCaptureReceiverManagerJni.get().createOrGet(webContents);
    }

    @CalledByNative
    private ContentCaptureReceiverManager() {
        if (sDump == null) sDump = ContentCaptureFeatures.isDumpForTestingEnabled();
    }

    public void addContentCaptureConsumer(ContentCaptureConsumer consumer) {
        mContentCaptureConsumers.add(consumer);
    }

    @CalledByNative
    private void didCaptureContent(Object[] session, ContentCaptureFrame data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, data);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentCaptured(frameSession, data);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Captured Content: %s", data);
    }

    @CalledByNative
    private void didUpdateContent(Object[] session, ContentCaptureFrame data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, data);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentUpdated(frameSession, data);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Updated Content: %s", data);
    }

    @CalledByNative
    private void didRemoveContent(Object[] session, long[] data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, null);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentRemoved(frameSession, data);
            }
        }
        if (sDump.booleanValue()) {
            Log.i(TAG, "Removed Content: %s", frameSession.get(0) + " " + Arrays.toString(data));
        }
    }

    @CalledByNative
    private void didRemoveSession(Object[] session) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, null);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onSessionRemoved(frameSession);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Removed Session: %s", frameSession.get(0));
    }

    @CalledByNative
    private void didUpdateTitle(ContentCaptureFrame mainFrame) {
        String[] urls = buildUrls(null, mainFrame);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onTitleUpdated(mainFrame);
            }
        }
        if (sDump.booleanValue()) Log.i(TAG, "Updated Title: %s", mainFrame);
    }

    private FrameSession toFrameSession(Object[] session) {
        FrameSession frameSession = new FrameSession(session.length);
        for (Object s : session) frameSession.add((ContentCaptureFrame) s);
        return frameSession;
    }

    private String[] buildUrls(FrameSession session, ContentCaptureFrame data) {
        ArrayList<String> urls = new ArrayList<String>();
        if (session != null) {
            for (ContentCaptureFrame d : session) {
                urls.add(d.getUrl());
            }
        }
        if (data != null) urls.add(data.getUrl());
        String[] result = new String[urls.size()];
        urls.toArray(result);
        return result;
    }

    @NativeMethods
    interface Natives {
        ContentCaptureReceiverManager createOrGet(WebContents webContents);
    }
}
