// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.ViewStructure;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_capture.ContentCaptureMetadataProto.ContentCaptureMetadata;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** This class receives captured content from native and forwards to ContentCaptureConsumer. */
@JNINamespace("content_capture")
@NullMarked
public class OnscreenContentProvider {
    private static final String TAG = "ContentCapture";
    private long mNativeOnscreenContentProviderAndroid;

    private final ArrayList<ContentCaptureConsumer> mContentCaptureConsumers = new ArrayList<>();

    private ContentCaptureMetadata.Builder mMetadataBuilder = ContentCaptureMetadata.newBuilder();

    private WeakReference<WebContents> mWebContents;

    public OnscreenContentProvider(
            Context context,
            View view,
            @Nullable ViewStructure structure,
            WebContents webContents) {
        mWebContents = new WeakReference<>(webContents);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ContentCaptureConsumer consumer =
                    PlatformContentCaptureConsumer.create(context, view, structure, webContents);
            if (consumer != null) {
                mContentCaptureConsumers.add(consumer);
            }
        }
        if (!mContentCaptureConsumers.isEmpty()) {
            createNativeObject();
        }
    }

    public OnscreenContentProvider(Context context, View view, WebContents webContents) {
        this(context, view, null, webContents);
    }

    public void destroy() {
        destroyNativeObject();
    }

    private void destroyNativeObject() {
        if (mNativeOnscreenContentProviderAndroid == 0) return;
        OnscreenContentProviderJni.get().destroy(mNativeOnscreenContentProviderAndroid);
        mNativeOnscreenContentProviderAndroid = 0;
    }

    private void createNativeObject() {
        WebContents webContents = mWebContents.get();
        if (webContents != null) {
            mNativeOnscreenContentProviderAndroid =
                    OnscreenContentProviderJni.get().init(this, webContents);
        }
    }

    public void addConsumer(ContentCaptureConsumer consumer) {
        mContentCaptureConsumers.add(consumer);
        if (mNativeOnscreenContentProviderAndroid == 0) createNativeObject();
    }

    public void removeConsumer(ContentCaptureConsumer consumer) {
        mContentCaptureConsumers.remove(consumer);
        if (mContentCaptureConsumers.isEmpty()) destroyNativeObject();
    }

    public void onWebContentsChanged(WebContents current) {
        mWebContents = new WeakReference<>(current);
        if (mNativeOnscreenContentProviderAndroid != 0) {
            OnscreenContentProviderJni.get()
                    .onWebContentsChanged(mNativeOnscreenContentProviderAndroid, current);
        }
    }

    @CalledByNative
    private void flushCaptureContent(Object[] session, ContentCaptureFrame data) {
        FrameSession frameSession = toFrameSession(session);
        String[] urls = buildUrls(frameSession, data);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onContentCaptureFlushed(frameSession, data);
            }
        }
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Flushed Capturing Content");
        }
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
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Captured Content: %s", data);
        }
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
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Updated Content: %s", data);
        }
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
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
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
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Removed Session: %s", frameSession.get(0));
        }
    }

    @CalledByNative
    private void didUpdateTitle(ContentCaptureFrame mainFrame) {
        String[] urls = buildUrls(null, mainFrame);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onTitleUpdated(mainFrame);
            }
        }
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Updated Title: %s", mainFrame.getTitle());
        }
    }

    @CalledByNative
    private void didUpdateFavicon(ContentCaptureFrame mainFrame) {
        String[] urls = buildUrls(null, mainFrame);
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) {
                consumer.onFaviconUpdated(mainFrame);
            }
        }
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Updated Favicon: %s", mainFrame.getFavicon());
        }
    }

    @CalledByNative
    private void didUpdateSensitivityScore(String url, float sensitivityScore) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }

        mMetadataBuilder.setSensitivityScore(sensitivityScore);

        ContentCaptureMetadata metadata = mMetadataBuilder.build();
        assumeNonNull(PlatformContentCaptureController.getInstance()).shareData(url, metadata);

        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(TAG, "Updated sensitivity score: %f", sensitivityScore);
        }
    }

    @CalledByNative
    private void didUpdateLanguageDetails(
            String url, String detectedLanguage, float languageConfidence) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }

        mMetadataBuilder.setDetectedLanguage(detectedLanguage);
        mMetadataBuilder.setLanguageConfidence(languageConfidence);
        ContentCaptureMetadata metadata = mMetadataBuilder.build();

        assumeNonNull(PlatformContentCaptureController.getInstance()).shareData(url, metadata);

        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i(
                    TAG,
                    "Updated language: %s, confidence: %f",
                    detectedLanguage,
                    languageConfidence);
        }
    }

    @CalledByNative
    private void clearContentCaptureMetadata() {
        // Reset the builder to discard data from the previous URL
        // when DidFinishNavigation fires in C++.
        mMetadataBuilder = ContentCaptureMetadata.newBuilder();
    }

    @CalledByNative
    private int getOffsetY(WebContents webContents) {
        return RenderCoordinates.fromWebContents(webContents).getContentOffsetYPixInt();
    }

    @CalledByNative
    private boolean shouldCapture(String url) {
        String[] urls = new String[] {url};
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer.shouldCapture(urls)) return true;
        }
        return false;
    }

    private FrameSession toFrameSession(Object[] session) {
        FrameSession frameSession = new FrameSession(session.length);
        for (Object s : session) frameSession.add((ContentCaptureFrame) s);
        return frameSession;
    }

    private String[] buildUrls(@Nullable FrameSession session, @Nullable ContentCaptureFrame data) {
        ArrayList<String> urls = new ArrayList<>();
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

    public List<ContentCaptureConsumer> getConsumersForTesting() {
        return mContentCaptureConsumers;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    @RequiresApi(Build.VERSION_CODES.Q)
    public void removePlatformConsumerForTesting() {
        for (ContentCaptureConsumer consumer : mContentCaptureConsumers) {
            if (consumer instanceof PlatformContentCaptureConsumer) {
                mContentCaptureConsumers.remove(consumer);
                return;
            }
        }
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(OnscreenContentProvider self, @Nullable WebContents webContents);

        void onWebContentsChanged(
                long nativeOnscreenContentProviderAndroid, @Nullable WebContents webContents);

        void destroy(long nativeOnscreenContentProviderAndroid);
    }
}
