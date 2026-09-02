// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/** Helper class for file operations on Android Storage Access Framework (SAF) Document URIs. */
@JNINamespace("download")
@NullMarked
public class DownloadDocumentUriBridge {
    private static final String TAG = "DownloadDocUri";

    /**
     * Checks whether the given URI is a Document URI.
     *
     * @param uriString URI string to check.
     * @return True if the URI is a Document URI, false otherwise.
     */
    @CalledByNative
    public static boolean isDocumentUri(@JniType("std::string") String uriString) {
        if (TextUtils.isEmpty(uriString)) return false;
        try {
            Context context = ContextUtils.getApplicationContext();
            if (context == null) return false;
            return DocumentsContract.isDocumentUri(context, Uri.parse(uriString));
        } catch (Exception e) {
            Log.w(TAG, "Failed to check if URI is a document URI: %s", uriString, e);
            return false;
        }
    }

    /**
     * Renames a document URI with a new display name.
     *
     * @param uriString Document URI string to rename.
     * @param newDisplayName New display name.
     * @return True if rename succeeded, false otherwise.
     */
    @CalledByNative
    public static boolean renameDocumentUri(
            @JniType("std::string") String uriString,
            @JniType("std::string") String newDisplayName) {
        if (TextUtils.isEmpty(uriString) || TextUtils.isEmpty(newDisplayName)) {
            return false;
        }
        try {
            Context context = ContextUtils.getApplicationContext();
            if (context == null) return false;
            ContentResolver resolver = context.getContentResolver();
            Uri uri = Uri.parse(uriString);
            Uri renamedUri = DocumentsContract.renameDocument(resolver, uri, newDisplayName);
            return renamedUri != null;
        } catch (Exception e) {
            Log.w(TAG, "Failed to rename document URI: %s to %s", uriString, newDisplayName, e);
            return false;
        }
    }
}
