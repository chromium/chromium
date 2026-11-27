// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import static org.mockito.ArgumentMatchers.eq;

import android.content.ContentResolver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.net.MimeTypeFilter;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link FileEnumWorkerTaskTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FileEnumWorkerTaskTest implements FileEnumWorkerTask.FilesEnumeratedCallback {
    private static class TestFileEnumWorkerTask extends FileEnumWorkerTask {
        private boolean mShouldShowCameraTile = true;
        private boolean mShouldShowBrowseTile = true;

        public TestFileEnumWorkerTask(
                WindowAndroid windowAndroid,
                FilesEnumeratedCallback callback,
                MimeTypeFilter filter,
                List<String> mimeTypes,
                ContentResolver contentResolver) {
            super(windowAndroid, callback, filter, mimeTypes, contentResolver);
        }

        public void setShouldShowCameraTile(boolean shouldShow) {
            mShouldShowCameraTile = shouldShow;
        }

        public void setShouldShowBrowseTile(boolean shouldShow) {
            mShouldShowBrowseTile = shouldShow;
        }

        @Override
        protected Cursor createImageCursor(
                Uri contentUri, String[] selectColumns, String[] whereArgs, String orderBy) {
            MatrixCursor cursor =
                    new MatrixCursor(
                            new String[] {
                                MediaStore.Files.FileColumns._ID,
                                MediaStore.Files.FileColumns.MIME_TYPE,
                                MediaStore.Files.FileColumns.DATE_ADDED,
                            });
            cursor.addRow(new Object[] {1, "text/html", 0L});
            cursor.addRow(new Object[] {2, "image/jpeg", 1L});
            cursor.addRow(new Object[] {3, "image/jpeg", 2L});
            cursor.addRow(new Object[] {4, "video/mp4", 3L});
            cursor.addRow(new Object[] {5, "video/mp4", 4L});
            return cursor;
        }

        @Override
        protected boolean shouldShowCameraTile() {
            return mShouldShowCameraTile;
        }

        @Override
        protected boolean shouldShowBrowseTile() {
            return mShouldShowBrowseTile;
        }
    }

    // A callback that fires the task completes.
    private final CallbackHelper mOnWorkerCompleteCallback = new CallbackHelper();

    // The last set of data returned by the FileEnumWorkerTask.
    private List<PickerBitmap> mFilesReturned;

    @Before
    public void setUp() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
    }

    // FileEnumWorkerTask.FilesEnumeratedCallback:
    @Override
    public void filesEnumeratedCallback(List<PickerBitmap> files) {
        mFilesReturned = files;
        mOnWorkerCompleteCallback.notifyCalled();
    }

    private void verifyCursorCreation(
            List<String> mimeTypes, String expectedWhereClause, String... expectedExtraArgs)
            throws Exception {
        ContentResolver contentResolver = Mockito.mock(ContentResolver.class);
        int callCount = mOnWorkerCompleteCallback.getCallCount();
        FileEnumWorkerTask task =
                new FileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        contentResolver);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForCallback(callCount);

        Uri contentUri = MediaStore.Files.getContentUri("external");
        String[] selectColumns = {
            MediaStore.Files.FileColumns._ID,
            MediaStore.Files.FileColumns.DATE_ADDED,
            MediaStore.Files.FileColumns.MEDIA_TYPE,
            MediaStore.Files.FileColumns.MIME_TYPE,
            MediaStore.Files.FileColumns.RELATIVE_PATH,
        };
        String orderBy = MediaStore.MediaColumns.DATE_ADDED + " DESC";

        ArgumentCaptor<String[]> argument = ArgumentCaptor.forClass(String[].class);
        Mockito.verify(contentResolver)
                .query(
                        eq(contentUri),
                        eq(selectColumns),
                        eq(expectedWhereClause),
                        argument.capture(),
                        eq(orderBy));
        String[] actualWhereArgs = argument.getValue();
        Assert.assertEquals(6 + expectedExtraArgs.length, actualWhereArgs.length);
        Assert.assertTrue(
                actualWhereArgs[0],
                actualWhereArgs[0].contains(Environment.DIRECTORY_DCIM + "/Camera"));
        Assert.assertTrue(
                actualWhereArgs[1], actualWhereArgs[1].contains(Environment.DIRECTORY_PICTURES));
        Assert.assertTrue(
                actualWhereArgs[2], actualWhereArgs[2].contains(Environment.DIRECTORY_MOVIES));
        Assert.assertTrue(
                actualWhereArgs[3], actualWhereArgs[3].contains(Environment.DIRECTORY_DOWNLOADS));
        Assert.assertTrue(
                actualWhereArgs[4],
                actualWhereArgs[4].contains(Environment.DIRECTORY_DCIM + "/Restored"));
        Assert.assertTrue(
                actualWhereArgs[5],
                actualWhereArgs[5].contains(Environment.DIRECTORY_DCIM + "/Screenshots"));
        for (int i = 0; i < expectedExtraArgs.length; i++) {
            Assert.assertEquals(expectedExtraArgs[i], actualWhereArgs[6 + i]);
        }
    }

    /**
     * Test cursor creation (with the help of a mocked ContentResolver). This calls direct into
     * {@link FileEnumWorkerTask} (as opposed to {@link TestFileEnumWorkerTask}), to make sure it
     * calls the {@link ContentResolver} back with the right parameters.
     */
    @Test
    public void testCursorCreation() throws Exception {
        String baseDirsWhere =
                "relative_path LIKE ? OR relative_path LIKE ? OR relative_path LIKE ? OR "
                        + "relative_path LIKE ? OR relative_path LIKE ? OR relative_path LIKE ?";
        String oneMediaTypeWhere = "(" + baseDirsWhere + ") AND media_type = ?";
        String twoMediaTypesWhere =
                "(" + baseDirsWhere + ") AND (media_type = ? OR media_type = ?)";
        String imageType = Integer.toString(MediaStore.Files.FileColumns.MEDIA_TYPE_IMAGE);
        String videoType = Integer.toString(MediaStore.Files.FileColumns.MEDIA_TYPE_VIDEO);

        verifyCursorCreation(Collections.singletonList(""), baseDirsWhere);
        verifyCursorCreation(Collections.singletonList("image/*"), oneMediaTypeWhere, imageType);
        verifyCursorCreation(Collections.singletonList("video/*"), oneMediaTypeWhere, videoType);
        verifyCursorCreation(
                Arrays.asList("image/*", "video/*"), twoMediaTypesWhere, imageType, videoType);
    }

    @Test
    public void testNoMimeTypes() throws Exception {
        List<String> mimeTypes = Collections.singletonList("");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForOnly();

        // If this assert hits, then onCancelled has been called in FileEnumWorkerTask, most likely
        // due to an exception thrown inside doInBackground. To surface the exception message, call
        // task.doInBackground() directly, instead of task.executeOnExecutor(...).
        Assert.assertTrue(mFilesReturned != null);
        Assert.assertEquals(2, mFilesReturned.size());

        PickerBitmap tile = mFilesReturned.get(0);
        Assert.assertEquals(PickerBitmap.TileTypes.CAMERA, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(1);
        Assert.assertEquals(PickerBitmap.TileTypes.GALLERY, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());
    }

    @Test
    public void testNoCameraTile() throws Exception {
        List<String> mimeTypes = Collections.singletonList("");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.setShouldShowCameraTile(false);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForOnly();

        // If this assert hits, then onCancelled has been called in FileEnumWorkerTask, most likely
        // due to an exception thrown inside doInBackground. To surface the exception message, call
        // task.doInBackground() directly, instead of task.executeOnExecutor(...).
        Assert.assertTrue(mFilesReturned != null);
        Assert.assertEquals(1, mFilesReturned.size());

        PickerBitmap tile = mFilesReturned.get(0);
        Assert.assertEquals(PickerBitmap.TileTypes.GALLERY, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());
    }

    @Test
    public void testNoBrowseTile() throws Exception {
        List<String> mimeTypes = Collections.singletonList("");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.setShouldShowBrowseTile(false);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForOnly();

        // If this assert hits, then onCancelled has been called in FileEnumWorkerTask, most likely
        // due to an exception thrown inside doInBackground. To surface the exception message, call
        // task.doInBackground() directly, instead of task.executeOnExecutor(...).
        Assert.assertTrue(mFilesReturned != null);
        Assert.assertEquals(1, mFilesReturned.size());

        PickerBitmap tile = mFilesReturned.get(0);
        Assert.assertEquals(PickerBitmap.TileTypes.CAMERA, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());
    }

    @Test
    public void testImagesOnly() throws Exception {
        List<String> mimeTypes = Collections.singletonList("image/*");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForOnly();

        // If this assert hits, then onCancelled has been called in FileEnumWorkerTask, most likely
        // due to an exception thrown inside doInBackground. To surface the exception message, call
        // task.doInBackground() directly, instead of task.executeOnExecutor(...).
        Assert.assertTrue(mFilesReturned != null);
        Assert.assertEquals(4, mFilesReturned.size());

        PickerBitmap tile = mFilesReturned.get(0);
        Assert.assertEquals(PickerBitmap.TileTypes.CAMERA, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(1);
        Assert.assertEquals(PickerBitmap.TileTypes.GALLERY, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(2);
        Assert.assertEquals(PickerBitmap.TileTypes.PICTURE, tile.type());
        Assert.assertEquals(1, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/2", tile.getUri().getPath());

        tile = mFilesReturned.get(3);
        Assert.assertEquals(PickerBitmap.TileTypes.PICTURE, tile.type());
        Assert.assertEquals(2, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/3", tile.getUri().getPath());
    }

    @Test
    public void testVideoOnly() throws Exception {
        // Try with just video files (plus camera and gallery tiles).
        List<String> mimeTypes = Collections.singletonList("video/*");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForOnly();

        // If this assert hits, then onCancelled has been called in FileEnumWorkerTask, most likely
        // due to an exception thrown inside doInBackground. To surface the exception message, call
        // task.doInBackground() directly, instead of task.executeOnExecutor(...).
        Assert.assertTrue(mFilesReturned != null);
        Assert.assertEquals(4, mFilesReturned.size());

        PickerBitmap tile = mFilesReturned.get(0);
        Assert.assertEquals(PickerBitmap.TileTypes.CAMERA, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(1);
        Assert.assertEquals(PickerBitmap.TileTypes.GALLERY, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(2);
        Assert.assertEquals(PickerBitmap.TileTypes.VIDEO, tile.type());
        Assert.assertEquals(3, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/4", tile.getUri().getPath());

        tile = mFilesReturned.get(3);
        Assert.assertEquals(PickerBitmap.TileTypes.VIDEO, tile.type());
        Assert.assertEquals(4, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/5", tile.getUri().getPath());
    }

    @Test
    public void testImagesAndVideos() throws Exception {
        List<String> mimeTypes = Arrays.asList("image/*", "video/*");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
        RobolectricUtil.runAllBackgroundAndUi();
        mOnWorkerCompleteCallback.waitForOnly();

        // If this assert hits, then onCancelled has been called in FileEnumWorkerTask, most likely
        // due to an exception thrown inside doInBackground. To surface the exception message, call
        // task.doInBackground() directly, instead of task.executeOnExecutor(...).
        Assert.assertTrue(mFilesReturned != null);
        Assert.assertEquals(6, mFilesReturned.size());

        PickerBitmap tile = mFilesReturned.get(0);
        Assert.assertEquals(PickerBitmap.TileTypes.CAMERA, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(1);
        Assert.assertEquals(PickerBitmap.TileTypes.GALLERY, tile.type());
        Assert.assertEquals(0, tile.getLastModifiedForTesting());
        Assert.assertEquals(null, tile.getUri());

        tile = mFilesReturned.get(2);
        Assert.assertEquals(PickerBitmap.TileTypes.PICTURE, tile.type());
        Assert.assertEquals(1, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/2", tile.getUri().getPath());

        tile = mFilesReturned.get(3);
        Assert.assertEquals(PickerBitmap.TileTypes.PICTURE, tile.type());
        Assert.assertEquals(2, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/3", tile.getUri().getPath());

        tile = mFilesReturned.get(4);
        Assert.assertEquals(PickerBitmap.TileTypes.VIDEO, tile.type());
        Assert.assertEquals(3, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/4", tile.getUri().getPath());

        tile = mFilesReturned.get(5);
        Assert.assertEquals(PickerBitmap.TileTypes.VIDEO, tile.type());
        Assert.assertEquals(4, tile.getLastModifiedForTesting());
        Assert.assertEquals("/external/file/5", tile.getUri().getPath());
    }
}
