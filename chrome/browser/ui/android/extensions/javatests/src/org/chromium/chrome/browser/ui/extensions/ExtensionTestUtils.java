// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

import java.io.File;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

/** Provides utilities for extension-related integration tests. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionTestUtils {
    /**
     * Loads an unpacked extension from a given directory, and returns a load extension's ID.
     *
     * <p>This function must be called from a non-UI thread since it needs to run some operations on
     * the UI thread, while others on other threads.
     *
     * @param profile The profile to load an extension for.
     * @param rootDir The root directory containing an unpacked extension.
     * @return The load extension's ID.
     */
    public static String loadUnpackedExtension(Profile profile, File rootDir) {
        ThreadUtils.assertOnBackgroundThread();

        CompletableFuture<String> future = new CompletableFuture<>();
        ThreadUtils.runOnUiThread(
                () -> {
                    ExtensionTestUtilsJni.get()
                            .loadUnpackedExtensionAsync(
                                    profile,
                                    rootDir.toString(),
                                    (extensionId) -> {
                                        future.complete(extensionId);
                                    });
                });

        try {
            return future.get();
        } catch (InterruptedException | ExecutionException e) {
            throw new RuntimeException(e);
        }
    }

    @NativeMethods
    public interface Natives {
        void loadUnpackedExtensionAsync(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String rootDir,
                Callback<String> callback);
    }
}
