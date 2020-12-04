// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.annotation.TargetApi;
import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.Clipboard;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/**
 * Utility class for file operations for image data.
 */
public class ShareImageFileUtils {
    private static final String TAG = "share";

    /**
     * Directory name for shared images.
     *
     * Named "screenshot" for historical reasons as we only initially shared screenshot images.
     * TODO(crbug.com/1055886): consider changing the directory name.
     */
    private static final String SHARE_IMAGES_DIRECTORY_NAME = "screenshot";
    private static final String JPEG_EXTENSION = ".jpg";
    private static final String FILE_NUMBER_FORMAT = " (%d)";
    private static final String MIME_TYPE = "image/JPEG";

    /**
     * Check if the file related to |fileUri| is in the |folder|.
     *
     * @param fileUri The {@link Uri} related to the file to be checked.
     * @param folder The folder that may contain the |fileUrl|.
     * @return Whether the |fileUri| is in the |folder|.
     */
    private static boolean isUriInDirectory(Uri fileUri, File folder) {
        if (fileUri == null) return false;

        Uri chromeUriPrefix = ContentUriUtils.getContentUriFromFile(folder);
        if (chromeUriPrefix == null) return false;

        return fileUri.toString().startsWith(chromeUriPrefix.toString());
    }

    /**
     * Check if the system clipboard contains a Uri that comes from Chrome. If yes, return the file
     * name from the Uri, otherwise return null.
     *
     * @return The file name if system clipboard contains a Uri from Chrome, otherwise return null.
     */
    private static String getClipboardCurrentFilepath() throws IOException {
        Uri clipboardUri = Clipboard.getInstance().getImageUri();
        if (isUriInDirectory(clipboardUri, getSharedFilesDirectory())) {
            return clipboardUri.getPath();
        }
        return null;
    }

    /**
     * Returns the directory where temporary files are stored to be shared with external
     * applications. These files are deleted on startup and when there are no longer any active
     * Activities.
     *
     * @return The directory where shared files are stored.
     */
    public static File getSharedFilesDirectory() throws IOException {
        File imagePath = UiUtils.getDirectoryForImageCapture(ContextUtils.getApplicationContext());
        return new File(imagePath, SHARE_IMAGES_DIRECTORY_NAME);
    }

    /**
     * Clears all shared image files.
     */
    public static void clearSharedImages() {
        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
            try {
                String clipboardFilepath = getClipboardCurrentFilepath();
                FileUtils.recursivelyDeleteFile(getSharedFilesDirectory(), (filepath) -> {
                    return filepath == null || clipboardFilepath == null
                            || !filepath.endsWith(clipboardFilepath);
                });
            } catch (IOException ie) {
                // Ignore exception.
            }
        });
    }

    /**
     * Temporarily saves the given set of image bytes and provides that URI to a callback for
     * sharing.
     *
     * @param imageData The image data to be shared in |fileExtension| format.
     * @param fileExtension File extension which |imageData| encoded to.
     * @param callback A provided callback function which will act on the generated URI.
     */
    public static void generateTemporaryUriFromData(
            final byte[] imageData, String fileExtension, Callback<Uri> callback) {
        if (imageData.length == 0) {
            Log.w(TAG, "Share failed -- Received image contains no data.");
            return;
        }
        OnImageSaveListener listener = new OnImageSaveListener() {
            @Override
            public void onImageSaved(Uri uri, String displayName) {
                callback.onResult(uri);
            }
            @Override
            public void onImageSaveError(String displayName) {}
        };

        String fileName = String.valueOf(System.currentTimeMillis());
        // Path is passed as a function because in some cases getting the path should be run on a
        // background thread.
        saveImage(fileName,
                ()
                        -> { return ""; },
                listener, (fos) -> { writeImageData(fos, imageData); }, true, fileExtension);
    }

    /**
     * Temporarily saves the bitmap and provides that URI to a callback for sharing.
     *
     * @param filename The filename without extension.
     * @param bitmap The Bitmap to download.
     * @param callback A provided callback function which will act on the generated URI.
     */
    public static void generateTemporaryUriFromBitmap(
            String fileName, Bitmap bitmap, Callback<Uri> callback) {
        OnImageSaveListener listener = new OnImageSaveListener() {
            @Override
            public void onImageSaved(Uri uri, String displayName) {
                callback.onResult(uri);
            }
            @Override
            public void onImageSaveError(String displayName) {}
        };

        FilePathProvider filePathProvider = () -> {
            return "";
        };
        FileOutputStreamWriter fileWriter = (fos) -> {
            writeBitmap(fos, bitmap);
        };

        saveImage(fileName, filePathProvider, listener, fileWriter, /*isTemporary=*/true,
                JPEG_EXTENSION);
    }

    /**
     * Saves bitmap to external storage directory.
     *
     * @param context The Context to use for determining download location.
     * @param filename The filename without extension.
     * @param bitmap The Bitmap to download.
     * @param listener The OnImageSaveListener to notify the download results.
     */
    public static void saveBitmapToExternalStorage(
            final Context context, String fileName, Bitmap bitmap, OnImageSaveListener listener) {
        // Passing the path as a function so that it can be called on a background thread in
        // |saveImage|.
        saveImage(fileName,
                ()
                        -> {
                    return context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS).getPath();
                },
                listener, (fos) -> { writeBitmap(fos, bitmap); }, false, JPEG_EXTENSION);
    }

    /**
     * Interface for notifying image download result.
     */
    public interface OnImageSaveListener {
        void onImageSaved(Uri uri, String displayName);
        void onImageSaveError(String displayName);
    }

    /**
     * Interface for writing image information to a output stream.
     */
    private interface FileOutputStreamWriter {
        void write(FileOutputStream fos) throws IOException;
    }

    /**
     * Interface for providing file path. This is used for passing a function for getting the path
     * to other function to be called while on a background thread. Should be used on a background
     * thread.
     */
    private interface FilePathProvider {
        String getPath();
    }

    /**
     * Saves image to the given file.
     *
     * @param fileName The File instance of a destination file.
     * @param filePathProvider The FilePathProvider for obtaining destination file path.
     * @param listener The OnImageSaveListener to notify the download results.
     * @param writer The FileOutputStreamWriter that writes to given stream.
     * @param isTemporary Indicates whether image should be save to a temporary file.
     * @param fileExtension The file's extension.
     */
    private static void saveImage(String fileName, FilePathProvider filePathProvider,
            OnImageSaveListener listener, FileOutputStreamWriter writer, boolean isTemporary,
            String fileExtension) {
        new AsyncTask<Uri>() {
            @Override
            protected Uri doInBackground() {
                FileOutputStream fOut = null;
                File destFile = null;
                try {
                    destFile = createFile(
                            fileName, filePathProvider.getPath(), isTemporary, fileExtension);
                    if (destFile != null && destFile.exists()) {
                        fOut = new FileOutputStream(destFile);
                        writer.write(fOut);
                    } else {
                        Log.w(TAG,
                                "Share failed -- Unable to create or write to destination file.");
                    }
                } catch (IOException ie) {
                    cancel(true);
                } finally {
                    StreamUtil.closeQuietly(fOut);
                }

                Uri uri = null;
                if (!isTemporary) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        uri = addToMediaStore(destFile);
                    } else {
                        long downloadId = addCompletedDownload(destFile);
                        DownloadManager manager =
                                (DownloadManager) ContextUtils.getApplicationContext()
                                        .getSystemService(Context.DOWNLOAD_SERVICE);
                        return manager.getUriForDownloadedFile(downloadId);
                    }
                } else {
                    uri = FileUtils.getUriForFile(destFile);
                }
                return uri;
            }

            @Override
            protected void onCancelled() {
                listener.onImageSaveError(fileName);
            }

            @Override
            protected void onPostExecute(Uri uri) {
                if (uri == null) {
                    listener.onImageSaveError(fileName);
                    return;
                }

                if (ApplicationStatus.getStateForApplication()
                        == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                    return;
                }

                listener.onImageSaved(uri, fileName);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Creates file with specified path, name and extension.
     *
     * @param filePath The file path a destination file.
     * @param fileName The file name a destination file.
     * @param isTemporary Indicates whether image should be save to a temporary file.
     * @param fileExtension The file's extension.
     *
     * @return The new File object.
     */
    private static File createFile(String fileName, String filePath, boolean isTemporary,
            String fileExtension) throws IOException {
        File path;
        if (filePath.isEmpty()) {
            path = getSharedFilesDirectory();
        } else {
            path = new File(filePath);
        }

        File newFile = null;
        if (path.exists() || path.mkdir()) {
            if (isTemporary) {
                newFile = File.createTempFile(fileName, fileExtension, path);
            } else {
                newFile = getNextAvailableFile(filePath, fileName, fileExtension);
            }
        }

        return newFile;
    }

    /**
     * Returns next available file for the given fileName.
     *
     * @param filePath The file path a destination file.
     * @param fileName The file name a destination file.
     * @param extension The extension a destination file.
     *
     * @return The new File object.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static File getNextAvailableFile(String filePath, String fileName, String extension)
            throws IOException {
        File destFile = new File(filePath, fileName + extension);
        int num = 0;
        while (destFile.exists()) {
            destFile = new File(filePath,
                    fileName + String.format(Locale.getDefault(), FILE_NUMBER_FORMAT, ++num)
                            + extension);
        }
        destFile.createNewFile();

        return destFile;
    }

    /**
     * Writes given bitmap to into the given fos.
     *
     * @param fos The FileOutputStream to write to.
     * @param bitmap The Bitmap to write.
     */
    private static void writeBitmap(FileOutputStream fos, Bitmap bitmap) throws IOException {
        bitmap.compress(Bitmap.CompressFormat.JPEG, 100, fos);
    }

    /**
     * Writes given data to into the given fos.
     *
     * @param fos The FileOutputStream to write to.
     * @param byte[] The byte[] to write.
     */
    private static void writeImageData(FileOutputStream fos, final byte[] data) throws IOException {
        fos.write(data);
    }

    /**
     * This is a pass through to the {@link AndroidDownloadManager} function of the same name.
     * @param file The File corresponding to the download.
     * @return the download ID of this item as assigned by the download manager.
     */
    public static long addCompletedDownload(File file) {
        String title = file.getName();
        String path = file.getPath();
        long length = file.length();

        return DownloadUtils.addCompletedDownload(
                title, title, MIME_TYPE, path, length, null, null);
    }

    @TargetApi(29)
    public static Uri addToMediaStore(File file) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;

        final ContentValues contentValues = new ContentValues();
        contentValues.put(MediaStore.MediaColumns.DISPLAY_NAME, file.getName());
        contentValues.put(MediaStore.MediaColumns.MIME_TYPE, MIME_TYPE);
        contentValues.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);

        ContentResolver database = ContextUtils.getApplicationContext().getContentResolver();
        Uri insertUri = database.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, contentValues);

        InputStream input = null;
        OutputStream output = null;
        try {
            input = new FileInputStream(file);
            if (insertUri != null) {
                output = database.openOutputStream(insertUri);
            }
            if (output != null) {
                byte[] buffer = new byte[4096];
                int byteCount = 0;
                while ((byteCount = input.read(buffer)) != -1) {
                    output.write(buffer, 0, byteCount);
                }
            }
            file.delete();
        } catch (IOException e) {
        } finally {
            StreamUtil.closeQuietly(input);
            StreamUtil.closeQuietly(output);
        }
        return insertUri;
    }

    /**
     * Captures a screenshot for the provided web contents, persists it and notifies the file
     * provider that the file is ready to be accessed by the client.
     *
     * The screenshot is compressed to JPEG before being written to the file.
     *
     * @param contents The WebContents instance for which to capture a screenshot.
     * @param width    The desired width of the resulting screenshot, or 0 for "auto."
     * @param height   The desired height of the resulting screenshot, or 0 for "auto."
     * @param callback The callback that will be called once the screenshot is saved.
     */
    public static void captureScreenshotForContents(
            WebContents contents, int width, int height, Callback<Uri> callback) {
        RenderWidgetHostView rwhv = contents.getRenderWidgetHostView();
        if (rwhv == null) {
            callback.onResult(null);
            return;
        }
        try {
            String path = UiUtils.getDirectoryForImageCapture(ContextUtils.getApplicationContext())
                    + File.separator + SHARE_IMAGES_DIRECTORY_NAME;
            rwhv.writeContentBitmapToDiskAsync(
                    width, height, path, new ExternallyVisibleUriCallback(callback));
        } catch (IOException e) {
            Log.e(TAG, "Error getting content bitmap: ", e);
            callback.onResult(null);
        }
    }

    private static class ExternallyVisibleUriCallback implements Callback<String> {
        private Callback<Uri> mComposedCallback;
        ExternallyVisibleUriCallback(Callback<Uri> cb) {
            mComposedCallback = cb;
        }

        @Override
        public void onResult(final String path) {
            if (TextUtils.isEmpty(path)) {
                mComposedCallback.onResult(null);
                return;
            }

            new AsyncTask<Uri>() {
                @Override
                protected Uri doInBackground() {
                    return ContentUriUtils.getContentUriFromFile(new File(path));
                }

                @Override
                protected void onPostExecute(Uri uri) {
                    mComposedCallback.onResult(uri);
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }
}
