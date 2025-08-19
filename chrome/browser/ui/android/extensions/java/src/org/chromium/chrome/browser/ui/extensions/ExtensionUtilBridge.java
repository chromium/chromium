// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
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
    private ExtensionUtilBridge() {
        assert false;
    }

    /**
     * Creates empty files under Downloads. The name of the given file is concatenated to the
     * extensions to determine the names of the files to be created. For example if the given file
     * has the name "foo" and the extensions are ".crx" and ".pem", the created files will have
     * names "foo.crx" and "foo.pem".
     *
     * @param contentUriForBasename the file whose name is used to determine the created file names.
     * @param dotExtensions the extensions starting with a dot.
     * @return List of created content URIs, or empty if creation of any file was unsuccessful.
     */
    @CalledByNative
    static @Nullable @JniType("std::vector<std::string>") List<String>
            createEmptyFilesUnderDownloads(
                    @JniType("std::string") String contentUriForBasename,
                    @JniType("std::vector<std::string>") List<String> dotExtensions) {
        String stem = ContentUriUtils.maybeGetDisplayName(contentUriForBasename);
        if (stem == null || stem.isEmpty()) return null;

        Context context = ContextUtils.getApplicationContext();
        ContentResolver resolver = context.getContentResolver();

        List<Uri> contentUris = new ArrayList<>();
        for (String ext : dotExtensions) {
            String name = stem + ext;
            Uri uri = createEmptyFileUnderDownloads(resolver, name);

            if (uri == null) {
                for (Uri u : contentUris) {
                    try {
                        resolver.delete(u, null, null);
                    } catch (Exception e) {
                        // The file was manually deleted concurrently.
                    }
                }
                return null;
            }

            contentUris.add(uri);
        }
        List<String> results = new ArrayList<>();
        for (Uri uri : contentUris) {
            results.add(uri.toString());
        }
        return results;
    }

    private static @Nullable Uri createEmptyFileUnderDownloads(
            ContentResolver resolver, String name) {
        ContentValues contentValues = new ContentValues();
        contentValues.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        contentValues.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);

        Uri collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
        try {
            return resolver.insert(collection, contentValues);
        } catch (Exception e) {
            return null;
        }
    }
}
