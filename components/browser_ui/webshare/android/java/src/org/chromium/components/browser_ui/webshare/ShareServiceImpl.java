// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.webshare;

import android.content.ComponentName;
import android.net.Uri;

import org.chromium.base.FileProviderUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.mojom.Url;
import org.chromium.webshare.mojom.ShareError;
import org.chromium.webshare.mojom.ShareService;
import org.chromium.webshare.mojom.SharedFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Random;
import java.util.Set;

/**
 * Android implementation of the ShareService service defined in
 * third_party/blink/public/mojom/webshare/webshare.mojom.
 */
@NullMarked
public class ShareServiceImpl implements ShareService {
    private final WebShareDelegate mDelegate;

    private static final String TAG = "share";

    // These numbers are written to histograms. Keep in sync with WebShareMethod enum in
    // histograms.xml, and don't reuse or renumber entries (except for the _COUNT entry).
    private static final int WEBSHARE_METHOD_SHARE = 0;
    // Count is technically 1, but recordEnumeratedHistogram requires a boundary of at least 2
    // (https://crbug.com/645032).
    private static final int WEBSHARE_METHOD_COUNT = 2;

    // These numbers are written to histograms. Keep in sync with WebShareOutcome enum in
    // histograms.xml, and don't reuse or renumber entries (except for the _COUNT entry).
    private static final int WEBSHARE_OUTCOME_SUCCESS = 0;
    private static final int WEBSHARE_OUTCOME_UNKNOWN_FAILURE = 1;
    private static final int WEBSHARE_OUTCOME_CANCELED = 2;
    private static final int WEBSHARE_OUTCOME_COUNT = 3;

    // These protect us if the renderer is compromised.
    private static final int MAX_SHARED_FILE_COUNT = 10;
    private static final int MAX_SHARED_FILE_BYTES = 50 * 1024 * 1024;

    // PERMITTED_EXTENSIONS and PERMITTED_MIME_TYPES should be kept in sync with
    // - //third_party/blink/renderer/modules/webshare/FILE_TYPES.md
    // - //chrome/browser/webshare/share_service_impl.cc

    private static final Set<String> PERMITTED_EXTENSIONS =
            Set.of(
                    "avif", "bmp", "css", "csv", "ehtml", "flac", "gif", "htm", "html", "ico",
                    "jfif", "jpeg", "jpg", "m4a", "m4v", "mp3", "mp4", "mpeg", "mpg", "oga", "ogg",
                    "ogm", "ogv", "opus", "pdf", "pjp", "pjpeg", "png", "shtm", "shtml", "svg",
                    "svgz", "text", "tif", "tiff", "txt", "wav", "weba", "webm", "webp", "xbm");

    private static final Set<String> PERMITTED_MIME_TYPES =
            Set.of(
                    "audio/flac",
                    "application/pdf",
                    "audio/mp3",
                    "audio/mpeg",
                    "audio/ogg",
                    "audio/wav",
                    "audio/webm",
                    "audio/x-m4a",
                    "image/avif",
                    "image/bmp",
                    "image/gif",
                    "image/jpeg",
                    "image/png",
                    "image/svg+xml",
                    "image/tiff",
                    "image/webp",
                    "image/x-icon",
                    "image/x-ms-bmp",
                    "image/x-xbitmap",
                    "text/comma-separated-values",
                    "text/css",
                    "text/csv",
                    "text/html",
                    "text/plain",
                    "video/mp4",
                    "video/mpeg",
                    "video/ogg",
                    "video/webm");

    private static final TaskRunner TASK_RUNNER =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);

    /** Delegate class that provides embedder-specific functionality. */
    public interface WebShareDelegate {
        /**
         * @return true if sharing is currently possible.
         */
        boolean canShare();

        /**
         * Overridden by the embedder to execute the share.
         *
         * @param params the share data.
         */
        void share(ShareParams params);

        /**
         * @return The current {@link WindowAndroid} used to perform sharing.
         */
        WindowAndroid getWindowAndroid();
    }

    public ShareServiceImpl(WebShareDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    @Override
    public void share(
            String title,
            String text,
            Url url,
            final SharedFile[] files,
            final Share_Response callback) {
        RecordHistogram.recordEnumeratedHistogram(
                "WebShare.ApiCount", WEBSHARE_METHOD_SHARE, WEBSHARE_METHOD_COUNT);

        if (!mDelegate.canShare()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "WebShare.ShareOutcome",
                    WEBSHARE_OUTCOME_UNKNOWN_FAILURE,
                    WEBSHARE_OUTCOME_COUNT);
            callback.call(ShareError.INTERNAL_ERROR);
            return;
        }

        ShareParams.TargetChosenCallback innerCallback =
                new ShareParams.TargetChosenCallback() {
                    @Override
                    public void onTargetChosen(@Nullable ComponentName chosenComponent) {
                        RecordHistogram.recordEnumeratedHistogram(
                                "WebShare.ShareOutcome",
                                WEBSHARE_OUTCOME_SUCCESS,
                                WEBSHARE_OUTCOME_COUNT);
                        callback.call(ShareError.OK);
                    }

                    @Override
                    public void onCancel() {
                        RecordHistogram.recordEnumeratedHistogram(
                                "WebShare.ShareOutcome",
                                WEBSHARE_OUTCOME_CANCELED,
                                WEBSHARE_OUTCOME_COUNT);
                        callback.call(ShareError.CANCELED);
                    }
                };

        final ShareParams.Builder paramsBuilder =
                new ShareParams.Builder(mDelegate.getWindowAndroid(), title, url.url)
                        .setText(text)
                        .setCallback(innerCallback);
        if (files == null || files.length == 0) {
            mDelegate.share(paramsBuilder.build());
            return;
        }

        if (files.length > MAX_SHARED_FILE_COUNT) {
            callback.call(ShareError.PERMISSION_DENIED);
            return;
        }

        for (SharedFile file : files) {
            if (isDangerousFilename(file.name.path.path)
                    || isDangerousMimeType(file.blob.contentType)) {
                Log.i(
                        TAG,
                        "Cannot share potentially dangerous \""
                                + file.blob.contentType
                                + "\" file \""
                                + file.name.path.path
                                + "\".");
                callback.call(ShareError.PERMISSION_DENIED);
                return;
            }
        }

        new AsyncTask<Boolean>() {
            @Override
            protected void onPostExecute(Boolean result) {
                if (!result) {
                    callback.call(ShareError.INTERNAL_ERROR);
                }
            }

            @Override
            // NullAway is failing due to the lambda.
            @NullUnmarked
            protected Boolean doInBackground() {
                ArrayList<Uri> fileUris = new ArrayList<>(files.length);
                ArrayList<BlobReceiver> blobReceivers = new ArrayList<>(files.length);
                try {
                    File sharePath = ShareImageFileUtils.getSharedFilesDirectory();

                    if (!sharePath.exists() && !sharePath.mkdir()) {
                        throw new IOException("Failed to create directory for shared file.");
                    }

                    // As multiple files may have the same name, we create a distinct
                    // subdirectory for each file.
                    // Oreo (API level 26) has Files.createTempDirectory(). We emulate it here by
                    // generating temp directories with random names.
                    Random rand = new Random();
                    for (SharedFile file : files) {
                        File tempDir;
                        File tempFile;
                        int attempts = 0;
                        do {
                            if (++attempts > 10) {
                                throw new IOException("Failed to create shared file.");
                            }
                            tempDir =
                                    new File(
                                            sharePath,
                                            "share" + Integer.toHexString(rand.nextInt(1 << 30)));
                            tempDir.mkdir();
                            tempFile = new File(tempDir, file.name.path.path);
                        } while (!tempFile.createNewFile());

                        fileUris.add(FileProviderUtils.getContentUriFromFile(tempFile));
                        blobReceivers.add(
                                new BlobReceiver(
                                        new FileOutputStream(tempFile), MAX_SHARED_FILE_BYTES));
                    }

                } catch (IOException ie) {
                    Log.w(TAG, "Error creating shared file", ie);
                    return false;
                }

                paramsBuilder.setFileContentType(SharedFileCollator.commonMimeType(files));
                paramsBuilder.setFileUris(fileUris);
                SharedFileCollator collator =
                        new SharedFileCollator(
                                files.length,
                                success -> {
                                    if (success) {
                                        mDelegate.share(paramsBuilder.build());
                                    } else {
                                        callback.call(ShareError.INTERNAL_ERROR);
                                    }
                                });

                for (int index = 0; index < files.length; ++index) {
                    blobReceivers.get(index).start(files[index].blob.blob, collator);
                }
                return true;
            }
        }.executeOnTaskRunner(TASK_RUNNER);
    }

    static boolean isDangerousFilename(String name) {
        // Reject filenames without a permitted extension.
        return name.indexOf('.') <= 0
                || !PERMITTED_EXTENSIONS.contains(FileUtils.getExtension(name));
    }

    static boolean isDangerousMimeType(String contentType) {
        return !PERMITTED_MIME_TYPES.contains(contentType);
    }
}
