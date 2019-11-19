// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Helper class for publishing download files to the public download collection.
 */
@JNINamespace("download")
public class DownloadCollectionBridge {
    // Singleton instance that allows embedders to replace their implementation.
    private static DownloadCollectionBridge sDownloadCollectionBridge;
    private static final String TAG = "DownloadCollection";
    // Guards access to sDownloadCollectionBridge.
    private static final Object sLock = new Object();

    /**
     *  Class representing the Uri and display name pair for downloads.
     */
    protected static class DisplayNameInfo {
        private final String mUri;
        private final String mDisplayName;

        public DisplayNameInfo(String uri, String displayName) {
            mUri = uri;
            mDisplayName = displayName;
        }

        @CalledByNative("DisplayNameInfo")
        private String getDownloadUri() {
            return mUri;
        }

        @CalledByNative("DisplayNameInfo")
        private String getDisplayName() {
            return mDisplayName;
        }
    }

    /**
     * Return getDownloadCollectionBridge singleton.
     */
    public static DownloadCollectionBridge getDownloadCollectionBridge() {
        synchronized (sLock) {
            if (sDownloadCollectionBridge == null) {
                sDownloadCollectionBridge = new DownloadCollectionBridge();
            }
        }
        return sDownloadCollectionBridge;
    }

    /**
     * Sets the singlton object to use later.
     */
    public static void setDownloadCollectionBridge(DownloadCollectionBridge bridge) {
        synchronized (sLock) {
            sDownloadCollectionBridge = bridge;
        }
    }

    /**
     * Returns whether a download needs to be published.
     * @param filePath File path of the download.
     * @return True if the download needs to be published, or false otherwise.
     */
    public boolean needToPublishDownload(final String filePath) {
        return false;
    }

    /**
     * Creates a pending session for download to be written into.
     * @param fileName Name of the file.
     * @param mimeType Mime type of the file.
     * @param originalUrl Originating URL of the download.
     * @param referrer Referrer of the download.
     * @return Uri created for the pending session.
     */
    protected Uri createPendingSession(final String fileName, final String mimeType,
            final String originalUrl, final String referrer) {
        return null;
    }

    /**
     * Copy file content from a source file to the pending Uri.
     * @param sourcePath File content to be copied from.
     * @param pendingUri Destination Uri to be copied to.
     * @return true on success, or false otherwise.
     */
    protected boolean copyFileToPendingUri(final String sourcePath, final String pendingUri) {
        return false;
    }

    /**
     * Abandon the the intermediate Uri.
     * @param pendingUri Intermediate Uri that is going to be deleted.
     */
    protected void abandonPendingUri(final String pendingUri) {}

    /**
     * Publish a completed download to public repository.
     * @param pendingUri Pending uri to publish.
     * @return Uri of the published file.
     */
    protected Uri publishCompletedDownload(final String pendingUri) {
        return null;
    }

    /**
     * Gets the content URI of the download that has the given file name.
     * @param pendingUri name of the file.
     * @return Uri of the download with the given display name.
     */
    public Uri getDownloadUriForFileName(final String fileName) {
        return null;
    }

    /**
     * Renames a download Uri with a display name.
     * @param downloadUri Uri of the download.
     * @param displayName New display name for the download.
     * @return whether rename was successful.
     */
    protected boolean rename(final String downloadUri, final String displayName) {
        return false;
    }

    /**
     * @return  Whether download display names needs to be retrieved.
     */
    protected boolean needToGetDisplayNames() {
        return false;
    }

    /**
     * Gets the display names for all downloads
     * @return an array of download Uri and display name pair.
     */
    protected DisplayNameInfo[] getDisplayNames() {
        return null;
    }

    /**
     * @return whether download collection is supported.
     */
    protected boolean isDownloadCollectionSupported() {
        return false;
    }

    /**
     *  Refreshes the expiration date so the unpublished download won't get abandoned.
     *  @param intermediateUri The intermediate Uri that is not yet published.
     */
    protected void refreshExpirationDate(final String intermediateUri) {}

    /**
     * Gets the display name for a download.
     * @param downloadUri Uri of the download.
     * @return the display name of the download.
     */
    protected String getDisplayNameForUri(final String downloadUri) {
        return null;
    }

    /**
     * Creates an intermediate URI for download to be written into. On completion, call
     * nativeOnCreateIntermediateUriResult() with |callbackId|.
     * @param fileName Name of the file.
     * @param mimeType Mime type of the file.
     * @param originalUrl Originating URL of the download.
     * @param referrer Referrer of the download.
     */
    @CalledByNative
    public static String createIntermediateUriForPublish(final String fileName,
            final String mimeType, final String originalUrl, final String referrer) {
        Uri uri = getDownloadCollectionBridge().createPendingSession(
                fileName, mimeType, originalUrl, referrer);
        return uri == null ? null : uri.toString();
    }

    /**
     * Returns whether a download needs to be published.
     * @param filePath File path of the download.
     * @return True if the download needs to be published, or false otherwise.
     */
    @CalledByNative
    private static boolean shouldPublishDownload(final String filePath) {
        return getDownloadCollectionBridge().needToPublishDownload(filePath);
    }

    /**
     * Copies file content from a source file to the destination Uri.
     * @param sourcePath File content to be copied from.
     * @param destinationUri Destination Uri to be copied to.
     * @return True on success, or false otherwise.
     */
    @CalledByNative
    public static boolean copyFileToIntermediateUri(
            final String sourcePath, final String destinationUri) {
        return getDownloadCollectionBridge().copyFileToPendingUri(sourcePath, destinationUri);
    }

    /**
     * Deletes the intermediate Uri.
     * @param uri Intermediate Uri that is going to be deleted.
     */
    @CalledByNative
    public static void deleteIntermediateUri(final String uri) {
        getDownloadCollectionBridge().abandonPendingUri(uri);
    }

    /**
     * Publishes the completed download to public download collection.
     * @param intermediateUri Intermediate Uri that is going to be published.
     * @return Uri of the published file.
     */
    @CalledByNative
    public static String publishDownload(final String intermediateUri) {
        Uri uri = getDownloadCollectionBridge().publishCompletedDownload(intermediateUri);
        return uri == null ? null : uri.toString();
    }

    /**
     * Opens the intermediate Uri for writing.
     * @param intermediateUri Intermediate Uri that is going to be written to.
     * @return file descriptor that is opened for writing.
     */
    @CalledByNative
    private static int openIntermediateUri(final String intermediateUri) {
        try {
            ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
            ParcelFileDescriptor pfd =
                    resolver.openFileDescriptor(Uri.parse(intermediateUri), "rw");
            getDownloadCollectionBridge().refreshExpirationDate(intermediateUri);
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "Cannot open intermediate Uri.", e);
        }
        return -1;
    }

    /**
     * @return whether a download with the file name exists.
     */
    @CalledByNative
    private static boolean fileNameExists(final String fileName) {
        Uri uri = getDownloadCollectionBridge().getDownloadUriForFileName(fileName);
        return uri != null;
    }

    /**
     * Renames a download Uri with a display name.
     * @param downloadUri Uri of the download.
     * @param displayName New display name for the download.
     * @return whether rename was successful.
     */
    @CalledByNative
    private static boolean renameDownloadUri(final String downloadUri, final String displayName) {
        return getDownloadCollectionBridge().rename(downloadUri, displayName);
    }

    /**
     * @return  Whether download display names needs to be retrieved.
     */
    @CalledByNative
    private static boolean needToRetrieveDisplayNames() {
        return getDownloadCollectionBridge().needToGetDisplayNames();
    }

    /**
     * Gets the display names for all downloads
     * @return an array of download Uri and display name pair.
     */
    @CalledByNative
    private static DisplayNameInfo[] getDisplayNamesForDownloads() {
        return getDownloadCollectionBridge().getDisplayNames();
    }

    /**
     * @return whether download collection is supported.
     */
    public static boolean supportsDownloadCollection() {
        return getDownloadCollectionBridge().isDownloadCollectionSupported();
    }

    /**
     * @return number of days for an intermediate download to expire.
     */
    public static int getExpirationDurationInDays() {
        return DownloadCollectionBridgeJni.get().getExpirationDurationInDays();
    }

    /**
     * Gets the display name for a download.
     * @param downloadUri Uri of the download.
     * @return the display name of the download.
     */
    @CalledByNative
    private static String getDisplayName(final String downloadUri) {
        return getDownloadCollectionBridge().getDisplayNameForUri(downloadUri);
    }

    @NativeMethods
    interface Natives {
        int getExpirationDurationInDays();
    }
}
