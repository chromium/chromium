// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;

import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Client for launching the Drive file picker and obtaining Drive file metadata. */
@NullMarked
public class DriveFilePickerClient {
    private static @Nullable DriveFilePickerClient sInstance;

    /** Returns the singleton instance of {@link DriveFilePickerClient}. */
    public static DriveFilePickerClient getInstance() {
        if (sInstance == null) {
            DriveFilePickerClient downstream =
                    ServiceLoaderUtil.maybeCreate(DriveFilePickerClient.class);
            sInstance = downstream != null ? downstream : new DriveFilePickerClient();
        }
        return sInstance;
    }

    /** Sets the instance for testing. */
    public static void setInstanceForTesting(@Nullable DriveFilePickerClient instance) {
        var prev = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = prev);
    }

    /**
     * Checks whether Google Drive file picker is available. Overridden by downstream implementation
     * in //clank.
     */
    public boolean isAvailable(Context context) {
        return false;
    }

    /**
     * Launches the Drive file picker asynchronously. Overridden by downstream implementation in
     * //clank.
     *
     * @param windowAndroid Window in which to display the picker.
     * @param mimeTypes Optional list of MIME type filters.
     * @return A promise resolved with {@link DriveAttachmentMetadata} or null if canceled.
     */
    public Promise<@Nullable DriveAttachmentMetadata> launchPicker(
            WindowAndroid windowAndroid, @Nullable List<String> mimeTypes) {
        return Promise.<@Nullable DriveAttachmentMetadata>fulfilled(null);
    }
}
