// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.view.KeyEvent;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

import java.util.Objects;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionActionsBridge implements Destroyable {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativeExtensionActionsBridge;

    public ExtensionActionsBridge(ChromeAndroidTask task, Profile profile) {
        mNativeExtensionActionsBridge =
                ExtensionActionsBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindowPtr(profile));
    }

    /** Represents the result of handling a key event. */
    public static class HandleKeyEventResult {
        /** Whether the key event has been handled in C++. */
        public final boolean handled;

        /** The action to trigger, or empty if no action should be triggered. */
        public final String actionId;

        @VisibleForTesting
        @CalledByNative("HandleKeyEventResult")
        public HandleKeyEventResult(boolean handled, @JniType("std::string") String actionId) {
            this.handled = handled;
            this.actionId = actionId;
        }

        @Override
        public boolean equals(Object o) {
            if (o instanceof HandleKeyEventResult other) {
                return handled == other.handled && actionId.equals(other.actionId);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(handled, actionId);
        }
    }

    @Override
    public void destroy() {
        assert mNativeExtensionActionsBridge != 0;
        ExtensionActionsBridgeJni.get().destroy(mNativeExtensionActionsBridge);
        mNativeExtensionActionsBridge = 0;
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    /**
     * Returns whether the extensions are disabled on the profile for Desktop Android. This is
     * temporary for until extensions are ready for dogfooding. TODO(crbug.com/422307625): Remove
     * this check once extensions are ready for dogfooding.
     */
    public static boolean extensionsEnabled(Profile profile) {
        return ExtensionActionsBridgeJni.get().extensionsEnabled(profile);
    }

    /** Handles the key down event and returns the result. */
    public HandleKeyEventResult handleKeyDownEvent(KeyEvent event) {
        return ExtensionActionsBridgeJni.get()
                .handleKeyDownEvent(mNativeExtensionActionsBridge, event);
    }

    @NativeMethods
    public interface Natives {
        boolean extensionsEnabled(@JniType("Profile*") Profile profile);

        long init(ExtensionActionsBridge bridge, long browserWindowInterfacePtr);

        void destroy(long nativeExtensionActionsBridge);

        HandleKeyEventResult handleKeyDownEvent(
                long nativeExtensionActionsBridge,
                @JniType("ui::KeyEventAndroid") KeyEvent keyEvent);
    }
}
