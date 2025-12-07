// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static android.view.Display.INVALID_DISPLAY;

import android.graphics.Bitmap;
import android.view.KeyEvent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Callback;
import org.chromium.base.JniOnceCallback;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;

/** Java peer of the native class of the same name. */
@JNINamespace("web_contents_delegate_android")
@NullMarked
public class WebContentsDelegateAndroid {
    // Equivalent of WebCore::WebConsoleMessage::LevelTip.
    public static final int LOG_LEVEL_TIP = 0;
    // Equivalent of WebCore::WebConsoleMessage::LevelLog.
    public static final int LOG_LEVEL_LOG = 1;
    // Equivalent of WebCore::WebConsoleMessage::LevelWarning.
    public static final int LOG_LEVEL_WARNING = 2;
    // Equivalent of WebCore::WebConsoleMessage::LevelError.
    public static final int LOG_LEVEL_ERROR = 3;

    /**
     * @param disposition The new tab disposition, defined in
     *     //ui/base/mojo/window_open_disposition.mojom.
     * @param isRendererInitiated Whether or not the renderer initiated this action.
     */
    @CalledByNative
    public void openNewTab(
            GURL url,
            String extraHeaders,
            ResourceRequestBody postData,
            int disposition,
            boolean isRendererInitiated) {}

    @CalledByNative
    public void activateContents() {}

    @CalledByNative
    public void closeContents() {}

    @CalledByNative
    public void loadingStateChanged(boolean shouldShowLoadingUi) {}

    @CalledByNative
    public void navigationStateChanged(int flags) {}

    @CalledByNative
    public void visibleSSLStateChanged() {}

    /** Signaled when the renderer has been deemed to be unresponsive. */
    @CalledByNative
    public void rendererUnresponsive() {}

    /** Signaled when the render has been deemed to be responsive. */
    @CalledByNative
    public void rendererResponsive() {}

    @CalledByNative
    public boolean shouldCreateWebContents(GURL targetUrl) {
        return true;
    }

    @CalledByNative
    public void onUpdateTargetUrl(GURL url) {}

    @CalledByNative
    public boolean takeFocus(boolean reverse) {
        return false;
    }

    @CalledByNative
    public boolean preHandleKeyboardEvent(long nativeKeyEvent) {
        return false;
    }

    @CalledByNative
    public void handleKeyboardEvent(KeyEvent event) {
        // TODO(bulach): we probably want to re-inject the KeyEvent back into
        // the system. Investigate if this is at all possible.
    }

    /**
     * Report a JavaScript console message.
     *
     * @param level message level. One of WebContentsDelegateAndroid.LOG_LEVEL*.
     * @param message the error message.
     * @param lineNumber the line number int the source file at which the error is reported.
     * @param sourceId the name of the source file that caused the error.
     * @return true if the client will handle logging the message.
     */
    @CalledByNative
    public boolean addMessageToConsole(int level, String message, int lineNumber, String sourceId) {
        return false;
    }

    /**
     * Report a form resubmission. The overwriter of this function should eventually call either of
     * NavigationController.ContinuePendingReload or NavigationController.CancelPendingReload.
     */
    @CalledByNative
    public void showRepostFormWarningDialog() {}

    @CalledByNative
    public void enterFullscreenModeForTab(
            RenderFrameHost renderFrameHost,
            boolean prefersNavigationBar,
            boolean prefersStatusBar,
            long displayId) {}

    @CalledByNative
    public void fullscreenStateChangedForTab(
            RenderFrameHost renderFrameHost,
            boolean prefersNavigationBar,
            boolean prefersStatusBar,
            long displayId) {}

    @CalledByNative
    public void exitFullscreenModeForTab() {}

    @CalledByNative
    public boolean isFullscreenForTabOrPending() {
        return false;
    }

    @CalledByNative
    public long getFullscreenTargetDisplay() {
        return INVALID_DISPLAY;
    }

    @CalledByNative
    public void requestKeyboardLock(boolean escKeyLocked) {}

    @CalledByNative
    public void cancelKeyboardLockRequest() {}

    /**
     * Called when BrowserMediaPlayerManager wants to load a media resource.
     *
     * @param url the URL of media resource to load.
     * @return true to prevent the resource from being loaded.
     */
    @CalledByNative
    public boolean shouldBlockMediaRequest(GURL url) {
        return false;
    }

    /**
     * @return The height of the top controls in physical pixels (not DIPs).
     */
    @CalledByNative
    public int getTopControlsHeight() {
        return 0;
    }

    /**
     * @return The minimum visible height the top controls can have in physical pixels (not DIPs).
     */
    @CalledByNative
    public int getTopControlsMinHeight() {
        return 0;
    }

    /**
     * @return The height of the bottom controls in physical pixels (not DIPs).
     */
    @CalledByNative
    public int getBottomControlsHeight() {
        return 0;
    }

    /**
     * @return The minimum visible height the bottom controls can have in physical pixels (not
     *     DIPs).
     */
    @CalledByNative
    public int getBottomControlsMinHeight() {
        return 0;
    }

    /**
     * @return Whether or not the browser controls height changes should be animated.
     */
    @CalledByNative
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return false;
    }

    /**
     * @return Whether or not the browser controls resize Blink's view size.
     */
    @CalledByNative
    public boolean controlsResizeView() {
        return false;
    }

    /**
     * @return If shown, returns the height of the virtual keyboard in physical pixels. Otherwise,
     *     returns 0.
     */
    @CalledByNative
    public int getVirtualKeyboardHeight() {
        return 0;
    }

    /**
     * Check and return the {@link DisplayMode} value.
     *
     * @return The {@link DisplayMode} value.
     */
    @CalledByNative
    protected final int getDisplayModeChecked() {
        int displayMode = getDisplayMode();
        assert DisplayMode.isKnownValue(displayMode);
        return displayMode;
    }

    @CalledByNative
    public void didBackForwardTransitionAnimationChange() {}

    @CalledByNative
    private boolean maybeCopyContentAreaAsBitmap(
            JniOnceCallback<@Nullable ScreenshotResult> callback) {
        boolean result = maybeCopyContentArea(callback, ScreenshotResult.Destination.BITMAP);
        if (!result) {
            // If the method returns false, the callback won't be called, so we need to destroy it
            // to prevent memory leaks and match the previous behavior of no callback.
            callback.destroy();
        }
        return result;
    }

    @CalledByNative
    private boolean maybeCopyContentAreaAsHardwareBuffer(
            JniOnceCallback<@Nullable ScreenshotResult> callback) {
        boolean result =
                maybeCopyContentArea(callback, ScreenshotResult.Destination.HARDWARE_BUFFER);
        if (!result) {
            // If the method returns false, the callback won't be called, so we need to destroy it
            // to prevent memory leaks and match the previous behavior of no callback.
            callback.destroy();
        }
        return result;
    }

    /**
     * Used to fetch the color info to compose the fallback UX for the navigation transitions when
     * no valid screenshots are available.
     *
     * @return The rounded rectangle's color.
     */
    @CalledByNative
    public int getBackForwardTransitionFallbackUXFaviconBackgroundColor() {
        return 0;
    }

    /**
     * Used to fetch the color info to compose the fallback UX for the navigation transitions when
     * no valid screenshots are available.
     *
     * @return The fallback UX's background color.
     */
    @CalledByNative
    public int getBackForwardTransitionFallbackUXPageBackgroundColor() {
        return 0;
    }

    /**
     * Request the delegate to change the zoom level of the current tab.
     *
     * @param zoomIn Whether to zoom in or out.
     */
    @CalledByNative
    public void contentsZoomChange(boolean zoomIn) {}

    /**
     * Returns whether to override user agent for prerendering navigation.
     *
     * @param url The target URL of the prerendering navigation.
     */
    @CalledByNative
    public @UserAgentOverrideOption int shouldOverrideUserAgentForPreloading(GURL url) {
        // Inherit UA override of the last committed navigation regardless of URL as fallback.
        return UserAgentOverrideOption.INHERIT;
    }

    /**
     * Capture current visible native view as a bitmap.
     *
     * @param callback Executed asynchronously with the captured screenshot if this returns true.
     *     Note this callback is guaranteed to not retain a reference to this bitmap once it
     *     returns.
     * @param destination whether to return the result as a Bitmap or a Hardware Buffer.
     * @return True if a native view such as an NTP is presenting.
     */
    public boolean maybeCopyContentArea(
            Callback<@Nullable ScreenshotResult> callback,
            ScreenshotResult.Destination destination) {
        return false;
    }

    /**
     * Synchronous version of {@link #maybeCopyContentAreaAsBitmap(long)}
     *
     * @return Null if there is no native view corresponding to the currently committed navigation
     *     entry or capture fails; otherwise, a bitmap object.
     */
    @CalledByNative
    public @Nullable Bitmap maybeCopyContentAreaAsBitmapSync() {
        return null;
    }

    /**
     * @return Null if the embedder fails to provide a privileged internal icon; otherwise, a bitmap
     *     object for the icon.
     */
    @CalledByNative
    public @Nullable Bitmap getBackForwardTransitionFallbackUXInternalPageIcon() {
        return null;
    }

    /**
     * @return The {@link DisplayMode} value.
     */
    public int getDisplayMode() {
        return DisplayMode.UNDEFINED;
    }

    /**
     * CloseWatcher web API support. If the currently focused frame has a CloseWatcher registered in
     * JavaScript, the CloseWatcher should receive the next "close" operation, based on what the OS
     * convention for closing is. This function is called when the focused frame changes or a
     * CloseWatcher registered/unregistered to update whether the CloseWatcher should intercept.
     */
    @CalledByNative
    public void didChangeCloseSignalInterceptStatus() {}

    /**
     * Requests that the web contents obtain a pointer lock.
     *
     * <p>A pointer lock restricts the mouse cursor to the bounds of the view and provides relative
     * motion events as the user moves the mouse.
     *
     * @param webContents The {@link WebContents} for which to request the pointer lock.
     * @param userGesture {@code true} if the request is a result of a user gesture, {@code false}
     *     otherwise. A user gesture is required for the pointer lock to be granted.
     * @param lastUnlockedByTarget {@code true} if the pointer was previously unlocked by the target
     *     (website) itself, {@code false} if it was unlocked by the system or user action. This
     *     flag helps to prevent websites from instantly re-locking the pointer after it has been
     *     released by the user or system.
     */
    public void requestPointerLock(
            WebContents webContents, boolean userGesture, boolean lastUnlockedByTarget) {}

    /**
     * Called when the pointer lock is lost, either by a system event, user action or when the
     * focused view changes.
     *
     * <p>This method is invoked when the pointer lock, previously requested via {@link
     * #requestPointerLock}, is no longer active. This can occur for several reasons:
     *
     * <ul>
     *   <li>The user pressed the escape key or a system-defined key to release the lock.
     *   <li>The application called {@code document.exitPointerLock()} in JavaScript.
     *   <li>The window lost focus.
     *   <li>The view hierarchy capturing the pointer went out of focus.
     *   <li>The system released the pointer lock for other reasons.
     * </ul>
     */
    public void lostPointerLock() {}
}
