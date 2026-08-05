// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/** ContentProvider for incognito PDF file by taking a file path and returning a content URI. */
@NullMarked
public class PdfContentProvider extends ContentProvider {
    private static final String[] COLUMNS =
            new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
    private static final String TAG = "PdfProvider";
    private static final String URI_AUTHORITY_SUFFIX = ".PdfContentProvider";
    private static final String PDF_MIMETYPE = MimeTypeUtils.PDF_MIME_TYPE;

    static class PdfFileInfo {
        public final String tabId;
        public final String filePath;
        public final String fileName;
        public final ParcelFileDescriptor pfd;

        public PdfFileInfo(
                String tabId, String filePath, String fileName, ParcelFileDescriptor pfd) {
            this.tabId = tabId;
            this.filePath = filePath;
            this.fileName = fileName;
            this.pfd = pfd;
        }
    }

    // Map from unique ID to PdfFileInfo
    private static final Map<String, PdfFileInfo> sStreamRegistry =
            Collections.synchronizedMap(new HashMap<>());

    public PdfContentProvider() {}

    /**
     * Registers a stream for a given tab, extracting the file descriptor from the path. Reuses an
     * existing content URI if one is already registered for the given tab and path.
     *
     * @param tabId Unique identifier for the tab.
     * @param filePath Path to the PDF file (e.g. /proc/self/fd/...).
     * @param fileName Display name of the file.
     * @return A content Uri to access the file, or null if registration fails.
     */
    public static @Nullable Uri registerStream(String tabId, String filePath, String fileName) {
        Uri existingUri = getUriForStream(tabId, filePath);
        if (existingUri != null) {
            Log.d(
                    TAG,
                    "Stream already registered for Tab: %s, Path: %s, reusing it.",
                    tabId,
                    filePath);
            return existingUri;
        }
        int fd = extractFd(filePath);
        ParcelFileDescriptor pfd = null;
        try {
            if (fd != -1) {
                pfd = ParcelFileDescriptor.fromFd(fd);
            } else if (filePath != null) {
                pfd =
                        ParcelFileDescriptor.open(
                                new File(filePath), ParcelFileDescriptor.MODE_READ_ONLY);
            }
            if (pfd != null) {
                return createContentUri(tabId, filePath, pfd, fileName);
            }
        } catch (IOException
                | SecurityException
                | IllegalArgumentException
                | IllegalStateException e) {
            Log.e(TAG, "Failed to open ParcelFileDescriptor for path: " + filePath, e);
        }
        return null;
    }

    private static int extractFd(@Nullable String filePath) {
        if (filePath == null) return -1;
        if (!filePath.startsWith("/proc/")) {
            Log.e(TAG, "File path may not contain a valid file descriptor: " + filePath);
            return -1;
        }
        String fd = filePath.substring(filePath.lastIndexOf('/') + 1);
        try {
            return Integer.parseInt(fd);
        } catch (NumberFormatException ex) {
            Log.e(TAG, "File path is invalid: " + filePath, ex);
        }
        return -1;
    }

    /**
     * Creates a content URI for a given file descriptor.
     *
     * @param tabId Unique identifier for the tab.
     * @param filePath Path to the PDF file.
     * @param pfd The ParcelFileDescriptor of the PDF.
     * @param fileName Display name of the file.
     * @return A content Uri to access the file.
     */
    public static @Nullable Uri createContentUri(
            String tabId, String filePath, ParcelFileDescriptor pfd, String fileName) {
        removeStreamsForTab(tabId);

        String streamId = UUID.randomUUID().toString();
        PdfFileInfo info = new PdfFileInfo(tabId, filePath, fileName, pfd);
        sStreamRegistry.put(streamId, info);
        return getUriForUniqueId(streamId);
    }

    /**
     * Removes a content Uri so that it is no longer valid for future access.
     *
     * @param streamId Unique stream identifier or URI string to be removed.
     */
    public static void removeContentUri(@Nullable String streamId) {
        if (streamId == null) {
            return;
        }
        String id = streamId;
        if (streamId.startsWith(UrlConstants.CONTENT_URL_PREFIX)) {
            Uri uri = Uri.parse(streamId);
            id = uri.getLastPathSegment();
        }

        PdfFileInfo info = sStreamRegistry.remove(id);
        if (info != null) {
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> {
                        StreamUtil.closeQuietly(info.pfd);
                    });
        }
    }

    /**
     * Removes all streams associated with a tab.
     *
     * @param tabId Unique identifier for the tab.
     */
    public static void removeStreamsForTab(String tabId) {
        synchronized (sStreamRegistry) {
            List<String> keysToRemove = new ArrayList<>();
            for (Map.Entry<String, PdfFileInfo> entry : sStreamRegistry.entrySet()) {
                if (entry.getValue().tabId.equals(tabId)) {
                    keysToRemove.add(entry.getKey());
                }
            }
            for (String key : keysToRemove) {
                PdfFileInfo info = sStreamRegistry.remove(key);
                if (info != null) {
                    PostTask.postTask(
                            TaskTraits.BEST_EFFORT_MAY_BLOCK,
                            () -> {
                                StreamUtil.closeQuietly(info.pfd);
                            });
                }
            }
        }
    }

    /**
     * Gets the content URI for a registered stream that matches the tab ID and file path.
     *
     * @param tabId Unique identifier for the tab.
     * @param filePath Path to the PDF file.
     * @return The content URI, or null if not found.
     */
    public static @Nullable Uri getUriForStream(String tabId, String filePath) {
        synchronized (sStreamRegistry) {
            for (Map.Entry<String, PdfFileInfo> entry : sStreamRegistry.entrySet()) {
                PdfFileInfo info = entry.getValue();
                if (info.tabId.equals(tabId) && info.filePath.equals(filePath)) {
                    try (ParcelFileDescriptor validationDup = info.pfd.dup()) {
                        if (validationDup != null && validationDup.getFileDescriptor().valid()) {
                            return getUriForUniqueId(entry.getKey());
                        }
                    } catch (IOException e) {
                        Log.w(
                                TAG,
                                "Registered stream FD is no longer valid for Tab: %s, removing"
                                        + " stale entry.",
                                tabId);
                    }
                    sStreamRegistry.remove(entry.getKey());
                    PostTask.postTask(
                            TaskTraits.BEST_EFFORT_MAY_BLOCK,
                            () -> {
                                StreamUtil.closeQuietly(info.pfd);
                            });
                    break;
                }
            }
        }
        return null;
    }

    /**
     * Helper to reconstruct the content URI for a given unique ID.
     *
     * @param uniqueId Unique identifier.
     * @return The content URI.
     */
    private static Uri getUriForUniqueId(String uniqueId) {
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_CONTENT)
                .authority(
                        ContextUtils.getApplicationContext().getPackageName()
                                + URI_AUTHORITY_SUFFIX)
                .path(uniqueId)
                .build();
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    /**
     * @see ContentProvider#getType(Uri)
     */
    @Override
    public @Nullable String getType(Uri uri) {
        if (uri == null) return null;
        String uniqueId = uri.getLastPathSegment();
        if (uniqueId == null || !sStreamRegistry.containsKey(uniqueId)) {
            return null;
        }
        return PDF_MIMETYPE;
    }

    /**
     * @see ContentProvider#getStreamTypes(Uri, String)
     */
    @Override
    public String @Nullable [] getStreamTypes(Uri uri, String mimeTypeFilter) {
        if (uri == null) return null;
        String uniqueId = uri.getLastPathSegment();
        if (uniqueId == null || !sStreamRegistry.containsKey(uniqueId)) {
            return null;
        }

        if (matchMimeTypeFilter(mimeTypeFilter)) {
            return new String[] {PDF_MIMETYPE};
        } else {
            return null;
        }
    }

    /**
     * @see ContentProvider#openFile(Uri, String)
     */
    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (uri == null) {
            throw new FileNotFoundException("Cannot open an empty Uri.");
        }

        String uniqueId = uri.getLastPathSegment();
        if (uniqueId == null) {
            throw new FileNotFoundException("Invalid URI: no path segment.");
        }

        PdfFileInfo info = sStreamRegistry.get(uniqueId);
        if (info != null) {
            try {
                // Duplicate so each caller owns an independent descriptor; closing one
                // does not invalidate descriptors held by other callers.
                return info.pfd.dup();
            } catch (IOException e) {
                throw new FileNotFoundException(
                        "Failed to duplicate file descriptor: " + e.getMessage());
            }
        }
        throw new FileNotFoundException(
                "The requested Incognito PDF stream has expired or does not exist.");
    }

    /**
     * @see ContentProvider#query(Uri, String[], String, String[], String)
     */
    @Override
    public Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder) {
        if (uri == null) return new MatrixCursor(COLUMNS, 0);
        String uniqueId = uri.getLastPathSegment();
        if (uniqueId == null || !sStreamRegistry.containsKey(uniqueId)) {
            return new MatrixCursor(COLUMNS, 0);
        }
        PdfFileInfo info = sStreamRegistry.get(uniqueId);
        if (info == null) {
            return new MatrixCursor(COLUMNS, 0);
        }
        long fileSize = info.pfd.getStatSize();
        String fileName = info.fileName;

        if (projection == null) {
            projection = COLUMNS;
        }

        boolean hasDisplayName = false;
        boolean hasSize = false;
        int length = 0;
        for (String col : projection) {
            if (OpenableColumns.DISPLAY_NAME.equals(col)) {
                hasDisplayName = true;
                length++;
            } else if (OpenableColumns.SIZE.equals(col)) {
                hasSize = true;
                length++;
            }
        }

        String[] cols = new String[length];
        Object[] values = new Object[length];
        int index = 0;
        if (hasDisplayName) {
            cols[index] = OpenableColumns.DISPLAY_NAME;
            values[index] = fileName;
            index++;
        }
        if (hasSize) {
            cols[index] = OpenableColumns.SIZE;
            values[index] = fileSize;
        }
        MatrixCursor cursor = new MatrixCursor(cols, 1);
        cursor.addRow(values);
        return cursor;
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues values,
            @Nullable String where,
            String @Nullable [] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, @Nullable String selection, String @Nullable [] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, @Nullable ContentValues values) {
        throw new UnsupportedOperationException();
    }

    private static boolean matchMimeTypeFilter(String mimeTypeFilter) {
        if (mimeTypeFilter == null) {
            return false;
        }

        // Check for exact match
        if (mimeTypeFilter.equals(PDF_MIMETYPE)) {
            return true;
        }

        // Check for wildcard matches (*/pdf, application/*, */*)
        if (mimeTypeFilter.endsWith("/pdf") || mimeTypeFilter.endsWith("/*")) {
            int idx = mimeTypeFilter.indexOf('/');
            String baseType = mimeTypeFilter.substring(0, idx);

            // Handle */* case
            if (baseType.equals("*") || baseType.equals("application")) {
                return true;
            }
        }
        return false;
    }

    static void setPdfFileInfoForTesting(Uri uri, PdfFileInfo pdfFileInfo) {
        String uniqueId = uri.getLastPathSegment();
        if (uniqueId != null) {
            sStreamRegistry.put(uniqueId, pdfFileInfo);
        }
    }

    static void cleanUpForTesting() {
        List<String> keys = new ArrayList<>(sStreamRegistry.keySet());
        for (String key : keys) {
            removeContentUri(key);
        }
        assert sStreamRegistry.isEmpty();
    }
}
