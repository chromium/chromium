// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * An android view backed by a {@link Surface} that is able to display a live {@link WebContents}.
 */
@JNINamespace("thin_webview::android")
@NullMarked
public class ThinWebViewImpl extends FrameLayout implements ThinWebView {
    private CompositorView mCompositorView;
    private final WindowAndroid mWindowAndroid;
    private long mNativeThinWebViewImpl;
    private @Nullable View mContentView;
    // Passed to native and stored as a weak reference, so ensure this strong
    // reference is not optimized away by R8.
    @DoNotInline private @Nullable WebContentsDelegateAndroid mWebContentsDelegate;
    private final boolean mOwnsWindowAndroid;
    private final boolean mEnablePermissionRequests;
    private @Nullable ModalDialogManager mModalDialogManager;

    /**
     * Creates a {@link ThinWebViewImpl} backed by a {@link Surface}.
     *
     * @param context The Context to create this view.
     * @param constraints A set of constraints associated with this view.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     * @param enablePermissionRequests Whether to enable permission requests.
     */
    public ThinWebViewImpl(
            Context context,
            ThinWebViewConstraints constraints,
            IntentRequestTracker intentRequestTracker,
            boolean enablePermissionRequests) {
        super(context);
        mEnablePermissionRequests = enablePermissionRequests;

        if (mEnablePermissionRequests) {
            mModalDialogManager =
                    new ModalDialogManager(
                            new AppModalPresenter(context), ModalDialogManager.ModalDialogType.APP);
        }

        if (ContextUtils.activityFromContext(context) != null) {
            mWindowAndroid =
                    new ActivityWindowAndroid(
                            context,
                            /* listenToActivityState= */ true,
                            intentRequestTracker,
                            /* insetObserver= */ null,
                            /* occlusionTrackingAllowed= */ true) {
                        @Override
                        public @Nullable ModalDialogManager getModalDialogManager() {
                            return mModalDialogManager != null
                                    ? mModalDialogManager
                                    : super.getModalDialogManager();
                        }
                    };
        } else {
            mWindowAndroid =
                    new WindowAndroid(context, /* occlusionTrackingAllowed= */ false) {
                        @Override
                        public @Nullable ModalDialogManager getModalDialogManager() {
                            return mModalDialogManager != null
                                    ? mModalDialogManager
                                    : super.getModalDialogManager();
                        }
                    };
        }

        mOwnsWindowAndroid = true;
        init(context, constraints);
    }

    /**
     * Creates a {@link ThinWebViewImpl} backed by a {@link Surface}.
     *
     * @param context The Context to create this view.
     * @param constraints A set of constraints associated with this view.
     * @param windowAndroid The {@link WindowAndroid} of the current activity.
     */
    public ThinWebViewImpl(
            Context context, ThinWebViewConstraints constraints, WindowAndroid windowAndroid) {
        super(context);
        mWindowAndroid = windowAndroid;
        mOwnsWindowAndroid = false;
        mEnablePermissionRequests = false;
        init(context, constraints);
    }

    @Initializer
    private void init(Context context, ThinWebViewConstraints constraints) {
        mCompositorView = new CompositorViewImpl(context, mWindowAndroid, constraints);

        LayoutParams layoutParams =
                new LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        addView(mCompositorView.getView(), layoutParams);

        mNativeThinWebViewImpl =
                ThinWebViewImplJni.get().init(this, mCompositorView, mWindowAndroid);
    }

    @Override
    public View getView() {
        return this;
    }

    @Override
    public void attachWebContents(
            WebContents webContents, View contentView, ThinWebViewAttachParams attachParams) {
        if (mNativeThinWebViewImpl == 0) return;

        // Native code holds only a weak reference to this object.
        mWebContentsDelegate = attachParams.webContentsDelegate;
        setContentView(contentView);
        ThinWebViewImplJni.get()
                .setWebContents(
                        mNativeThinWebViewImpl,
                        webContents,
                        attachParams.webContentsDelegate,
                        mEnablePermissionRequests,
                        attachParams.supportTheming);

        // Allow highlighting text.
        SelectionPopupController controller = SelectionPopupController.fromWebContents(webContents);
        if (attachParams.selectionDropdownMenuDelegate != null) {
            controller.setDropdownMenuDelegate(attachParams.selectionDropdownMenuDelegate);
        }
        controller.setActionModeCallback(new ThinWebViewActionModeCallback(webContents));
        controller.setSelectionClient(SelectionClient.createSmartSelectionClient(webContents));

        // Populate context menu.
        if (attachParams.contextMenuPopulatorFactory != null) {
            ThinWebViewImplJni.get()
                    .setContextMenuPopulatorFactory(
                            mNativeThinWebViewImpl, attachParams.contextMenuPopulatorFactory);
        }

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
        ThinWebViewImplJni.get().destroy(mNativeThinWebViewImpl);
        mNativeThinWebViewImpl = 0;
        if (mOwnsWindowAndroid) {
            mWindowAndroid.destroy();
        }
        if (mModalDialogManager != null) {
            mModalDialogManager.destroy();
            mModalDialogManager = null;
        }
    }

    @Override
    public void setAlpha(float alpha) {
        mCompositorView.setAlpha(alpha);
    }

    @Override
    public void runOnNextFrame(Runnable runnable) {
        mCompositorView.runOnNextFrame(runnable);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (mNativeThinWebViewImpl == 0) return;
        if (w != oldw || h != oldh) {
            ThinWebViewImplJni.get().sizeChanged(mNativeThinWebViewImpl, w, h);
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
        long init(ThinWebViewImpl self, CompositorView compositorView, WindowAndroid windowAndroid);

        void destroy(long nativeThinWebView);

        void setWebContents(
                long nativeThinWebView,
                WebContents webContents,
                @Nullable WebContentsDelegateAndroid delegate,
                boolean enablePermissionRequests,
                boolean supportTheming);

        void setContextMenuPopulatorFactory(
                long nativeThinWebView, ContextMenuPopulatorFactory factory);

        void sizeChanged(long nativeThinWebView, int width, int height);
    }
}
