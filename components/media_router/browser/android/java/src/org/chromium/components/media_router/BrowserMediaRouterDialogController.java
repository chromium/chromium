// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.mediarouter.app.MediaRouteChooserDialogFragment;
import androidx.mediarouter.app.MediaRouteControllerDialogFragment;
import androidx.mediarouter.media.MediaRouteSelector;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.media_router.caf.CastMediaSource;
import org.chromium.components.media_router.caf.remoting.RemotingMediaSource;
import org.chromium.content_public.browser.WebContents;

/**
 * Implements the JNI interface called from the C++ Media Router dialog controller implementation on
 * Android.
 */
@JNINamespace("media_router")
public class BrowserMediaRouterDialogController implements MediaRouteDialogDelegate {

    private final long mNativeDialogController;
    private BaseMediaRouteDialogManager mDialogManager;
    private WebContents mWebContents;

    /**
     * Returns a new initialized {@link BrowserMediaRouterDialogController}.
     * @param nativeDialogController the handle of the native object.
     * @param webContents the initiator of the dialog.
     * @return a new dialog controller to use from the native side.
     */
    @CalledByNative
    public static BrowserMediaRouterDialogController create(
            long nativeDialogController, WebContents webContents) {
        return new BrowserMediaRouterDialogController(nativeDialogController, webContents);
    }

    /**
     * Shows the {@link MediaRouteChooserDialogFragment} if it's not shown yet.
     * @param sourceUrns the URNs identifying the media sources to filter the devices with.
     */
    @CalledByNative
    public void openRouteChooserDialog(String[] sourceUrns) {
        if (isShowingDialog()) return;

        MediaSource source = null;
        for (String sourceUrn : sourceUrns) {
            source = CastMediaSource.from(sourceUrn);
            if (source == null) source = RemotingMediaSource.from(sourceUrn);

            if (source != null) break;
        }

        MediaRouteSelector routeSelector = source == null ? null : source.buildRouteSelector();

        if (routeSelector == null) {
            BrowserMediaRouterDialogControllerJni.get()
                    .onMediaSourceNotSupported(
                            mNativeDialogController, BrowserMediaRouterDialogController.this);
            return;
        }

        mDialogManager =
                new MediaRouteChooserDialogManager(source.getSourceId(), routeSelector, this);
        mDialogManager.openDialog(mWebContents);
    }

    /**
     * Shows the {@link MediaRouteControllerDialogFragment} if it's not shown yet.
     * @param sourceUrn the URN identifying the media source of the current media route.
     * @param mediaRouteId the identifier of the route to be controlled.
     */
    @CalledByNative
    public void openRouteControllerDialog(String sourceUrn, String mediaRouteId) {
        if (isShowingDialog()) return;

        MediaSource source = CastMediaSource.from(sourceUrn);
        if (source == null) source = RemotingMediaSource.from(sourceUrn);

        MediaRouteSelector routeSelector = source == null ? null : source.buildRouteSelector();

        if (routeSelector == null) {
            BrowserMediaRouterDialogControllerJni.get()
                    .onMediaSourceNotSupported(
                            mNativeDialogController, BrowserMediaRouterDialogController.this);
            return;
        }

        mDialogManager =
                new MediaRouteControllerDialogManager(
                        source.getSourceId(), routeSelector, mediaRouteId, this);
        mDialogManager.openDialog(mWebContents);
    }

    /** Closes the currently open dialog if it's open. */
    @CalledByNative
    public void closeDialog() {
        if (mDialogManager == null) return;

        mDialogManager.closeDialog();
        mDialogManager = null;
    }

    /** @return if any media route dialog is currently open. */
    @CalledByNative
    public boolean isShowingDialog() {
        return mDialogManager != null && mDialogManager.isShowingDialog();
    }

    @Override
    public void onSinkSelected(String sourceUrn, MediaSink sink) {
        mDialogManager = null;
        BrowserMediaRouterDialogControllerJni.get()
                .onSinkSelected(
                        mNativeDialogController,
                        BrowserMediaRouterDialogController.this,
                        sourceUrn,
                        sink.getId());
    }

    @Override
    public void onRouteClosed(String mediaRouteId) {
        mDialogManager = null;
        BrowserMediaRouterDialogControllerJni.get()
                .onRouteClosed(
                        mNativeDialogController,
                        BrowserMediaRouterDialogController.this,
                        mediaRouteId);
    }

    @Override
    public void onDialogCancelled() {
        // For MediaRouteControllerDialog this method will be called in case the route is closed
        // since it only call onDismiss() and there's no way to distinguish between the two.
        // Here we can figure it out: if mDialogManager is null, onRouteClosed() was called and
        // there's no need to tell the native controller the dialog has been cancelled.
        if (mDialogManager == null) return;

        mDialogManager = null;
        BrowserMediaRouterDialogControllerJni.get()
                .onDialogCancelled(
                        mNativeDialogController, BrowserMediaRouterDialogController.this);
    }

    private BrowserMediaRouterDialogController(
            long nativeDialogController, WebContents webContents) {
        mNativeDialogController = nativeDialogController;
        mWebContents = webContents;
    }

    @NativeMethods
    interface Natives {
        void onDialogCancelled(
                long nativeMediaRouterDialogControllerAndroid,
                BrowserMediaRouterDialogController caller);

        void onSinkSelected(
                long nativeMediaRouterDialogControllerAndroid,
                BrowserMediaRouterDialogController caller,
                String sourceUrn,
                String sinkId);

        void onRouteClosed(
                long nativeMediaRouterDialogControllerAndroid,
                BrowserMediaRouterDialogController caller,
                String routeId);

        void onMediaSourceNotSupported(
                long nativeMediaRouterDialogControllerAndroid,
                BrowserMediaRouterDialogController caller);
    }
}
