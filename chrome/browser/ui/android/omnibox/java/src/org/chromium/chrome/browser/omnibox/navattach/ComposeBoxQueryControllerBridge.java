// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import org.jni_zero.JniType;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;

/**
 * Bridge for native Composebox query controller functionality, allowing for management of a
 * Composebox session
 */
@SuppressWarnings("unused")
@NullMarked
public class ComposeBoxQueryControllerBridge {

    private long mNativeInstance;

    public ComposeBoxQueryControllerBridge(Profile profile) {
        mNativeInstance = ComposeBoxQueryControllerBridgeJni.get().init(profile);
    }

    public void destroy() {
        ComposeBoxQueryControllerBridgeJni.get().destroy(mNativeInstance);
        mNativeInstance = 0;
    }

    /** Start a new Composebox session. An active session is required to upload files. */
    void notifySessionStarted() {
        ComposeBoxQueryControllerBridgeJni.get().notifySessionStarted(mNativeInstance);
    }

    /**
     * End the current Composebox session. This will drop all the files associated with the session.
     */
    void notifySessionAbandoned() {
        ComposeBoxQueryControllerBridgeJni.get().notifySessionAbandoned(mNativeInstance);
    }

    /**
     * Add the given file to the current session.
     *
     * @return unique token representig the file, used to manipulate added files.
     */
    String addFile(String fileName, String fileType, byte[] fileData) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(fileData.length);
        byteBuffer.put(fileData);
        return ComposeBoxQueryControllerBridgeJni.get()
                .addFile(mNativeInstance, fileName, fileType, byteBuffer);
    }

    GURL getAimUrl(String queryText) {
        return ComposeBoxQueryControllerBridgeJni.get().getAimUrl(mNativeInstance, queryText);
    }

    /** Remove the given file from the current session. */
    void removeAttachment(String token) {
        ComposeBoxQueryControllerBridgeJni.get().removeAttachment(mNativeInstance, token);
    }

    @NativeMethods
    public interface Natives {
        long init(@JniType("Profile*") Profile profile);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void destroy(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void notifySessionStarted(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void notifySessionAbandoned(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        String addFile(
                long nativeInstance,
                @JniType("std::string") String fileName,
                @JniType("std::string") String fileType,
                ByteBuffer fileData);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        @JniType("GURL")
        GURL getAimUrl(long nativeInstance, @JniType("std::string") String queryText);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void removeAttachment(long nativeInstance, @JniType("std::string") String token);
    }
}
