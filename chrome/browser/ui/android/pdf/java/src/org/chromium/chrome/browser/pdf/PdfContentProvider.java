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
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/** ContentProvider for incognito PDF file by taking a file path and returning a content URI. */
public class PdfContentProvider extends ContentProvider {
    private static final String[] COLUMNS =
            new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
    private static final String TAG = "PdfProvider";
    private static final String URI_AUTHORITY_SUFFIX = ".PdfContentProvider";
    private static final String PDF_FILE_PREFIX = "/proc/";
    private static final Object LOCK = new Object();
    private static final String PDF_MIMETYPE = "application/pdf";

    static class PdfFileInfo {
        public final String filePath;
        public final String fileName;
        public final ParcelFileDescriptor pfd;

        public PdfFileInfo(String filePath, String fileName, ParcelFileDescriptor pfd) {
            this.filePath = filePath;
            this.fileName = fileName;
            this.pfd = pfd;
        }
    }

    // Map from content URI to PdfFileInfo
    private static final Map<Uri, PdfFileInfo> sPdfUriMap = new HashMap<>();

    public PdfContentProvider() {}

    /**
     * Creates a content URI for a given file.
     *
     * @param filePath Path to the file.
     * @param fileName Display name of the file.
     * @return A content Uri to access the file by other apps.
     */
    public static Uri createContentUri(String filePath, String fileName) {
        synchronized (LOCK) {
            for (Map.Entry<Uri, PdfFileInfo> entry : sPdfUriMap.entrySet()) {
                PdfFileInfo info = entry.getValue();
                if (TextUtils.equals(filePath, info.filePath)
                        && TextUtils.equals(fileName, info.fileName)) {
                    return entry.getKey();
                }
            }

            PdfFileInfo info = getPdfFileInfo(filePath, fileName);
            if (info != null) {
                Uri uri =
                        new Uri.Builder()
                                .scheme(ContentResolver.SCHEME_CONTENT)
                                .authority(
                                        ContextUtils.getApplicationContext().getPackageName()
                                                + URI_AUTHORITY_SUFFIX)
                                .path(String.valueOf(System.currentTimeMillis()))
                                .build();
                sPdfUriMap.put(uri, info);
                return uri;
            }
            return null;
        }
    }

    /**
     * Removes a content Uri so that it is no longer valid for future access.
     *
     * @param uri Uri to be removed.
     */
    public static void removeContentUri(String uri) {
        if (uri == null) {
            return;
        }

        try {
            Uri contentUri = Uri.parse(uri);
            synchronized (LOCK) {
                PdfFileInfo info = sPdfUriMap.remove(contentUri);
                if (info != null) {
                    try {
                        info.pfd.close();
                    } catch (IOException ex) {
                        Log.e(TAG, "Unable to close file.", ex);
                    }
                }
            }
        } catch (Exception ex) {
            Log.e(TAG, "Cannot parse uri.", ex);
            return;
        }
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    /**
     * @see ContentProvider#getType(Uri)
     */
    @Override
    public String getType(Uri uri) {
        synchronized (LOCK) {
            if (uri == null || !sPdfUriMap.containsKey(uri)) {
                return null;
            }
            return PDF_MIMETYPE;
        }
    }

    /**
     * @see ContentProvider#getStreamTypes(Uri, String)
     */
    @Override
    public String[] getStreamTypes(Uri uri, String mimeTypeFilter) {
        synchronized (LOCK) {
            if (uri == null || !sPdfUriMap.containsKey(uri)) {
                return null;
            }
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

        synchronized (LOCK) {
            PdfFileInfo info = sPdfUriMap.get(uri);
            if (info != null) {
                return info.pfd;
            }
        }
        throw new FileNotFoundException("Uri has expired or doesn't exist.");
    }

    /**
     * @see ContentProvider#query(Uri, String[], String, String[], String)
     */
    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        String fileName;
        long fileSize = 0;
        synchronized (LOCK) {
            if (uri == null || !sPdfUriMap.containsKey(uri)) {
                return new MatrixCursor(COLUMNS, 0);
            }
            PdfFileInfo info = sPdfUriMap.get(uri);
            fileSize = info.pfd.getStatSize();
            fileName = info.fileName;
        }
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
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    private static PdfFileInfo getPdfFileInfo(String filePath, String fileName) {
        if (!filePath.startsWith(PDF_FILE_PREFIX)) {
            Log.e(TAG, "File path may not contain a valid file descriptor.");
        } else {
            String fd = filePath.substring(filePath.lastIndexOf('/') + 1);
            try {
                int intFd = Integer.parseInt(fd);
                return new PdfFileInfo(filePath, fileName, ParcelFileDescriptor.adoptFd(intFd));
            } catch (NumberFormatException ex) {
                Log.e(TAG, "File path is invalid.", ex);
            }
        }
        return null;
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
        synchronized (LOCK) {
            sPdfUriMap.put(uri, pdfFileInfo);
        }
    }

    static void cleanUpForTesting() {
        synchronized (LOCK) {
            sPdfUriMap.clear();
        }
    }
}
