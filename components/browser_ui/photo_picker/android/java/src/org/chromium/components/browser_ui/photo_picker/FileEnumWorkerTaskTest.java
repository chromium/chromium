// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import static org.mockito.ArgumentMatchers.eq;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

import androidx.annotation.IntDef;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.fakes.BaseCursor;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.net.MimeTypeFilter;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link FileEnumWorkerTaskTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class FileEnumWorkerTaskTest implements FileEnumWorkerTask.FilesEnumeratedCallback {
    // The Fields the test Cursor represents.
    @IntDef({Fields.ID, Fields.MIME_TYPE, Fields.DATE_ADDED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Fields {
        int ID = 0;
        int MIME_TYPE = 1;
        int DATE_ADDED = 2;
    }

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
                Uri contentUri,
                String[] selectColumns,
                String whereClause,
                String[] whereArgs,
                String orderBy) {
            ArrayList<TestData> list = new ArrayList<TestData>();
            list.add(new TestData("file0", "text/html", 0));
            list.add(new TestData("file1", "image/jpeg", 1));
            list.add(new TestData("file2", "image/jpeg", 2));
            list.add(new TestData("file3", "video/mp4", 3));
            list.add(new TestData("file4", "video/mp4", 4));

            return new FileCursor(list);
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

    private static class TestData {
        public Uri mUri;
        public String mMimeType;
        public long mDateAdded;

        public TestData(String uri, String mimeType, long dateAdded) {
            mUri = Uri.parse(uri);
            mMimeType = mimeType;
            mDateAdded = dateAdded;
        }
    }

    private static class FileCursor extends BaseCursor {
        private List<TestData> mData;

        private int mIndex;

        public FileCursor(List<TestData> data) {
            mData = data;
        }

        @Override
        public int getCount() {
            return mData.size();
        }

        @Override
        public boolean moveToNext() {
            return mIndex++ < mData.size();
        }

        @Override
        public int getColumnIndex(String columnName) {
            switch (columnName) {
                case MediaStore.Files.FileColumns._ID:
                    return Fields.ID;
                case MediaStore.Files.FileColumns.MIME_TYPE:
                    return Fields.MIME_TYPE;
                case MediaStore.Files.FileColumns.DATE_ADDED:
                    return Fields.DATE_ADDED;
                default:
                    return -1;
            }
        }

        @Override
        public int getInt(int columnIndex) {
            if (columnIndex == Fields.ID) {
                return mIndex;
            }
            return -1;
        }

        @Override
        public long getLong(int columnIndex) {
            if (columnIndex == Fields.DATE_ADDED) {
                return mData.get(mIndex - 1).mDateAdded;
            }
            return -1;
        }

        @Override
        public String getString(int columnIndex) {
            if (columnIndex == Fields.MIME_TYPE) {
                return mData.get(mIndex - 1).mMimeType;
            }
            return "";
        }

        @Override
        public void close() {}
    }

    private final RoboExecutorService mRoboExecutorService = new RoboExecutorService();

    // A callback that fires the task completes.
    private final CallbackHelper mOnWorkerCompleteCallback = new CallbackHelper();

    // The last set of data returned by the FileEnumWorkerTask.
    private List<PickerBitmap> mFilesReturned;

    @Before
    public void setUp() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
    }

    @After
    public void tearDown() {
        Assert.assertTrue(mRoboExecutorService.shutdownNow().isEmpty());
    }

    // FileEnumWorkerTask.FilesEnumeratedCallback:

    @Override
    public void filesEnumeratedCallback(List<PickerBitmap> files) {
        mFilesReturned = files;
        mOnWorkerCompleteCallback.notifyCalled();
    }

    /**
     * Test cursor creation (with the help of a mocked ContentResolver). This calls direct into
     * {@link FileEnumWorkerTask} (as opposed to {@link TestFileEnumWorkerTask}), to make sure it
     * calls the {@link ContentResolver} back with the right parameters.
     */
    @Test
    @SmallTest
    public void testCursorCreation() throws Exception {
        ContentResolver contentResolver = Mockito.mock(ContentResolver.class);
        List<String> mimeTypes = Collections.singletonList("");
        FileEnumWorkerTask task =
                new FileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        contentResolver);
        task.executeOnExecutor(mRoboExecutorService);
        mOnWorkerCompleteCallback.waitForOnly();

        Uri contentUri = MediaStore.Files.getContentUri("external");
        String[] selectColumns = {
            MediaStore.Files.FileColumns._ID,
            MediaStore.Files.FileColumns.DATE_ADDED,
            MediaStore.Files.FileColumns.MEDIA_TYPE,
            MediaStore.Files.FileColumns.MIME_TYPE,
            MediaStore.Files.FileColumns.DATA
        };
        String whereClause =
                "_data LIKE ? OR _data LIKE ? OR _data LIKE ? OR _data LIKE ? OR "
                        + "_data LIKE ? OR _data LIKE ?";
        String orderBy = MediaStore.MediaColumns.DATE_ADDED + " DESC";

        ArgumentCaptor<String[]> argument = ArgumentCaptor.forClass(String[].class);
        Mockito.verify(contentResolver)
                .query(
                        eq(contentUri),
                        eq(selectColumns),
                        eq(whereClause),
                        argument.capture(),
                        eq(orderBy));
        String[] actualWhereArgs = argument.getValue();
        Assert.assertEquals(6, actualWhereArgs.length);
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
    }

    @Test
    @SmallTest
    public void testNoMimeTypes() throws Exception {
        List<String> mimeTypes = Collections.singletonList("");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(mRoboExecutorService);
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
    @SmallTest
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
        task.executeOnExecutor(mRoboExecutorService);
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
    @SmallTest
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
        task.executeOnExecutor(mRoboExecutorService);
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
    @SmallTest
    public void testImagesOnly() throws Exception {
        List<String> mimeTypes = Collections.singletonList("image/*");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(mRoboExecutorService);
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
    @SmallTest
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
        task.executeOnExecutor(mRoboExecutorService);
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
    @SmallTest
    public void testImagesAndVideos() throws Exception {
        List<String> mimeTypes = Arrays.asList("image/*", "video/*");
        TestFileEnumWorkerTask task =
                new TestFileEnumWorkerTask(
                        /* windowAndroid= */ null,
                        this,
                        new MimeTypeFilter(mimeTypes, true),
                        mimeTypes,
                        /* contentResolver= */ null);
        task.executeOnExecutor(mRoboExecutorService);
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
