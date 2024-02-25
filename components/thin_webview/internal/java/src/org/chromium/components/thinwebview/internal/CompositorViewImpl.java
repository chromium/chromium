// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.ui.base.WindowAndroid;

/**
 * An android view backed by a {@link Surface} that is able to display a cc::Layer. Either, a {@link
 * TextureView} or {@link SurfaceView} can be used to provide the surface. The cc::Layer should be
 * provided in the native.
 */
@JNINamespace("thin_webview::android")
public class CompositorViewImpl implements CompositorView {
    private final Context mContext;
    private final View mView;
    private final ThinWebViewConstraints mViewConstraints;
    private long mNativeCompositorViewImpl;

    /**
     * Creates a {@link CompositorView} backed by a {@link Surface}. The surface is provided by
     * a either a {@link TextureView} or {@link SurfaceView}.
     * @param context The context to create this view.
     * @param windowAndroid The associated {@code WindowAndroid} on which the view is to be
     *         displayed.
     * @param constraints A set of constraints associated with this view.
     */
    public CompositorViewImpl(
            Context context, WindowAndroid windowAndroid, ThinWebViewConstraints constraints) {
        mContext = context;
        mViewConstraints = constraints.clone();
        mView = useSurfaceView() ? createSurfaceView() : createTextureView();
        mNativeCompositorViewImpl =
                CompositorViewImplJni.get()
                        .init(CompositorViewImpl.this, windowAndroid, constraints.backgroundColor);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void destroy() {
        if (mNativeCompositorViewImpl != 0) {
            CompositorViewImplJni.get().destroy(mNativeCompositorViewImpl, CompositorViewImpl.this);
            mNativeCompositorViewImpl = 0;
        }
    }

    @Override
    public void requestRender() {
        if (mNativeCompositorViewImpl != 0) {
            CompositorViewImplJni.get()
                    .setNeedsComposite(mNativeCompositorViewImpl, CompositorViewImpl.this);
        }
    }

    @Override
    public void setAlpha(float alpha) {
        assert mViewConstraints.supportsOpacity;
        if (mNativeCompositorViewImpl == 0) return;
        mView.setAlpha(alpha);
    }

    private SurfaceView createSurfaceView() {
        SurfaceView surfaceView = new SurfaceView(mContext);
        surfaceView.setZOrderMediaOverlay(true);
        surfaceView
                .getHolder()
                .addCallback(
                        new SurfaceHolder.Callback() {
                            @Override
                            public void surfaceCreated(SurfaceHolder surfaceHolder) {
                                if (mNativeCompositorViewImpl == 0) return;
                                CompositorViewImplJni.get()
                                        .surfaceCreated(
                                                mNativeCompositorViewImpl, CompositorViewImpl.this);
                            }

                            @Override
                            public void surfaceChanged(
                                    SurfaceHolder surfaceHolder,
                                    int format,
                                    int width,
                                    int height) {
                                if (mNativeCompositorViewImpl == 0) return;
                                CompositorViewImplJni.get()
                                        .surfaceChanged(
                                                mNativeCompositorViewImpl,
                                                CompositorViewImpl.this,
                                                format,
                                                width,
                                                height,
                                                true,
                                                surfaceHolder.getSurface());
                            }

                            @Override
                            public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
                                if (mNativeCompositorViewImpl == 0) return;
                                CompositorViewImplJni.get()
                                        .surfaceDestroyed(
                                                mNativeCompositorViewImpl, CompositorViewImpl.this);
                            }
                        });

        return surfaceView;
    }

    private TextureView createTextureView() {
        TextureView textureView = new TextureView(mContext);
        textureView.setSurfaceTextureListener(
                new TextureView.SurfaceTextureListener() {
                    @Override
                    public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}

                    @Override
                    public void onSurfaceTextureSizeChanged(
                            SurfaceTexture surfaceTexture, int width, int height) {
                        if (mNativeCompositorViewImpl == 0) return;
                        CompositorViewImplJni.get()
                                .surfaceChanged(
                                        mNativeCompositorViewImpl,
                                        CompositorViewImpl.this,
                                        PixelFormat.OPAQUE,
                                        width,
                                        height,
                                        false,
                                        new Surface(surfaceTexture));
                    }

                    @Override
                    public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
                        if (mNativeCompositorViewImpl == 0) return false;
                        CompositorViewImplJni.get()
                                .surfaceDestroyed(
                                        mNativeCompositorViewImpl, CompositorViewImpl.this);
                        return false;
                    }

                    @Override
                    public void onSurfaceTextureAvailable(
                            SurfaceTexture surfaceTexture, int width, int height) {
                        if (mNativeCompositorViewImpl == 0) return;
                        CompositorViewImplJni.get()
                                .surfaceCreated(mNativeCompositorViewImpl, CompositorViewImpl.this);
                        CompositorViewImplJni.get()
                                .surfaceChanged(
                                        mNativeCompositorViewImpl,
                                        CompositorViewImpl.this,
                                        PixelFormat.OPAQUE,
                                        width,
                                        height,
                                        false,
                                        new Surface(surfaceTexture));
                    }
                });
        return textureView;
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeCompositorViewImpl != 0;
        return mNativeCompositorViewImpl;
    }

    @CalledByNative
    private void onCompositorLayout() {}

    @CalledByNative
    private void recreateSurface() {
        // TODO(shaktisahu): May be detach and reattach the surface view from the hierarchy.
    }

    private boolean useSurfaceView() {
        if (mViewConstraints.supportsOpacity) return false;
        // TODO(shaktisahu): Use TextureView for M81. Revert back in M82 when surface control is
        // fully enabled in Q (crbug/1031636).
        return false;
    }

    @NativeMethods
    interface Natives {
        long init(CompositorViewImpl caller, WindowAndroid windowAndroid, int backgroundColor);

        void destroy(long nativeCompositorViewImpl, CompositorViewImpl caller);

        void surfaceCreated(long nativeCompositorViewImpl, CompositorViewImpl caller);

        void surfaceDestroyed(long nativeCompositorViewImpl, CompositorViewImpl caller);

        void surfaceChanged(
                long nativeCompositorViewImpl,
                CompositorViewImpl caller,
                int format,
                int width,
                int height,
                boolean canBeUsedWithSurfaceControl,
                Surface surface);

        void setNeedsComposite(long nativeCompositorViewImpl, CompositorViewImpl caller);
    }
}
