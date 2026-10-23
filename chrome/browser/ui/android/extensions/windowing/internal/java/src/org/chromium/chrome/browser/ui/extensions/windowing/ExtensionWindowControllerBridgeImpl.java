// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import android.graphics.Rect;
import android.util.ArrayMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Implements {@link ExtensionWindowControllerBridge}. */
@NullMarked
final class ExtensionWindowControllerBridgeImpl implements ExtensionWindowControllerBridge {
    @JniType("::ExtensionWindowControllerBridge")
    interface NativeExtensionWindowControllerBridge extends JniTypeToken {}

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
    static void initializeWindowControllerListObserverForTesting() {
        ExtensionWindowControllerBridgeImplJni.get()
                .addWindowControllerListObserverForTesting(); // IN-TEST

        ResettersForTesting.register(
                () -> {
                    ExtensionWindowControllerBridgeImplJni.get()
                            .removeWindowControllerListObserverForTesting(); // IN-TEST
                    sExtensionInternalEventsForTesting.clear();
                });
    }

    static Map<Integer, List<@ExtensionInternalWindowEventForTesting Integer>>
            getExtensionInternalEventsForTesting() {
        return sExtensionInternalEventsForTesting;
    }

    @CalledByNative
    private static void recordExtensionInternalEventForTesting(
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

    private @Nullable JniUniquePtr<NativeExtensionWindowControllerBridge> mNative;

    ExtensionWindowControllerBridgeImpl() {}

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        ThreadUtils.assertOnUiThread();
        assert mNative == null : "ExtensionWindowControllerBridge is already added to a task.";

        mNative = ExtensionWindowControllerBridgeImplJni.get().create(initInfo.nativeBrowserWindow);
    }

    @Override
    public void onFeatureRemoved() {
        ThreadUtils.assertOnUiThread();
        destroyNativeExtensionWindowControllerBridge();
    }

    @Override
    public void onTaskBoundsChanged(int displayId, Rect newBoundsInDp, Rect newBoundsInPx) {
        ThreadUtils.assertOnUiThread();
        if (mNative != null) {
            ExtensionWindowControllerBridgeImplJni.get().onTaskBoundsChanged(mNative);
        }
    }

    @Override
    public void onTaskFocusChanged(boolean hasFocus) {
        ThreadUtils.assertOnUiThread();
        if (mNative != null) {
            ExtensionWindowControllerBridgeImplJni.get().onTaskFocusChanged(mNative, hasFocus);
        }
    }

    @Nullable JniPtr<NativeExtensionWindowControllerBridge> getNativePtrForTesting() {
        return mNative;
    }

    int getExtensionWindowIdForTesting() {
        assert mNative != null;
        return ExtensionWindowControllerBridgeImplJni.get()
                .getExtensionWindowIdForTesting(mNative); // IN-TEST
    }

    private void destroyNativeExtensionWindowControllerBridge() {
        if (mNative != null) {
            mNative.destroy();
            mNative = null;
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code ExtensionWindowControllerBridge}.
         *
         * @param browserWindow The native {@code BrowserWindowInterface}.
         */
        JniUniquePtr<NativeExtensionWindowControllerBridge> create(
                @Nullable JniPtr<NativeBrowserWindowInterface> browserWindow);

        /** Called when the window (Task) bounds have changed. */
        void onTaskBoundsChanged(JniPtr<NativeExtensionWindowControllerBridge> self);

        /** Called when the window (Task) focus has changed. */
        void onTaskFocusChanged(
                JniPtr<NativeExtensionWindowControllerBridge> self, boolean hasFocus);

        /**
         * Returns the extension internal window ID for the given {@code self}, as in {@code
         * extensions::WindowController::GetWindowId()}.
         */
        int getExtensionWindowIdForTesting( // IN-TEST
                JniPtr<NativeExtensionWindowControllerBridge> self);

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
