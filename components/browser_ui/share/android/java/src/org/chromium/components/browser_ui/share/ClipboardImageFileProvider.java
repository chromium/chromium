// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import static org.chromium.components.browser_ui.share.ClipboardConstants.CLIPBOARD_SHARED_URI;
import static org.chromium.components.browser_ui.share.ClipboardConstants.CLIPBOARD_SHARED_URI_TIMESTAMP;

import android.content.SharedPreferences;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.ui.base.Clipboard;

/** Implementation class for {@link Clipboard.ImageFileProvider}. */
public class ClipboardImageFileProvider implements Clipboard.ImageFileProvider {
    @Override
    public void storeImageAndGenerateUri(
            byte[] imageData, String fileExtension, Callback<Uri> callback) {
        ShareImageFileUtils.generateTemporaryUriFromData(imageData, fileExtension, callback);
    }

    @Override
    public void storeLastCopiedImageMetadata(@NonNull ClipboardFileMetadata clipboardFileMetadata) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(CLIPBOARD_SHARED_URI, clipboardFileMetadata.uri.toString())
                .putLong(CLIPBOARD_SHARED_URI_TIMESTAMP, clipboardFileMetadata.timestamp)
                .apply();
    }

    @Override
    public @Nullable ClipboardFileMetadata getLastCopiedImageMetadata() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            String uriString = prefs.getString(CLIPBOARD_SHARED_URI, null);
            if (TextUtils.isEmpty(uriString)) return null;

            Uri uri = Uri.parse(uriString);
            long timestamp = prefs.getLong(CLIPBOARD_SHARED_URI_TIMESTAMP, 0L);

            return new ClipboardFileMetadata(uri, timestamp);
        }
    }

    @Override
    public void clearLastCopiedImageMetadata() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(CLIPBOARD_SHARED_URI)
                .remove(CLIPBOARD_SHARED_URI_TIMESTAMP)
                .apply();
    }
}
