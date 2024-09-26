// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;

import android.app.DownloadManager;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Environment;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.MediaStore;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.ClipboardImpl;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests of {@link ShareImageFileUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareImageFileUtilsTest extends BlankUiTestActivityTestCase {
    private static final long WAIT_TIMEOUT_SECONDS = 30L;
    private static final byte[] TEST_IMAGE_DATA = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    private static final String TEST_IMAGE_FILE_NAME = "chrome-test-bitmap";
    private static final String TEST_GIF_IMAGE_FILE_EXTENSION = ".gif";
    private static final String TEST_JPG_IMAGE_FILE_EXTENSION = ".jpg";
    private static final String TEST_PNG_IMAGE_FILE_EXTENSION = ".png";

    private static class GenerateUriCallback extends CallbackHelper implements Callback<Uri> {
        private Uri mImageUri;

        public Uri getImageUri() {
            return mImageUri;
        }

        @Override
        public void onResult(Uri uri) {
            mImageUri = uri;
            notifyCalled();
        }
    }

    private static class AsyncTaskRunnableHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    /** Convenient class to mark timestamp for ClipDescription. */
    private static class TestClipDescriptionWrapper extends ClipDescription {
        public static ClipDescription create(ClipDescription origin) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return origin;
            return new TestClipDescriptionWrapper(origin);
        }

        private final long mTimeStamp;

        private TestClipDescriptionWrapper(ClipDescription other) {
            super(other);
            mTimeStamp = SystemClock.elapsedRealtime();
        }

        @Override
        public long getTimestamp() {
            return mTimeStamp;
        }
    }

    @Mock ClipboardManager mMockClipboardManager;

    @Nullable ClipData mPrimaryClip;
    @Nullable ClipDescription mPrimaryClipDescription;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.openMocks(this);
        Looper.prepare();
        FileProviderUtils.setFileProviderUtil(new FileProviderHelper());
        ClipboardImpl clipboard = (ClipboardImpl) Clipboard.getInstance();
        clipboard.setImageFileProvider(new ClipboardImageFileProvider());

        // Setup mock clipboard manager for test.
        doAnswer(
                        invocationOnMock -> {
                            mPrimaryClip = invocationOnMock.getArgument(0);
                            mPrimaryClipDescription =
                                    new TestClipDescriptionWrapper(mPrimaryClip.getDescription());
                            return null;
                        })
                .when(mMockClipboardManager)
                .setPrimaryClip(notNull());
        doAnswer(
                        invocationOnMock -> {
                            return mPrimaryClip;
                        })
                .when(mMockClipboardManager)
                .getPrimaryClip();
        doAnswer(
                        invocationOnMock -> {
                            return mPrimaryClipDescription;
                        })
                .when(mMockClipboardManager)
                .getPrimaryClipDescription();
        clipboard.overrideClipboardManagerForTesting(mMockClipboardManager);
    }

    @Override
    public void tearDownTest() throws Exception {
        Clipboard.resetForTesting();
        clearSharedImages();
        deleteAllTestImages();
        super.tearDownTest();
    }

    private int fileCount(File file) {
        if (file.isFile()) {
            return 1;
        }

        int count = 0;
        if (file.isDirectory()) {
            for (File f : file.listFiles()) count += fileCount(f);
        }
        return count;
    }

    private boolean filepathExists(File file, String filepath) {
        if (file.isFile() && filepath.endsWith(file.getName())) {
            return true;
        }

        if (file.isDirectory()) {
            for (File f : file.listFiles()) {
                if (filepathExists(f, filepath)) return true;
            }
        }
        return false;
    }

    private Uri generateAnImageToClipboard(String fileExtension) throws TimeoutException {
        GenerateUriCallback imageCallback = new GenerateUriCallback();
        ShareImageFileUtils.generateTemporaryUriFromData(
                TEST_IMAGE_DATA, fileExtension, imageCallback);
        imageCallback.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Clipboard.getInstance().setImageUri(imageCallback.getImageUri());
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            Clipboard.getInstance().getImageUri(),
                            Matchers.is(imageCallback.getImageUri()));
                });
        return imageCallback.getImageUri();
    }

    private Uri generateAnImageToClipboard() throws TimeoutException {
        return generateAnImageToClipboard(TEST_JPG_IMAGE_FILE_EXTENSION);
    }

    private void clearSharedImages() throws TimeoutException {
        ShareImageFileUtils.clearSharedImages();

        // ShareImageFileUtils::clearSharedImages uses AsyncTask.SERIAL_EXECUTOR to schedule a
        // clearing the shared folder job, so schedule a new job and wait for the new job finished
        // to make sure ShareImageFileUtils::clearSharedImages's clearing folder job finished.
        waitForAsync();
    }

    private void waitForAsync() throws TimeoutException {
        AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
        AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
        runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        AsyncTask.THREAD_POOL_EXECUTOR.execute(runnableHelper);
        runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void deleteAllTestImages() throws TimeoutException {
        AsyncTask.SERIAL_EXECUTOR.execute(
                () -> {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        deleteMediaStoreFiles();
                    }
                    deleteExternalStorageFiles();
                });
        waitForAsync();
    }

    @RequiresApi(29)
    private void deleteMediaStoreFiles() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        Cursor cursor =
                contentResolver.query(MediaStore.Downloads.EXTERNAL_CONTENT_URI, null, null, null);
        while (cursor.moveToNext()) {
            long id = cursor.getLong(cursor.getColumnIndexOrThrow(MediaStore.Downloads._ID));
            Uri uri = ContentUris.withAppendedId(MediaStore.Downloads.EXTERNAL_CONTENT_URI, id);
            contentResolver.delete(uri, null, null);
        }
    }

    public void deleteExternalStorageFiles() {
        File externalStorageDir =
                ContextUtils.getApplicationContext()
                        .getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        String[] children = externalStorageDir.list();
        for (int i = 0; i < children.length; i++) {
            new File(externalStorageDir, children[i]).delete();
        }
    }

    private int fileCountInShareDirectory() throws IOException {
        return fileCount(ShareImageFileUtils.getSharedFilesDirectory());
    }

    private boolean fileExistsInShareDirectory(Uri fileUri) throws IOException {
        return filepathExists(ShareImageFileUtils.getSharedFilesDirectory(), fileUri.getPath());
    }

    @Test
    @SmallTest
    public void clipboardUriDoNotClearTest() throws TimeoutException, IOException {
        Uri clipboardUri = generateAnImageToClipboard(TEST_GIF_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(clipboardUri.getPath().endsWith(TEST_GIF_IMAGE_FILE_EXTENSION));
        clipboardUri = generateAnImageToClipboard(TEST_JPG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(clipboardUri.getPath().endsWith(TEST_JPG_IMAGE_FILE_EXTENSION));
        clipboardUri = generateAnImageToClipboard(TEST_PNG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(clipboardUri.getPath().endsWith(TEST_PNG_IMAGE_FILE_EXTENSION));
        Assert.assertEquals(3, fileCountInShareDirectory());

        clearSharedImages();
        Assert.assertEquals(1, fileCountInShareDirectory());
        Assert.assertTrue(fileExistsInShareDirectory(clipboardUri));
    }

    @Test
    @SmallTest
    public void clearEverythingIfNoClipboardImageTest() throws TimeoutException, IOException {
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        Assert.assertEquals(3, fileCountInShareDirectory());

        Clipboard.getInstance().setText("");
        clearSharedImages();
        Assert.assertEquals(0, fileCountInShareDirectory());
    }

    @Test
    @SmallTest
    public void testGetNextAvailableFile() throws IOException {
        String fileName = TEST_IMAGE_FILE_NAME + "_next_availble";
        File externalStorageDir =
                getActivity().getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        File imageFile =
                ShareImageFileUtils.getNextAvailableFile(
                        externalStorageDir.getPath(), fileName, TEST_JPG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(imageFile.exists());

        File imageFile2 =
                ShareImageFileUtils.getNextAvailableFile(
                        externalStorageDir.getPath(), fileName, TEST_JPG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(imageFile2.exists());
        Assert.assertNotEquals(imageFile.getPath(), imageFile2.getPath());
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(value = VERSION_CODES.P, reason = "Added to MediaStore.Downloads on Q+")
    public void testAddCompletedDownload() throws IOException {
        String filename =
                TEST_IMAGE_FILE_NAME + "_add_completed_download" + TEST_JPG_IMAGE_FILE_EXTENSION;
        File externalStorageDir =
                getActivity().getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        File qrcodeFile = new File(externalStorageDir, filename);
        Assert.assertTrue(qrcodeFile.createNewFile());

        long downloadId = ShareImageFileUtils.addCompletedDownload(qrcodeFile);
        Assert.assertNotEquals(0L, downloadId);

        DownloadManager downloadManager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        DownloadManager.Query query = new DownloadManager.Query().setFilterById(downloadId);
        Cursor c = downloadManager.query(query);

        Assert.assertNotNull(c);
        Assert.assertTrue(c.moveToFirst());
        Assert.assertEquals(
                filename, c.getString(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TITLE)));
        c.close();
    }
}
