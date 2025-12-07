// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.view.KeyEvent;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * JNI bridge for ExtensionKeybindingRegistryAndroid (C++). Owns the native
 * ExtensionKeybindingRegistryAndroid object and handles communication between Java and C++.
 */
@NullMarked
@JNINamespace("extensions")
@ServiceImpl(ExtensionKeybindingRegistry.class)
public class ExtensionKeybindingRegistryAndroid implements ExtensionKeybindingRegistry {
    private long mNativeExtensionKeybindingRegistryAndroid;

    @Override
    public void initialize(Profile profile) {
        mNativeExtensionKeybindingRegistryAndroid =
                ExtensionKeybindingRegistryAndroidJni.get().init(profile);
    }

    @Override
    public boolean handleKeyEvent(KeyEvent event) {
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() > 0) return false;

        if (mNativeExtensionKeybindingRegistryAndroid == 0) return false;

        return ExtensionKeybindingRegistryAndroidJni.get()
                .handleKeyEvent(mNativeExtensionKeybindingRegistryAndroid, event);
    }

    /** Destroys the native counterpart. */
    @Override
    public void destroy() {
        if (mNativeExtensionKeybindingRegistryAndroid != 0) {
            ExtensionKeybindingRegistryAndroidJni.get()
                    .destroy(mNativeExtensionKeybindingRegistryAndroid);
            mNativeExtensionKeybindingRegistryAndroid = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void destroy(long nativeExtensionKeybindingRegistryAndroid);

        boolean handleKeyEvent(
                long nativeExtensionKeybindingRegistryAndroid,
                @JniType("ui::KeyEventAndroid") KeyEvent event);
    }
}
