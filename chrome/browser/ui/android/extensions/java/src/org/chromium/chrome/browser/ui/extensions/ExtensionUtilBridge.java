// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/** A JNI bridge to provide static utilities for extensions. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionUtilBridge {
    private static final Uri DOWNLOADS =
            MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);

    private ExtensionUtilBridge() {
        assert false;
    }

    /**
     * Get the file under Downloads by name.
     *
     * @param fileName the complete file name with extension, e.g., foo.crx
     * @return a content URI if the file exists; otherwise, the empty string
     */
    @CalledByNative
    static @JniType("std::string") String getFileUnderDownloads(
            @JniType("std::string") String fileName) {
        if (fileName.isEmpty()) return "";

        Context context = ContextUtils.getApplicationContext();
        ContentResolver resolver = context.getContentResolver();

        Uri uri = getMediaFileUriByName(resolver, fileName, DOWNLOADS);
        if (uri == null) {
            return "";
        }
        return uri.toString();
    }

    /**
     * Get files under Downloads. If they don't exist, create empty ones. The name of the given file
     * is concatenated to the extensions to determine the names of the files to be queried or
     * created. For example if the given file has the name "foo" and the extensions are ".crx" and
     * ".pem", the files will have names "foo.crx" and "foo.pem".
     *
     * @param contentUriForBasename the file whose name is used to determine the file names.
     * @param dotExtensions the extensions starting with a dot.
     * @return A list of content URIs. The list will be empty if any file query or creation failed.
     */
    @CalledByNative
    static @JniType("std::vector<std::string>") List<String> getOrCreateEmptyFilesUnderDownloads(
            @JniType("std::string") String contentUriForBasename,
            @JniType("std::vector<std::string>") List<String> dotExtensions) {
        String stem = ContentUriUtils.maybeGetDisplayName(contentUriForBasename);
        if (stem == null || stem.isEmpty()) return new ArrayList<>();

        Context context = ContextUtils.getApplicationContext();
        ContentResolver resolver = context.getContentResolver();

        List<Uri> contentUris = new ArrayList<>();
        for (String ext : dotExtensions) {
            String name = stem + ext;
            Uri uri = getOrCreateEmptyFileUnderDownloads(resolver, name);

            if (uri == null) {
                for (Uri u : contentUris) {
                    try {
                        resolver.delete(u, null, null);
                    } catch (Exception e) {
                        // The file was manually deleted concurrently.
                    }
                }
                // Query and creation not successful, return empty list.
                return new ArrayList<>();
            }

            contentUris.add(uri);
        }
        List<String> results = new ArrayList<>();
        for (Uri uri : contentUris) {
            results.add(uri.toString());
        }
        return results;
    }

    private static @Nullable Uri getOrCreateEmptyFileUnderDownloads(
            ContentResolver resolver, String name) {
        ContentValues contentValues = new ContentValues();

        Uri existedUri = getMediaFileUriByName(resolver, name, DOWNLOADS);
        if (existedUri != null) {
            return existedUri;
        }

        contentValues.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        contentValues.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);

        try {
            return resolver.insert(DOWNLOADS, contentValues);
        } catch (Exception e) {
            return null;
        }
    }

    private static @Nullable Uri getMediaFileUriByName(
            ContentResolver resolver, String name, Uri collection) {
        String[] projection =
                new String[] {MediaStore.MediaColumns._ID, MediaStore.MediaColumns.DISPLAY_NAME};
        String selection = MediaStore.MediaColumns.DISPLAY_NAME + " = ?";
        String[] selectionArgs = new String[] {name};

        try (Cursor cursor =
                resolver.query(collection, projection, selection, selectionArgs, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int idColumnIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns._ID);
                long id = cursor.getLong(idColumnIndex);
                return ContentUris.withAppendedId(collection, id);
            }
        } catch (Exception e) {
            // Failed to query for existing file URI
        }

        return null;
    }
}
