// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.content_public.browser.WebContents;

/**
 * Manages the Java-side representation of an extension action's popup.
 *
 * <p>An extension action popup is typically a small HTML page displayed when an extension's action
 * button in the toolbar is clicked. This class owns the {@link WebContents} that hosts the popup's
 * content and bridges communication with its native C++ counterpart.
 *
 * <p>This class implements {@link Destroyable} to ensure proper cleanup of its associated native
 * resources. The native counterpart's lifetime is tied to this Java object; when {@link #destroy()}
 * is called, the native object is also destroyed.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionActionPopupContents implements Destroyable {
    /**
     * Pointer to the native C++ ExtensionActionPopupContents object. This is 0 if the native object
     * has been destroyed.
     */
    private long mNativeExtensionActionPopupContents;

    /** The WebContents hosting the extension popup's UI. */
    private final WebContents mWebContents;

    /** Delegate to handle UI events related to the popup, such as resizing. */
    @Nullable private Delegate mDelegate;

    @CalledByNative
    private ExtensionActionPopupContents(
            long nativeExtensionActionPopupContents,
            @JniType("content::WebContents*") WebContents webContents) {
        mNativeExtensionActionPopupContents = nativeExtensionActionPopupContents;
        mWebContents = webContents;
    }

    /** Creates an {@link ExtensionActionPopupContents} instance. */
    public static ExtensionActionPopupContents create(
            ChromeAndroidTask task, String actionId, int tabId) {
        return ExtensionActionPopupContentsJni.get()
                .create(task.getOrCreateNativeBrowserWindowPtr(), actionId, tabId);
    }

    /**
     * Cleans up the resources associated with this popup.
     *
     * <p>This method must be called when the popup is no longer needed to prevent resource leaks.
     * Safe to call multiple times; subsequent calls after the first are no-ops.
     */
    @Override
    public void destroy() {
        if (mNativeExtensionActionPopupContents == 0) {
            return;
        }
        ExtensionActionPopupContentsJni.get().destroy(mNativeExtensionActionPopupContents);
        mNativeExtensionActionPopupContents = 0;
    }

    /** Returns the {@link WebContents} that hosts the extension popup's UI. */
    public WebContents getWebContents() {
        return mWebContents;
    }

    /**
     * Instructs to load the initial page for the extension popup into its {@link WebContents}.
     *
     * <p>This should be called after the popup is created and its view is ready to display content.
     */
    public void loadInitialPage() {
        assert mNativeExtensionActionPopupContents != 0;
        ExtensionActionPopupContentsJni.get().loadInitialPage(mNativeExtensionActionPopupContents);
    }

    /**
     * Sets the delegate to receive UI-related callbacks for this popup.
     *
     * @param delegate The {@link Delegate} to handle events, or {@code null} to remove the current
     *     delegate.
     */
    public void setDelegate(@Nullable Delegate delegate) {
        mDelegate = delegate;
    }

    @CalledByNative
    private void resizeDueToAutoResize(int width, int height) {
        if (mDelegate != null) {
            mDelegate.resizeDueToAutoResize(width, height);
        }
    }

    @CalledByNative
    private void onLoaded() {
        if (mDelegate != null) {
            mDelegate.onLoaded();
        }
    }

    @CalledByNative
    private void onClose() {
        if (mDelegate != null) {
            mDelegate.onClose();
        }
    }

    /**
     * Interface for receiving UI-related callbacks from an {@link ExtensionActionPopupContents}.
     *
     * <p>This allows embedders or UI coordinators to react to events like content resizing.
     */
    public interface Delegate {
        /** Called when the renderer requested to resize the window to fit the content size. */
        void resizeDueToAutoResize(int width, int height);

        /** Called when it finished loading the initial page. */
        void onLoaded();

        /**
         * Called when the popup is requested to close programmatically (e.g. by window.close()).
         */
        void onClose();
    }

    @NativeMethods
    public interface Natives {
        /**
         * Creates the native ExtensionActionPopupContents object and returns its Java peer.
         *
         * @param androidBrowserWindowPtr The address of a native {@code BrowserWindowInterface}.
         * @param actionId The ID of the extension action.
         * @param tabId The ID of the tab context.
         * @return The Java {@link ExtensionActionPopupContents} object, or {@code null} on failure.
         */
        ExtensionActionPopupContents create(
                long androidBrowserWindowPtr, @JniType("std::string") String actionId, int tabId);

        /**
         * Destroys the native ExtensionActionPopupContents object.
         *
         * @param nativeExtensionActionPopupContents The pointer to the native object.
         */
        void destroy(long nativeExtensionActionPopupContents);

        /**
         * Triggers the loading of the initial URL in the native ExtensionActionPopupContents.
         *
         * @param nativeExtensionActionPopupContents The pointer to the native object.
         */
        void loadInitialPage(long nativeExtensionActionPopupContents);
    }
}
