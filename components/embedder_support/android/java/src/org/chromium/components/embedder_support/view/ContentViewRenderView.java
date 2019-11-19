// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.view;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/***
 * This view is used by a ContentView to render its content.
 * Call {@link #setCurrentWebContents(WebContents)} with the webContents that should be
 * managing the content.
 * Note that only one WebContents can be shown at a time.
 */
@JNINamespace("embedder_support")
public class ContentViewRenderView extends FrameLayout {
    // The native side of this object.
    private long mNativeContentViewRenderView;
    private SurfaceHolder.Callback mSurfaceCallback;
    private WindowAndroid mWindowAndroid;

    private final SurfaceView mSurfaceView;
    protected WebContents mWebContents;

    private int mWidth;
    private int mHeight;

    /**
     * Constructs a new ContentViewRenderView.
     * This should be called and the {@link ContentViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     */
    public ContentViewRenderView(Context context) {
        super(context);

        mSurfaceView = createSurfaceView(getContext());
        mSurfaceView.setZOrderMediaOverlay(true);

        setSurfaceViewBackgroundColor(Color.WHITE);
        addView(mSurfaceView,
                new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mSurfaceView.setVisibility(GONE);
    }

    /**
     * Initialization that requires native libraries should be done here.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param rootWindow The {@link WindowAndroid} this render view should be linked to.
     */
    public void onNativeLibraryLoaded(WindowAndroid rootWindow) {
        assert !mSurfaceView.getHolder().getSurface().isValid()
            : "Surface created before native library loaded.";
        assert rootWindow != null;
        mNativeContentViewRenderView =
                ContentViewRenderViewJni.get().init(ContentViewRenderView.this, rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
        mSurfaceCallback = new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceChanged(mNativeContentViewRenderView,
                        ContentViewRenderView.this, format, width, height, holder.getSurface());
                if (mWebContents != null) {
                    ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                            mNativeContentViewRenderView, ContentViewRenderView.this, mWebContents,
                            width, height);
                }
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceCreated(
                        mNativeContentViewRenderView, ContentViewRenderView.this);

                // On pre-M Android, layers start in the hidden state until a relayout happens.
                // There is a bug that manifests itself when entering overlay mode on pre-M devices,
                // where a relayout never happens. This bug is out of Chromium's control, but can be
                // worked around by forcibly re-setting the visibility of the surface view.
                // Otherwise, the screen stays black, and some tests fail.
                mSurfaceView.setVisibility(mSurfaceView.getVisibility());

                onReadyToRender();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                assert mNativeContentViewRenderView != 0;
                ContentViewRenderViewJni.get().surfaceDestroyed(
                        mNativeContentViewRenderView, ContentViewRenderView.this);
            }
        };
        mSurfaceView.getHolder().addCallback(mSurfaceCallback);
        mSurfaceView.setVisibility(VISIBLE);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mWidth = w;
        mHeight = h;
        if (mWebContents != null) mWebContents.setSize(w, h);
    }

    /**
     * View's method override to notify WindowAndroid about changes in its visibility.
     */
    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (mWindowAndroid == null) return;

        if (visibility == View.GONE) {
            mWindowAndroid.onVisibilityChanged(false);
        } else if (visibility == View.VISIBLE) {
            mWindowAndroid.onVisibilityChanged(true);
        }
    }

    /**
     * Sets the background color of the surface view.  This method is necessary because the
     * background color of ContentViewRenderView itself is covered by the background of
     * SurfaceView.
     * @param color The color of the background.
     */
    public void setSurfaceViewBackgroundColor(int color) {
        if (mSurfaceView != null) {
            mSurfaceView.setBackgroundColor(color);
        }
    }

    /**
     * Gets the SurfaceView for this ContentViewRenderView
     */
    public SurfaceView getSurfaceView() {
        return mSurfaceView;
    }

    /**
     * Should be called when the ContentViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
        mWindowAndroid = null;
        ContentViewRenderViewJni.get().destroy(
                mNativeContentViewRenderView, ContentViewRenderView.this);
        mNativeContentViewRenderView = 0;
    }

    public void setCurrentWebContents(WebContents webContents) {
        assert mNativeContentViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null) {
            webContents.setSize(mWidth, mHeight);
            ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                    mNativeContentViewRenderView, ContentViewRenderView.this, webContents, mWidth,
                    mHeight);
        }
        ContentViewRenderViewJni.get().setCurrentWebContents(
                mNativeContentViewRenderView, ContentViewRenderView.this, webContents);
    }

    /**
     * This method should be subclassed to provide actions to be performed once the view is ready to
     * render.
     */
    protected void onReadyToRender() {}

    /**
     * This method could be subclassed optionally to provide a custom SurfaceView object to
     * this ContentViewRenderView.
     * @param context The context used to create the SurfaceView object.
     * @return The created SurfaceView object.
     */
    protected SurfaceView createSurfaceView(Context context) {
        return new SurfaceView(context);
    }

    /**
     * @return whether the surface view is initialized and ready to render.
     */
    public boolean isInitialized() {
        return mSurfaceView.getHolder().getSurface() != null;
    }

    /**
     * Enter or leave overlay video mode.
     * @param enabled Whether overlay mode is enabled.
     */
    public void setOverlayVideoMode(boolean enabled) {
        int format = enabled ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
        mSurfaceView.getHolder().setFormat(format);
        ContentViewRenderViewJni.get().setOverlayVideoMode(
                mNativeContentViewRenderView, ContentViewRenderView.this, enabled);
    }

    @CalledByNative
    private void didSwapFrame() {
        if (mSurfaceView.getBackground() != null) {
            post(new Runnable() {
                @Override
                public void run() {
                    mSurfaceView.setBackgroundResource(0);
                }
            });
        }
    }

    @NativeMethods
    interface Natives {
        long init(ContentViewRenderView caller, WindowAndroid rootWindow);
        void destroy(long nativeContentViewRenderView, ContentViewRenderView caller);
        void setCurrentWebContents(long nativeContentViewRenderView, ContentViewRenderView caller,
                WebContents webContents);
        void onPhysicalBackingSizeChanged(long nativeContentViewRenderView,
                ContentViewRenderView caller, WebContents webContents, int width, int height);
        void surfaceCreated(long nativeContentViewRenderView, ContentViewRenderView caller);
        void surfaceDestroyed(long nativeContentViewRenderView, ContentViewRenderView caller);
        void surfaceChanged(long nativeContentViewRenderView, ContentViewRenderView caller,
                int format, int width, int height, Surface surface);
        void setOverlayVideoMode(
                long nativeContentViewRenderView, ContentViewRenderView caller, boolean enabled);
    }
}
