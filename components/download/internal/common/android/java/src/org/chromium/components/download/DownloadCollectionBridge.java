// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;
import android.provider.BaseColumns;
import android.provider.MediaStore;
import android.provider.MediaStore.Downloads;
import android.provider.MediaStore.MediaColumns;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.third_party.android.provider.MediaStoreUtils;
import org.chromium.third_party.android.provider.MediaStoreUtils.PendingParams;
import org.chromium.third_party.android.provider.MediaStoreUtils.PendingSession;

import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** Helper class for publishing download files to the public download collection. */
@JNINamespace("download")
public class DownloadCollectionBridge {
    private static final String TAG = "DownloadCollection";

    // File name pattern to be used when media store has too many duplicates. This matches
    // that of download_path_reservation_tracker.cc.
    private static final String FILE_NAME_PATTERN = "yyyy-MM-dd'T'HHmmss.SSS";

    private static final List<String> COMMON_DOUBLE_EXTENSIONS =
            new ArrayList<String>(Arrays.asList("tar.gz", "tar.z", "tar.bz2", "tar.bz", "user.js"));

    private static DownloadDelegate sDownloadDelegate = new DownloadDelegate();

    /**  Class representing the Uri and display name pair for downloads. */
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
     * Sets the DownloadDelegate to be used for utility methods.
     * TODO(qinmin): remove this method once we moved all the utility methods into
     * components/.
     * @param downloadDelegate The new delegate to be used.
     */
    public static void setDownloadDelegate(DownloadDelegate downloadDelegate) {
        // TODO(qinmin): On Android O, ClassLoader may need to access disk when
        // setting the |sDownloadDelegate|. Move this to a background thread.
        // See http://crbug.com/1061042.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            sDownloadDelegate = downloadDelegate;
        }
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
    public static String createIntermediateUriForPublish(
            final String fileName,
            final String mimeType,
            final String originalUrl,
            final String referrer) {
        Uri uri = createPendingSessionInternal(fileName, mimeType, originalUrl, referrer);
        if (uri != null) return uri.toString();

        // If there are too many duplicates on the same file name, createPendingSessionInternal()
        // will return null. Generate a new file name with timestamp.
        SimpleDateFormat sdf = new SimpleDateFormat(FILE_NAME_PATTERN, Locale.getDefault());
        // Remove the extension first.
        String baseName = getBaseName(fileName);
        String extension = fileName.substring(baseName.length());
        StringBuilder sb = new StringBuilder(baseName);
        sb.append(" - ");
        sb.append(sdf.format(new Date()));
        sb.append(extension);
        uri = createPendingSessionInternal(sb.toString(), mimeType, originalUrl, referrer);
        return uri == null ? null : uri.toString();
    }

    /**
     * Returns whether a download needs to be published.
     * @param filePath File path of the download.
     * @return True if the download needs to be published, or false otherwise.
     */
    @CalledByNative
    public static boolean shouldPublishDownload(final String filePath) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            if (filePath == null) return false;
            // Only need to publish downloads that are on primary storage.
            return !sDownloadDelegate.isDownloadOnSDCard(filePath);
        }
        return false;
    }

    /**
     * Copies file content from a source file to the destination Uri.
     * @param sourcePath File content to be copied from.
     * @param destinationUri Destination Uri to be copied to.
     * @return True on success, or false otherwise.
     */
    @CalledByNative
    @RequiresApi(29)
    public static boolean copyFileToIntermediateUri(
            final String sourcePath, final String destinationUri) {
        try {
            PendingSession session = openPendingUri(destinationUri);
            OutputStream out = session.openOutputStream();
            InputStream in = new FileInputStream(sourcePath);
            FileUtils.copy(in, out);
            in.close();
            out.close();
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Unable to copy content to pending Uri.", e);
        }
        return false;
    }

    /**
     * Deletes the intermediate Uri.
     * @param uri Intermediate Uri that is going to be deleted.
     */
    @CalledByNative
    public static void deleteIntermediateUri(final String uri) {
        PendingSession session = openPendingUri(uri);
        session.abandon();
    }

    /**
     * Publishes the completed download to public download collection.
     * @param intermediateUri Intermediate Uri that is going to be published.
     * @return Uri of the published file.
     */
    @CalledByNative
    public static String publishDownload(final String intermediateUri) {
        // Android Q's MediaStore.Downloads has an issue that the custom mime type which is not
        // supported by MimeTypeMap is overridden to "application/octet-stream" when publishing.
        // To deal with this issue we set the mime type again after publishing.
        // See crbug.com/1010829 for more details.
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        String mimeType = null;
        Cursor cursor = null;
        try {
            cursor =
                    resolver.query(
                            Uri.parse(intermediateUri),
                            new String[] {MediaColumns.MIME_TYPE},
                            null,
                            null,
                            null);
            if (cursor != null && cursor.getCount() != 0 && cursor.moveToNext()) {
                mimeType = cursor.getString(cursor.getColumnIndexOrThrow(MediaColumns.MIME_TYPE));
            }
        } catch (Exception e) {
            Log.e(TAG, "Unable to get mimeType.", e);
        } finally {
            if (cursor != null) cursor.close();
        }

        PendingSession session = openPendingUri(intermediateUri);
        Uri publishedUri = session.publish();
        if (!TextUtils.isEmpty(mimeType)) {
            try {
                final ContentValues updateValues = new ContentValues();
                updateValues.put(MediaColumns.MIME_TYPE, mimeType);
                resolver.update(publishedUri, updateValues, null, null);
            } catch (Exception e) {
                Log.e(TAG, "Unable to modify mimeType.", e);
            }
        }
        return publishedUri.toString();
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
            ContentValues updateValues = new ContentValues();
            updateValues.put("date_expires", getNewExpirationTime());
            ContextUtils.getApplicationContext()
                    .getContentResolver()
                    .update(Uri.parse(intermediateUri), updateValues, null, null);
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "Cannot open intermediate Uri.", e);
        }
        return -1;
    }

    /**
     * Check if a download with the same name already exists.
     * @param fileName The name of the file to check.
     * @return whether a download with the file name exists.
     */
    @CalledByNative
    private static boolean fileNameExists(final String fileName) {
        return getDownloadUriForFileName(fileName) != null;
    }

    /**
     * Renames a download Uri with a display name.
     * @param downloadUri Uri of the download.
     * @param displayName New display name for the download.
     * @return whether rename was successful.
     */
    @CalledByNative
    private static boolean renameDownloadUri(final String downloadUri, final String displayName) {
        final ContentValues updateValues = new ContentValues();
        Uri uri = Uri.parse(downloadUri);
        updateValues.put(MediaColumns.DISPLAY_NAME, displayName);
        return ContextUtils.getApplicationContext()
                        .getContentResolver()
                        .update(uri, updateValues, null, null)
                == 1;
    }

    /**
     * Gets the display names for all downloads
     * @return an array of download Uri and display name pair.
     */
    @CalledByNative
    @RequiresApi(29)
    private static DisplayNameInfo[] getDisplayNamesForDownloads() {
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        Cursor cursor = null;
        try {
            Uri uri = Downloads.EXTERNAL_CONTENT_URI;
            cursor =
                    resolver.query(
                            MediaStore.setIncludePending(uri),
                            new String[] {BaseColumns._ID, MediaColumns.DISPLAY_NAME},
                            null,
                            null,
                            null);
            if (cursor == null || cursor.getCount() == 0) return null;
            List<DisplayNameInfo> infos = new ArrayList<DisplayNameInfo>();
            while (cursor.moveToNext()) {
                String displayName =
                        cursor.getString(cursor.getColumnIndexOrThrow(MediaColumns.DISPLAY_NAME));
                Uri downloadUri =
                        ContentUris.withAppendedId(
                                uri, cursor.getInt(cursor.getColumnIndexOrThrow(BaseColumns._ID)));
                infos.add(new DisplayNameInfo(downloadUri.toString(), displayName));
            }
            return infos.toArray(new DisplayNameInfo[0]);
        } catch (Exception e) {
            Log.e(TAG, "Unable to get display names for downloads.", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return null;
    }

    /** @return whether download collection is supported. */
    public static boolean supportsDownloadCollection() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    }

    /**
     * Gets the content URI of the download that has the given file name.
     * @param fileName name of the file.
     * @return Uri of the download with the given display name.
     */
    @RequiresApi(29)
    public static Uri getDownloadUriForFileName(String fileName) {
        Cursor cursor = null;
        try {
            Uri uri = Downloads.EXTERNAL_CONTENT_URI;
            cursor =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .query(
                                    MediaStore.setIncludePending(uri),
                                    new String[] {BaseColumns._ID},
                                    "_display_name LIKE ?1",
                                    new String[] {fileName},
                                    null);
            if (cursor == null) return null;
            if (cursor.moveToNext()) {
                return ContentUris.withAppendedId(
                        uri, cursor.getInt(cursor.getColumnIndexOrThrow(BaseColumns._ID)));
            }
        } catch (Exception e) {
            Log.e(TAG, "Unable to check file name existence.", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return null;
    }

    /** @return number of days for an intermediate download to expire. */
    public static int getExpirationDurationInDays() {
        return DownloadCollectionBridgeJni.get().getExpirationDurationInDays();
    }

    /**
     * Helper method to create a pending session for download to be written into.
     * @param fileName Name of the file.
     * @param mimeType Mime type of the file.
     * @param originalUrl Originating URL of the download.
     * @param referrer Referrer of the download.
     * @return Uri created for the pending session, or null if failed.
     */
    private static Uri createPendingSessionInternal(
            final String fileName,
            final String mimeType,
            final String originalUrl,
            final String referrer) {
        PendingParams pendingParams =
                createPendingParams(fileName, mimeType, originalUrl, referrer);
        pendingParams.setExpirationTime(getNewExpirationTime());
        try {
            return MediaStoreUtils.createPending(
                    ContextUtils.getApplicationContext(), pendingParams);
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Helper method to create PendingParams needed for PendingSession creation.
     * @param fileName Name of the file.
     * @param mimeType Mime type of the file.
     * @param originalUrl Originating URL of the download.
     * @param referrer Referrer of the download.
     * @return PendingParams needed for creating the PendingSession.
     */
    @RequiresApi(29)
    private static PendingParams createPendingParams(
            final String fileName,
            final String mimeType,
            final String originalUrl,
            final String referrer) {
        Uri downloadsUri = Downloads.EXTERNAL_CONTENT_URI;
        String newMimeType =
                sDownloadDelegate.remapGenericMimeType(mimeType, originalUrl, fileName);
        PendingParams pendingParams = new PendingParams(downloadsUri, fileName, newMimeType);
        Uri originalUri = sDownloadDelegate.parseOriginalUrl(originalUrl);
        Uri referrerUri = TextUtils.isEmpty(referrer) ? null : Uri.parse(referrer);
        pendingParams.setDownloadUri(originalUri);
        pendingParams.setRefererUri(referrerUri);
        return pendingParams;
    }

    /**
     *  Gets the base name, without extension, from a file name.
     *  TODO(qinmin): move this into a common utility class.
     *  @param fileName Name of the file.
     *  @return Base name of the file.
     */
    private static String getBaseName(final String fileName) {
        for (String extension : COMMON_DOUBLE_EXTENSIONS) {
            if (fileName.endsWith(extension)) {
                String name = fileName.substring(0, fileName.length() - extension.length());
                // remove the "." at the end.
                if (name.endsWith(".")) {
                    return name.substring(0, name.length() - 1);
                }
            }
        }
        int index = fileName.lastIndexOf('.');
        if (index == -1) {
            return fileName;
        } else {
            return fileName.substring(0, index);
        }
    }

    private static @NonNull PendingSession openPendingUri(final String pendingUri) {
        return MediaStoreUtils.openPending(
                ContextUtils.getApplicationContext(), Uri.parse(pendingUri));
    }

    /**
     * Helper method to generate a new expiration epoch time in seconds.
     * @return Epoch time value in seconds for the download to expire.
     */
    private static long getNewExpirationTime() {
        return (System.currentTimeMillis()
                        + DownloadCollectionBridge.getExpirationDurationInDays()
                                * DateUtils.DAY_IN_MILLIS)
                / 1000;
    }

    /**
     * Gets the display name for a download.
     * @param downloadUri Uri of the download.
     * @return the display name of the download.
     */
    @CalledByNative
    private static String getDisplayName(final String downloadUri) {
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        Cursor cursor = null;
        try {
            cursor =
                    resolver.query(
                            Uri.parse(downloadUri),
                            new String[] {MediaColumns.DISPLAY_NAME},
                            null,
                            null,
                            null);
            if (cursor == null || cursor.getCount() == 0) return null;
            if (cursor.moveToNext()) {
                return cursor.getString(cursor.getColumnIndexOrThrow(MediaColumns.DISPLAY_NAME));
            }
        } catch (Exception e) {
            Log.e(TAG, "Unable to get display name for download.", e);
        } finally {
            if (cursor != null) cursor.close();
        }
        return null;
    }

    @NativeMethods
    interface Natives {
        int getExpirationDurationInDays();
    }
}
