// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.contextual_search.FileUploadStatus;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;

/**
 * Bridge for native Composebox query controller functionality, allowing for management of a
 * Composebox session
 */
@SuppressWarnings("unused")
@NullMarked
public class ComposeBoxQueryControllerBridge {

    /** Observer for file upload status changes. */
    interface FileUploadObserver {
        /**
         * @param token Unique string identifier for the file.
         * @param status The status of the file's upload.
         */
        void onFileUploadStatusChanged(String token, @FileUploadStatus int status);
    }

    private long mNativeInstance;
    private @Nullable FileUploadObserver mFileUploadObserver;

    private ComposeBoxQueryControllerBridge() {}

    /** Create a new ComposeBoxQueryControllerBridge using the given profile. */
    public static @Nullable ComposeBoxQueryControllerBridge getForProfile(Profile profile) {
        ComposeBoxQueryControllerBridge javaInstance = new ComposeBoxQueryControllerBridge();
        long nativeInstance = ComposeBoxQueryControllerBridgeJni.get().init(profile, javaInstance);
        if (nativeInstance == 0L) return null;
        javaInstance.mNativeInstance = nativeInstance;
        return javaInstance;
    }

    public void destroy() {
        ComposeBoxQueryControllerBridgeJni.get().destroy(mNativeInstance);
        mNativeInstance = 0;
        mFileUploadObserver = null;
    }

    public long getNativeInstance() {
        return mNativeInstance;
    }

    /**
     * Set the current file upload observer. If non-null, the observer will be notified of file
     * upload status changes for all files, identified by token. Note that there are intermediate
     * statuses (neither success nor failure), there is no guarantee of ordering between files, but
     * a file upload will either succeed/fail at most once.
     */
    void setFileUploadObserver(@Nullable FileUploadObserver observer) {
        mFileUploadObserver = observer;
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
    @Nullable String addFile(String fileName, String fileType, byte[] fileData) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(fileData.length);
        byteBuffer.put(fileData);
        return ComposeBoxQueryControllerBridgeJni.get()
                .addFile(mNativeInstance, fileName, fileType, byteBuffer);
    }

    /**
     * Uploads the given tab, adding it to the current session. If the upload can't be performed,
     * null is returned.
     */
    @Nullable String addTabContext(Tab tab) {
        if (tab.getWebContents() == null) return null;
        return ComposeBoxQueryControllerBridgeJni.get()
                .addTabContext(mNativeInstance, tab.getWebContents());
    }

    /**
     * Uploads the given tab, adding it to the current session. If the upload can't be performed,
     * null is returned.
     */
    @Nullable String addTabContextFromCache(long tabId) {
        return ComposeBoxQueryControllerBridgeJni.get()
                .addTabContextFromCache(mNativeInstance, tabId);
    }

    GURL getAimUrl(GURL url) {
        return ComposeBoxQueryControllerBridgeJni.get().getAimUrl(mNativeInstance, url);
    }

    GURL getImageGenerationUrl(GURL url) {
        return ComposeBoxQueryControllerBridgeJni.get().getImageGenerationUrl(mNativeInstance, url);
    }

    /** Remove the given file from the current session. */
    void removeAttachment(String token) {
        ComposeBoxQueryControllerBridgeJni.get().removeAttachment(mNativeInstance, token);
    }

    /** Returns whether the user is eligible for PDF uploads. */
    boolean isPdfUploadEligible() {
        return ComposeBoxQueryControllerBridgeJni.get().isPdfUploadEligible(mNativeInstance);
    }

    /** Returns whether the user is eligible for creating images. */
    boolean isCreateImagesEligible() {
        return ComposeBoxQueryControllerBridgeJni.get().isCreateImagesEligible(mNativeInstance);
    }

    @CalledByNative
    void onFileUploadStatusChanged(String token, @FileUploadStatus int fileUploadStatus) {
        if (mFileUploadObserver != null) {
            mFileUploadObserver.onFileUploadStatusChanged(token, fileUploadStatus);
        }
    }

    @NativeMethods
    public interface Natives {
        long init(
                @JniType("Profile*") Profile profile, ComposeBoxQueryControllerBridge javaInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void destroy(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void notifySessionStarted(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void notifySessionAbandoned(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        @Nullable String addFile(
                long nativeInstance,
                @JniType("std::string") String fileName,
                @JniType("std::string") String fileType,
                ByteBuffer fileData);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        @Nullable String addTabContext(
                long nativeInstance, @JniType("content::WebContents*") WebContents webContents);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        @Nullable String addTabContextFromCache(long nativeInstance, long tabId);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        @JniType("GURL")
        GURL getAimUrl(long nativeInstance, @JniType("GURL") GURL url);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        @JniType("GURL")
        GURL getImageGenerationUrl(long nativeInstance, @JniType("GURL") GURL url);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        void removeAttachment(long nativeInstance, @JniType("std::string") String token);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        boolean isPdfUploadEligible(long nativeInstance);

        @NativeClassQualifiedName("ComposeboxQueryControllerBridge")
        boolean isCreateImagesEligible(long nativeInstance);
    }
}
