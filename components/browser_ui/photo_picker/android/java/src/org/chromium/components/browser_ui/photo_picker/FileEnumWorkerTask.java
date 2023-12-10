// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.Manifest;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.net.MimeTypeFilter;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A worker task to enumerate image files on disk. */
class FileEnumWorkerTask extends AsyncTask<List<PickerBitmap>> {
    // A tag for logging error messages.
    private static final String TAG = "PhotoPicker";

    /** An interface to use to communicate back the results to the client. */
    public interface FilesEnumeratedCallback {
        /**
         * A callback to define to receive the list of all images on disk.
         *
         * @param files The list of images, or null if the function fails.
         */
        void filesEnumeratedCallback(List<PickerBitmap> files);
    }

    private final WindowAndroid mWindowAndroid;

    // The callback to use to communicate the results.
    private FilesEnumeratedCallback mCallback;

    // The filter to apply to the list.
    private MimeTypeFilter mFilter;

    // Whether any image MIME types were requested.
    private boolean mIncludeImages;

    // Whether any video MIME types were requested.
    private boolean mIncludeVideos;

    // The ContentResolver to use to retrieve image metadata from disk.
    private ContentResolver mContentResolver;

    // The camera directory under DCIM.
    private static final String SAMPLE_DCIM_SOURCE_SUB_DIRECTORY = "Camera";

    /**
     * A FileEnumWorkerTask constructor.
     *
     * @param windowAndroid The window wrapper associated with the current activity.
     * @param callback The callback to use to communicate back the results.
     * @param filter The file filter to apply to the list.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     */
    public FileEnumWorkerTask(
            WindowAndroid windowAndroid,
            FilesEnumeratedCallback callback,
            MimeTypeFilter filter,
            List<String> mimeTypes,
            ContentResolver contentResolver) {
        mWindowAndroid = windowAndroid;
        mCallback = callback;
        mFilter = filter;
        mContentResolver = contentResolver;

        for (String mimeType : mimeTypes) {
            if (mimeType.startsWith("image/")) {
                mIncludeImages = true;
            } else if (mimeType.startsWith("video/")) {
                mIncludeVideos = true;
            }

            if (mIncludeImages && mIncludeVideos) break;
        }
    }

    /** Retrieves the DCIM/camera directory. */
    private String getCameraDirectory() {
        return Environment.DIRECTORY_DCIM + File.separator + SAMPLE_DCIM_SOURCE_SUB_DIRECTORY;
    }

    /**
     * Enumerates (in the background) the image files on disk. Called on a non-UI thread
     *
     * @return A sorted list of images (by last-modified first).
     */
    @Override
    protected List<PickerBitmap> doInBackground() {
        ThreadUtils.assertOnBackgroundThread();

        if (isCancelled()) return null;

        List<PickerBitmap> pickerBitmaps = new ArrayList<>();

        // The DATA column is deprecated in the Android Q SDK. Replaced by relative_path.
        String directoryColumnName =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                        ? "relative_path"
                        : MediaStore.Files.FileColumns.DATA;
        final String[] selectColumns = {
            MediaStore.Files.FileColumns._ID,
            MediaStore.Files.FileColumns.DATE_ADDED,
            MediaStore.Files.FileColumns.MEDIA_TYPE,
            MediaStore.Files.FileColumns.MIME_TYPE,
            directoryColumnName,
        };

        String whereClause =
                directoryColumnName
                        + " LIKE ? OR "
                        + directoryColumnName
                        + " LIKE ? OR "
                        + directoryColumnName
                        + " LIKE ? OR "
                        + directoryColumnName
                        + " LIKE ? OR "
                        + directoryColumnName
                        + " LIKE ? OR "
                        + directoryColumnName
                        + " LIKE ?";
        String additionalClause = "";
        if (mIncludeImages) {
            additionalClause =
                    MediaStore.Files.FileColumns.MEDIA_TYPE
                            + "="
                            + MediaStore.Files.FileColumns.MEDIA_TYPE_IMAGE;
        }
        if (mIncludeVideos) {
            if (mIncludeImages) additionalClause += " OR ";
            additionalClause +=
                    MediaStore.Files.FileColumns.MEDIA_TYPE
                            + "="
                            + MediaStore.Files.FileColumns.MEDIA_TYPE_VIDEO;
        }
        if (!additionalClause.isEmpty()) whereClause += " AND (" + additionalClause + ")";

        String cameraDir = getCameraDirectory();
        String picturesDir = Environment.DIRECTORY_PICTURES;
        String moviesDir = Environment.DIRECTORY_MOVIES;
        String downloadsDir = Environment.DIRECTORY_DOWNLOADS;
        // Files downloaded from the user's Google Photos library go to a Restored folder.
        String restoredDir = Environment.DIRECTORY_DCIM + "/Restored";
        // On some devices, such as Samsung and Redmi, the Screenshots folder is located under
        // DCIM/Screenshots, as opposed to DCIM/Pictures/Screenshots.
        String screenshotsDir = Environment.DIRECTORY_DCIM + "/Screenshots";
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            cameraDir = Environment.getExternalStoragePublicDirectory(cameraDir).toString();
            picturesDir = Environment.getExternalStoragePublicDirectory(picturesDir).toString();
            moviesDir = Environment.getExternalStoragePublicDirectory(moviesDir).toString();
            downloadsDir = Environment.getExternalStoragePublicDirectory(downloadsDir).toString();
            restoredDir = Environment.getExternalStoragePublicDirectory(restoredDir).toString();
            screenshotsDir =
                    Environment.getExternalStoragePublicDirectory(screenshotsDir).toString();
        }

        String[] whereArgs =
                new String[] {
                    // Include:
                    cameraDir + "%",
                    picturesDir + "%",
                    moviesDir + "%",
                    downloadsDir + "%",
                    restoredDir + "%",
                    screenshotsDir + "%",
                };

        final String orderBy = MediaStore.MediaColumns.DATE_ADDED + " DESC";

        Uri contentUri = MediaStore.Files.getContentUri("external");
        Cursor imageCursor =
                createImageCursor(contentUri, selectColumns, whereClause, whereArgs, orderBy);
        if (imageCursor == null) {
            Log.e(TAG, "Content Resolver query() returned null");
            return null;
        }

        Log.i(
                TAG,
                "Found "
                        + imageCursor.getCount()
                        + " media files, when requesting columns: "
                        + Arrays.toString(selectColumns)
                        + ", with WHERE "
                        + whereClause
                        + ", params: "
                        + Arrays.toString(whereArgs));

        while (imageCursor.moveToNext()) {
            int mimeTypeIndex = imageCursor.getColumnIndex(MediaStore.Files.FileColumns.MIME_TYPE);
            String mimeType = imageCursor.getString(mimeTypeIndex);
            if (!mFilter.accept(null, mimeType)) continue;

            int dateTakenIndex =
                    imageCursor.getColumnIndex(MediaStore.Files.FileColumns.DATE_ADDED);
            int idIndex = imageCursor.getColumnIndex(MediaStore.Files.FileColumns._ID);
            Uri uri = ContentUris.withAppendedId(contentUri, imageCursor.getInt(idIndex));
            long dateTaken = imageCursor.getLong(dateTakenIndex);

            @PickerBitmap.TileTypes int type = PickerBitmap.TileTypes.PICTURE;
            if (mimeType.startsWith("video/")) type = PickerBitmap.TileTypes.VIDEO;

            pickerBitmaps.add(new PickerBitmap(uri, dateTaken, type));
        }
        imageCursor.close();

        if (shouldShowBrowseTile()) {
            pickerBitmaps.add(0, new PickerBitmap(null, 0, PickerBitmap.TileTypes.GALLERY));
        }
        if (shouldShowCameraTile()) {
            pickerBitmaps.add(0, new PickerBitmap(null, 0, PickerBitmap.TileTypes.CAMERA));
        }

        return pickerBitmaps;
    }

    @Override
    protected void onCancelled() {
        super.onCancelled();
        mCallback.filesEnumeratedCallback(null);
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     *
     * @param files The resulting list of files on disk.
     */
    @Override
    protected void onPostExecute(List<PickerBitmap> files) {
        if (isCancelled()) {
            return;
        }

        mCallback.filesEnumeratedCallback(files);
    }

    /**
     * Creates a cursor containing the image files to show. Can be overridden in tests to provide
     * fake data.
     */
    protected Cursor createImageCursor(
            Uri contentUri,
            String[] selectColumns,
            String whereClause,
            String[] whereArgs,
            String orderBy) {
        return mContentResolver.query(contentUri, selectColumns, whereClause, whereArgs, orderBy);
    }

    /** Returns whether to include the Camera tile also. */
    protected boolean shouldShowCameraTile() {
        boolean hasCameraAppAvailable =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_IMAGE_CAPTURE));
        boolean hasOrCanRequestCameraPermission =
                (mWindowAndroid.hasPermission(Manifest.permission.CAMERA)
                        || mWindowAndroid.canRequestPermission(Manifest.permission.CAMERA));
        return hasCameraAppAvailable && hasOrCanRequestCameraPermission;
    }

    /** Returns whether to include the Browse tile also. */
    protected boolean shouldShowBrowseTile() {
        return !PhotoPickerFeatures.launchRegularWithoutBrowse();
    }
}
