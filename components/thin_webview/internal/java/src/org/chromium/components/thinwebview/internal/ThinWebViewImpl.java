// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

/** An android view backed by a {@link Surface} that is able to display a live {@link WebContents}. */
@JNINamespace("thin_webview::android")
public class ThinWebViewImpl extends FrameLayout implements ThinWebView {
    private final CompositorView mCompositorView;
    private final WindowAndroid mWindowAndroid;
    private long mNativeThinWebViewImpl;
    private View mContentView;
    // Passed to native and stored as a weak reference, so ensure this strong
    // reference is not optimized away by R8.
    @DoNotInline private WebContentsDelegateAndroid mWebContentsDelegate;

    /**
     * Creates a {@link ThinWebViewImpl} backed by a {@link Surface}.
     * @param context The Context to create this view.
     * @param constraints A set of constraints associated with this view.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ThinWebViewImpl(
            Context context,
            ThinWebViewConstraints constraints,
            IntentRequestTracker intentRequestTracker) {
        super(context);
        if (ContextUtils.activityFromContext(context) != null) {
            mWindowAndroid =
                    new ActivityWindowAndroid(
                            context, /* listenToActivityState= */ true, intentRequestTracker);
        } else {
            mWindowAndroid = new WindowAndroid(context);
        }
        mCompositorView = new CompositorViewImpl(context, mWindowAndroid, constraints);

        LayoutParams layoutParams =
                new LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        addView(mCompositorView.getView(), layoutParams);

        mNativeThinWebViewImpl =
                ThinWebViewImplJni.get()
                        .init(ThinWebViewImpl.this, mCompositorView, mWindowAndroid);
    }

    @Override
    public View getView() {
        return this;
    }

    @Override
    public void attachWebContents(
            WebContents webContents,
            @Nullable View contentView,
            @Nullable WebContentsDelegateAndroid delegate) {
        if (mNativeThinWebViewImpl == 0) return;
        // Native code holds only a weak reference to this object.
        mWebContentsDelegate = delegate;
        setContentView(contentView);
        ThinWebViewImplJni.get()
                .setWebContents(
                        mNativeThinWebViewImpl, ThinWebViewImpl.this, webContents, delegate);
        webContents.updateWebContentsVisibility(Visibility.VISIBLE);
    }

    @Override
    public void destroy() {
        if (mNativeThinWebViewImpl == 0) return;
        if (mContentView != null) {
            removeView(mContentView);
            mContentView = null;
        }
        mCompositorView.destroy();
        ThinWebViewImplJni.get().destroy(mNativeThinWebViewImpl, ThinWebViewImpl.this);
        mNativeThinWebViewImpl = 0;
        mWindowAndroid.destroy();
    }

    @Override
    public void setAlpha(float alpha) {
        mCompositorView.setAlpha(alpha);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (mNativeThinWebViewImpl == 0) return;
        if (w != oldw || h != oldh) {
            ThinWebViewImplJni.get()
                    .sizeChanged(mNativeThinWebViewImpl, ThinWebViewImpl.this, w, h);
        }
    }

    private void setContentView(View contentView) {
        if (mContentView == contentView) return;

        if (mContentView != null) {
            assert getChildCount() > 1;
            removeViewAt(1);
        }

        mContentView = contentView;
        if (mContentView != null) addView(mContentView, 1);
    }

    @NativeMethods
    interface Natives {
        long init(
                ThinWebViewImpl caller, CompositorView compositorView, WindowAndroid windowAndroid);

        void destroy(long nativeThinWebView, ThinWebViewImpl caller);

        void setWebContents(
                long nativeThinWebView,
                ThinWebViewImpl caller,
                WebContents webContents,
                WebContentsDelegateAndroid delegate);

        void sizeChanged(long nativeThinWebView, ThinWebViewImpl caller, int width, int height);
    }
}
