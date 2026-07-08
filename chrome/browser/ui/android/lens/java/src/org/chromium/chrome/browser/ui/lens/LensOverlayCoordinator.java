// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.webkit.JavascriptInterface;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserData;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIdentityUtils;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.UUID;

/**
 * Coordinator for the Lens Overlay feature. Stays alive for the lifetime of the {@link Tab} to
 * manage the JNI bridge and UI state.
 */
@JNINamespace("lens")
@NullMarked
public class LensOverlayCoordinator implements UserData {
    private static final String TAG = "LensOverlay";
    static final String LENS_OVERLAY_IMPL_INTENT = "intent";
    static final String LENS_OVERLAY_IMPL_WEBUI = "webui";
    static final String LENS_OVERLAY_IMPL_DEFAULT = LENS_OVERLAY_IMPL_INTENT;

    private static final String LENS_OVERLAY_JS_BRIDGE_NAME = "lensOverlay";

    /**
     * Returns whether the WebUI implementation of the Lens Overlay is enabled via feature params.
     */
    public static boolean isWebUiImplementationEnabled() {
        String implType =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.LENS_OVERLAY_ANDROID, "implementation_type");
        return LENS_OVERLAY_IMPL_WEBUI.equals(implType);
    }

    private static final String LENS_OVERLAY_DEFAULT_HTML =
            """
            <!DOCTYPE html>
            <html>
            <head>
            <style>
              html, body {
                align-items: center;
                display: flex;
                font-family: sans-serif;
                height: 100vh;
                justify-content: center;
                margin: 0;
                padding: 0;
                width: 100vw;
              }
            </style>
            </head>
            <body onclick="lensOverlay.close()" role="dialog" aria-label="Lens Overlay Placeholder">
              <h1>Lens Overlay Placeholder: Click to dismiss.</h1>
            </body>
            </html>
            """;

    private final Tab mTab;
    private long mNativeLensOverlayControllerAndroid;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mOverlayWebContents;

    /**
     * Returns the {@link LensOverlayCoordinator} for the {@link Tab}, creating it if needed.
     *
     * @param tab The tab to get the coordinator for.
     * @return The {@link LensOverlayCoordinator} for the tab.
     */
    public static LensOverlayCoordinator getOrCreateForTab(Tab tab) {
        LensOverlayCoordinator coordinator =
                tab.getUserDataHost().getUserData(LensOverlayCoordinator.class);
        if (coordinator == null) {
            coordinator = new LensOverlayCoordinator(tab);
            tab.getUserDataHost().setUserData(LensOverlayCoordinator.class, coordinator);
        }
        return coordinator;
    }

    private LensOverlayCoordinator(Tab tab) {
        mTab = tab;
        WebContents webContents = tab.getWebContents();
        assert webContents != null;
        mNativeLensOverlayControllerAndroid =
                LensOverlayCoordinatorJni.get().init(this, webContents);
    }

    /**
     * Starts the Lens Overlay.
     *
     * @param invocationSource The source of the invocation.
     * @return Whether the overlay was successfully started.
     */
    public boolean start(@LensOverlayInvocationSource int invocationSource) {
        if (LensOverlayTabHelper.isOverlayShowing(mTab)) {
            return false;
        }

        setShowing(true);

        // TODO(b/510446979): Implement TabObserver to call setShowing(false) and teardown
        // the UI if the user navigates to a new page or back/forward while the overlay
        // is active or starting up.

        Log.i(TAG, "Starting Lens Overlay");

        // The flow:
        // 1. Java calls C++ showUI().
        // 2. C++ triggers asynchronous screenshot capture.
        // 3. Upon completion, C++ invokes Java's onScreenshotCaptured().
        // Failure cases:
        // - If capture fails to start, start() returns failure immediately.
        // - If capture fails asynchronously (e.g., GPU crash), onCaptureError() is called.
        if (!LensOverlayCoordinatorJni.get()
                .showUI(mNativeLensOverlayControllerAndroid, invocationSource)) {
            Log.e(TAG, "Failed to show Lens Overlay UI");
            setShowing(false);
            return false;
        }

        return true;
    }

    /**
     * Called by the C++ controller when the window screenshot is ready. If the C++ capture fails,
     * this method is never called and the feature gracefully aborts.
     *
     * @param bitmap The captured screenshot of the Chrome window.
     */
    @CalledByNative
    void onScreenshotCaptured(@JniType("SkBitmap") Bitmap bitmap) {
        ThreadUtils.assertOnUiThread();
        String implType =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.LENS_OVERLAY_ANDROID, "implementation_type");

        if (TextUtils.isEmpty(implType)) {
            implType = LENS_OVERLAY_IMPL_DEFAULT;
        }

        boolean isRunning = false;
        Log.d(TAG, "Lens Overlay " + implType + " implementation started");
        if (LENS_OVERLAY_IMPL_INTENT.equals(implType)) {
            isRunning = startIntentFlow(bitmap);
        } else if (LENS_OVERLAY_IMPL_WEBUI.equals(implType)) {
            isRunning = startWebUIFlow(bitmap);
        } else {
            Log.e(TAG, "Unrecognized implementation type: " + implType);
        }

        if (!isRunning) {
            setShowing(false);
        }
    }

    @VisibleForTesting
    boolean startIntentFlow(Bitmap bitmap) {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) {
            return false;
        }

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            return false;
        }

        // Collect physical screen metrics on the UI thread before hopping to background.
        LensOverlayImageHelper.LensOverlayScreenMetrics metrics =
                LensOverlayImageHelper.getScreenMetrics(window);
        if (metrics == null) {
            // Fallback: If metrics can't be determined, use the windowshot as-is.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT, () -> saveCompositedImageAndLaunch(window, bitmap));
        } else {
            // Hop to background thread for heavy image processing.
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> compositeImageInBackground(window, bitmap, metrics));
        }

        return true;
    }

    @VisibleForTesting
    boolean startWebUIFlow(Bitmap bitmap) {
        clearOverlay();

        if (bitmap != null) {
            bitmap.recycle();
        }

        ViewGroup tabContentView = mTab.getContentView();
        if (tabContentView == null) {
            return false;
        }

        WebContents webContents = mTab.getWebContents();
        if (webContents == null) {
            return false;
        }

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            return false;
        }

        Context context = window.getContext().get();
        if (context == null) {
            return false;
        }

        IntentRequestTracker intentRequestTracker = getIntentRequestTracker(window);
        if (intentRequestTracker == null) {
            Log.e(TAG, "ThinWebView requires an IntentRequestTracker.");
            return false;
        }

        mThinWebView =
                ThinWebViewFactory.create(
                        context,
                        new ThinWebViewConstraints(),
                        intentRequestTracker,
                        /* enablePermissionRequests= */ false);

        mOverlayWebContents = WebContentsFactory.createWebContents(mTab.getProfile(), false, false);

        ContentView contentView = ContentView.createContentView(context, mOverlayWebContents);

        final ViewAndroidDelegate delegate = ViewAndroidDelegate.createBasicDelegate(contentView);
        mOverlayWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                delegate,
                contentView,
                window,
                WebContents.createDefaultInternalsHolder());

        mThinWebView.attachWebContents(
                mOverlayWebContents, contentView, new ThinWebViewAttachParams.Builder().build());

        JavascriptInjector injector = JavascriptInjector.fromWebContents(mOverlayWebContents);
        if (injector != null) {
            injector.addPossiblyUnsafeInterface(
                    new Object() {
                        @JavascriptInterface
                        public void close() {
                            ThreadUtils.postOnUiThread(() -> LensOverlayCoordinator.this.close());
                        }
                    },
                    LENS_OVERLAY_JS_BRIDGE_NAME,
                    JavascriptInterface.class);
        }

        tabContentView.addView(
                mThinWebView.getView(),
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mOverlayWebContents
                .getNavigationController()
                .loadUrl(
                        new LoadUrlParams(
                                "data:text/html;charset=utf-8,"
                                        + Uri.encode(LENS_OVERLAY_DEFAULT_HTML)));
        return true;
    }

    @VisibleForTesting
    void compositeImageInBackground(
            WindowAndroid window,
            Bitmap windowshot,
            LensOverlayImageHelper.LensOverlayScreenMetrics metrics) {
        ThreadUtils.assertOnBackgroundThread();
        Bitmap composited = LensOverlayImageHelper.compositeBitmap(metrics, windowshot);

        // Hop back to UI thread to interact with ShareImageFileUtils.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT, () -> saveCompositedImageAndLaunch(window, composited));
    }

    @VisibleForTesting
    void saveCompositedImageAndLaunch(WindowAndroid window, Bitmap composited) {
        ThreadUtils.assertOnUiThread();
        ShareImageFileUtils.generateTemporaryUriFromBitmap(
                UUID.randomUUID().toString(),
                composited,
                (Uri imageUri) -> {
                    composited.recycle();
                    if (imageUri != null) {
                        launchLensIntent(window, imageUri);
                    } else {
                        Log.e(TAG, "Failed to process image for Lens Overlay");
                        setShowing(false);
                    }
                });
    }

    private void launchLensIntent(WindowAndroid window, Uri imageUri) {
        String pageUrl = mTab.getUrl() != null ? mTab.getUrl().getSpec() : "";
        LensIntentParams params =
                new LensIntentParams.Builder(
                                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, mTab.isIncognito())
                        .withImageUri(imageUri)
                        .withPageUrl(pageUrl)
                        .withSrcUrl("")
                        .withImageTitleOrAltText("")
                        .withAccountName(LensIdentityUtils.getAccountName(mTab.getProfile()))
                        // Unlocking orientation is mainly to help landscape mode (seen in Desktop
                        // Android), but made unconditional here for simplicity and consistency.
                        .withForceUnlockOrientation(true)
                        .build();

        LensController.getInstance().startLens(window, params);

        // For the AGSA intent-based flow, we dismiss the overlay state immediately after
        // handing off to the external app.
        setShowing(false);
    }

    /** Called by the C++ controller when an asynchronous error occurs during capture. */
    @CalledByNative
    void onCaptureError() {
        ThreadUtils.assertOnUiThread();
        Log.e(TAG, "onCaptureError called in Java");
        setShowing(false);
    }

    private void setShowing(boolean showing) {
        LensOverlayTabHelper.setOverlayShowing(mTab, showing);
    }

    private void clearOverlay() {
        if (mThinWebView != null) {
            ViewParent parent = mThinWebView.getView().getParent();
            if (parent instanceof ViewGroup viewGroup) {
                viewGroup.removeView(mThinWebView.getView());
            }
            mThinWebView.destroy();
            mThinWebView = null;
        }
        if (mOverlayWebContents != null) {
            mOverlayWebContents.destroy();
            mOverlayWebContents = null;
        }
    }

    public void close() {
        clearOverlay();
        setShowing(false);
    }

    @Override
    public void destroy() {
        if (mNativeLensOverlayControllerAndroid != 0) {
            LensOverlayCoordinatorJni.get().destroy(mNativeLensOverlayControllerAndroid);
            mNativeLensOverlayControllerAndroid = 0;
        }
        close();
    }

    @VisibleForTesting
    @Nullable IntentRequestTracker getIntentRequestTracker(WindowAndroid window) {
        return window.getIntentRequestTracker();
    }

    @NativeMethods
    interface Natives {
        long init(
                LensOverlayCoordinator caller,
                @JniType("content::WebContents*") WebContents webContents);

        boolean showUI(
                long nativeLensOverlayControllerAndroid,
                @LensOverlayInvocationSource int invocationSource);

        void destroy(long nativeLensOverlayControllerAndroid);
    }
}
