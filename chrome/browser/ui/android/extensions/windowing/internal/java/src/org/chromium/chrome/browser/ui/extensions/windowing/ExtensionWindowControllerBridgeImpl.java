// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import android.graphics.Rect;
import android.util.ArrayMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Implements {@link ExtensionWindowControllerBridge}. */
@NullMarked
final class ExtensionWindowControllerBridgeImpl
        implements ExtensionWindowControllerBridge, ProfileManager.Observer {

    /**
     * Events received by the native singleton {@code WindowControllerListObserverForTesting}.
     *
     * <p>The keys are extension internal window IDs as returned by {@code
     * extensions::WindowController::GetWindowId()}, and the value for each key is the sequence of
     * events received by a window.
     */
    private static final Map<Integer, List<@ExtensionInternalWindowEventForTesting Integer>>
            sExtensionInternalEventsForTesting = new ArrayMap<>();

    /**
     * See {@link Natives#addWindowControllerListObserverForTesting()}.
     *
     * <p>Note: this is a static method, but in tests, we should call it after the native library
     * has been loaded, which usually means starting a {@code ChromeTabbedActivity}. Otherwise, the
     * JNI call in the method will cause {@code UnsatisfiedLinkError}.
     */
    static void addWindowControllerListObserverForTesting() {
        ExtensionWindowControllerBridgeImplJni.get()
                .addWindowControllerListObserverForTesting(); // IN-TEST
    }

    static void removeWindowControllerListObserverForTesting() {
        ExtensionWindowControllerBridgeImplJni.get()
                .removeWindowControllerListObserverForTesting(); // IN-TEST
        sExtensionInternalEventsForTesting.clear();
    }

    static Map<Integer, List<@ExtensionInternalWindowEventForTesting Integer>>
            getExtensionInternalEventsForTesting() {
        return sExtensionInternalEventsForTesting;
    }

    @CalledByNative
    private static void recordExtensionInternalEventForTesting( // IN-TEST
            int extensionWindowId, @ExtensionInternalWindowEventForTesting int event) {
        List<@ExtensionInternalWindowEventForTesting Integer> events =
                sExtensionInternalEventsForTesting.get(extensionWindowId);
        if (events != null) {
            events.add(event);
            return;
        }

        events = new ArrayList<>();
        events.add(event);
        sExtensionInternalEventsForTesting.put(extensionWindowId, events);
    }

    private final ChromeAndroidTask mChromeAndroidTask;

    private long mNativeExtensionWindowControllerBridge;

    ExtensionWindowControllerBridgeImpl(ChromeAndroidTask chromeAndroidTask) {
        mChromeAndroidTask = chromeAndroidTask;
    }

    @Override
    public void onAddedToTask() {
        assert mNativeExtensionWindowControllerBridge == 0
                : "ExtensionWindowControllerBridge is already added to a task.";

        ProfileManager.addObserver(this);
        mNativeExtensionWindowControllerBridge =
                ExtensionWindowControllerBridgeImplJni.get()
                        .create(
                                /* caller= */ this,
                                mChromeAndroidTask.getOrCreateNativeBrowserWindowPtr());
    }

    @Override
    public void onTaskRemoved() {
        destroyNativeExtensionWindowControllerBridge();
        ProfileManager.removeObserver(this);
    }

    @Override
    public void onTaskBoundsChanged(Rect newBoundsInDp) {
        if (mNativeExtensionWindowControllerBridge != 0) {
            ExtensionWindowControllerBridgeImplJni.get()
                    .onTaskBoundsChanged(mNativeExtensionWindowControllerBridge);
        }
    }

    @Override
    public void onTaskFocusChanged(boolean hasFocus) {
        if (mNativeExtensionWindowControllerBridge != 0) {
            ExtensionWindowControllerBridgeImplJni.get()
                    .onTaskFocusChanged(mNativeExtensionWindowControllerBridge, hasFocus);
        }
    }

    @Override
    public void onProfileAdded(Profile profile) {}

    @Override
    public void onProfileDestroyed(Profile profile) {
        // This is a short-term fix for http://crbug.com/450234852.
        //
        // The native extension code is cross-platform and depends on BrowserWindowInterface. Per
        // documentation of BrowserWindowInterface::GetProfile(), cross-platform code assumes
        // BrowserWindowInterface will always be destroyed *before* its Profile.
        //
        // See:
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/browser_window/public/browser_window_interface.h;l=108;drc=c8dc70b538f1bb0862f1be58237d6e945ee81819
        //
        // However, on Android, http://crbug.com/450234852 reveals a case where the Profile is
        // destroyed before its BrowserWindowInterface, which caused native extension code to
        // dereference an invalid Profile pointer.
        //
        // To avoid this issue, we destroy the native objects for extensions as soon as the native
        // Profile is about to be destroyed. The long-term fix should be to correct the object
        // destruction order for the case described in http://crbug.com/450234852.
        if (profile == mChromeAndroidTask.getProfile()) {
            destroyNativeExtensionWindowControllerBridge();
        }
    }

    long getNativePtrForTesting() {
        return mNativeExtensionWindowControllerBridge;
    }

    int getExtensionWindowIdForTesting() {
        assert mNativeExtensionWindowControllerBridge != 0;
        return ExtensionWindowControllerBridgeImplJni.get()
                .getExtensionWindowIdForTesting(mNativeExtensionWindowControllerBridge); // IN-TEST
    }

    private void destroyNativeExtensionWindowControllerBridge() {
        if (mNativeExtensionWindowControllerBridge != 0) {
            ExtensionWindowControllerBridgeImplJni.get()
                    .destroy(mNativeExtensionWindowControllerBridge);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeExtensionWindowControllerBridge = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code ExtensionWindowControllerBridge}.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The address of a native {@code BrowserWindowInterface}.
         *     It's the caller's responsibility to ensure the validity of the address. Failure to do
         *     so will result in undefined behavior on the native side.
         */
        long create(ExtensionWindowControllerBridgeImpl caller, long nativeBrowserWindowPtr);

        void destroy(long nativeExtensionWindowControllerBridge);

        /** Called when the window (Task) bounds have changed. */
        void onTaskBoundsChanged(long nativeExtensionWindowControllerBridge);

        /** Called when the window (Task) focus has changed. */
        void onTaskFocusChanged(long nativeExtensionWindowControllerBridge, boolean hasFocus);

        /**
         * Returns the extension internal window ID for the given {@param
         * nativeExtensionWindowControllerBridge}, as in {@code
         * extensions::WindowController::GetWindowId()}.
         */
        int getExtensionWindowIdForTesting(long nativeExtensionWindowControllerBridge); // IN-TEST

        /**
         * Add the native singleton {@code WindowControllerListObserverForTesting} for tests to
         * observe window events received by extension internals.
         */
        void addWindowControllerListObserverForTesting(); // IN-TEST

        /**
         * Removes the native singleton {@code WindowControllerListObserverForTesting} added by
         * {@link #addWindowControllerListObserverForTesting()}
         */
        void removeWindowControllerListObserverForTesting(); // IN-TEST
    }
}
