// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.database.SQLException;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileTime;

/**
 * Helper class to fetch PDF document properties in a background thread.
 */
@NullMarked
class PdfDocumentPropertiesFetcher {
    private static final String TAG = "PdfDocPropsFetcher";

    static class DocProperties {
        String mFileName = "";
        long mFileSize;
        long mLastModified;
        long mCreationTime;
    }

    static DocProperties getDocProperties(
            Context appContext,
            @Nullable Uri uri,
            String title,
            @Nullable String pdfFilePath) {

        DocProperties props = new DocProperties();
        props.mFileName = title;

        if (uri == null) return props;

        if (ContentResolver.SCHEME_CONTENT.equals(uri.getScheme())) {
            ContentResolver cr = appContext.getContentResolver();
            // Combined projection: query displayName, size, and lastModified in a single query.
            String[] projection = {
                OpenableColumns.DISPLAY_NAME,
                OpenableColumns.SIZE,
                DocumentsContract.Document.COLUMN_LAST_MODIFIED
            };
            try (Cursor cursor = cr.query(uri, projection, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
                    int modIndex =
                            cursor.getColumnIndex(DocumentsContract.Document.COLUMN_LAST_MODIFIED);

                    if (nameIndex != -1) {
                        String displayName = cursor.getString(nameIndex);
                        if (displayName != null) {
                            props.mFileName = displayName;
                        }
                    }
                    if (sizeIndex != -1) {
                        props.mFileSize = cursor.getLong(sizeIndex);
                    }
                    if (modIndex != -1) {
                        props.mLastModified = cursor.getLong(modIndex);
                    }
                }
            } catch (SecurityException
                    | IllegalArgumentException
                    | NullPointerException
                    | IllegalStateException
                    | SQLException e) {
                Log.w(
                        TAG,
                        "Failed to query content URI properties in a single query, attempting"
                            + " separate queries",
                        e);
                // Fallback: If combined query failed, query displayName and size.
                String[] fallbackProjection = {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
                try (Cursor cursor = cr.query(uri, fallbackProjection, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst()) {
                        int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                        int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
                        if (nameIndex != -1) {
                            String displayName = cursor.getString(nameIndex);
                            if (displayName != null) {
                                props.mFileName = displayName;
                            }
                        }
                        if (sizeIndex != -1) {
                            props.mFileSize = cursor.getLong(sizeIndex);
                        }
                    }
                } catch (SecurityException
                        | IllegalArgumentException
                        | NullPointerException
                        | IllegalStateException
                        | SQLException ex) {
                    Log.w(TAG, "Failed to query OpenableColumns", ex);
                }

                // Query last modified separately
                String[] docProjection = {DocumentsContract.Document.COLUMN_LAST_MODIFIED};
                try (Cursor cursor = cr.query(uri, docProjection, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst()) {
                        int modIndex =
                                cursor.getColumnIndex(
                                        DocumentsContract.Document.COLUMN_LAST_MODIFIED);
                        if (modIndex != -1) {
                            props.mLastModified = cursor.getLong(modIndex);
                        }
                    }
                } catch (SecurityException
                        | IllegalArgumentException
                        | NullPointerException
                        | IllegalStateException
                        | SQLException ex) {
                    // Ignore
                }
            }
        } else if (ContentResolver.SCHEME_FILE.equals(uri.getScheme())) {
            String path = uri.getPath();
            if (path != null) {
                File file = new File(path);
                props.mFileName = file.getName();
                props.mFileSize = file.length();
                props.mLastModified = file.lastModified();
                props.mCreationTime = getFileCreationTime(file.getAbsolutePath());
            }
        }

        // Handle pdfFilePath fallback.
        // Make sure it is not a URI string. If it starts with file://, parse it. If it starts with
        // content://, ignore it.
        if (pdfFilePath != null) {
            String path = null;
            if (pdfFilePath.startsWith("file://")) {
                path = Uri.parse(pdfFilePath).getPath();
            } else if (!pdfFilePath.startsWith("content://")) {
                path = pdfFilePath;
            }

            if (path != null) {
                File file = new File(path);
                if (props.mFileSize <= 0) {
                    props.mFileSize = file.length();
                }
                if (props.mLastModified <= 0) {
                    props.mLastModified = file.lastModified();
                }
                if (props.mCreationTime <= 0) {
                    props.mCreationTime = getFileCreationTime(file.getAbsolutePath());
                }
                if (TextUtils.isEmpty(props.mFileName) || props.mFileName.equals(title)) {
                    props.mFileName = file.getName();
                }
            }
        }

        return props;
    }

    private static long getFileCreationTime(String path) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                BasicFileAttributes attrs =
                        Files.readAttributes(Paths.get(path), BasicFileAttributes.class);
                FileTime time = attrs.creationTime();
                return time.toMillis();
            } catch (IOException | SecurityException e) {
                Log.w(TAG, "Failed to get file creation time", e);
            }
        }
        return 0;
    }
}
