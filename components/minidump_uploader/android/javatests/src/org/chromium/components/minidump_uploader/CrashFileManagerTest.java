// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import static org.junit.Assert.assertArrayEquals;

import android.os.ParcelFileDescriptor;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.Date;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

/** Unittests for {@link CrashFileManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CrashFileManagerTest {
    @Rule public CrashTestRule mTestRule = new CrashTestRule();

    private static final int TEST_PID = 23;
    private static final int TEST_PID2 = 24;

    private long mInitialModificationTimestamp;
    private long mModificationTimestamp;

    private static final int MAX_TRIES_ALLOWED = 3;
    private static final int MULTI_DIGIT_MAX_TRIES_ALLOWED = 20;

    private File mTmpFile1;
    private File mTmpFile2;
    private File mTmpFile3;

    private File mDmpSansLogcatFile1;
    private File mDmpSansLogcatFile2;
    private File mDmpSansLogcatFileForPid2;

    private File mDmpFile1;
    private File mDmpFile2;

    private File mOneBelowMaxTriesFile; // MAX_TRIES_ALLOWED-1 tries.
    private File mMaxTriesFile; // MAX_TRIES_ALLOWED tries.

    private File mOneBelowMultiDigitMaxTriesFile; // MULTI_DIGIT_MAX_TRIES_ALLOWED-1 tries.
    private File mMultiDigitMaxTriesFile; // MULTI_DIGIT_MAX_TRIES_ALLOWED tries.

    private File mUpFile1;
    private File mUpFile2;

    private File mLogfile;

    @Before
    public void setUp() throws Exception {
        mInitialModificationTimestamp = new Date().getTime();
        mModificationTimestamp = mInitialModificationTimestamp;

        // The following files will be deleted in CrashTestRule#tearDown().
        mTmpFile1 = new File(mTestRule.getCrashDir(), "12345ABCDE" + CrashFileManager.TMP_SUFFIX);
        mTmpFile1.createNewFile();
        mTmpFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mTmpFile2 = new File(mTestRule.getCrashDir(), "abcde12345" + CrashFileManager.TMP_SUFFIX);
        mTmpFile2.createNewFile();
        mTmpFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mTmpFile3 = new File(mTestRule.getCrashDir(), "abcdefghi" + CrashFileManager.TMP_SUFFIX);
        mTmpFile3.createNewFile();
        mTmpFile3.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpSansLogcatFile1 = new File(mTestRule.getCrashDir(), "123_abc.dmp");
        mDmpSansLogcatFile1.createNewFile();
        mDmpSansLogcatFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpSansLogcatFile2 =
                new File(mTestRule.getCrashDir(), "chromium-renderer_abc.dmp" + TEST_PID);
        mDmpSansLogcatFile2.createNewFile();
        mDmpSansLogcatFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpSansLogcatFileForPid2 = new File(mTestRule.getCrashDir(), "321_cba.dmp" + TEST_PID2);
        mDmpSansLogcatFileForPid2.createNewFile();
        mDmpSansLogcatFileForPid2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpFile1 = new File(mTestRule.getCrashDir(), "123_abc.dmp.try0");
        mDmpFile1.createNewFile();
        mDmpFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mDmpFile2 =
                new File(mTestRule.getCrashDir(), "chromium-renderer_abc.dmp" + TEST_PID + ".try1");
        mDmpFile2.createNewFile();
        mDmpFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mOneBelowMaxTriesFile =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer_abc.dmp" + TEST_PID + ".try" + (MAX_TRIES_ALLOWED - 1));
        mOneBelowMaxTriesFile.createNewFile();
        mOneBelowMaxTriesFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mMaxTriesFile =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer_abc.dmp" + TEST_PID + ".try" + MAX_TRIES_ALLOWED);
        mMaxTriesFile.createNewFile();
        mMaxTriesFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mOneBelowMultiDigitMaxTriesFile =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer_abc.dmp"
                                + TEST_PID
                                + ".try"
                                + (MULTI_DIGIT_MAX_TRIES_ALLOWED - 1));
        mOneBelowMultiDigitMaxTriesFile.createNewFile();
        mOneBelowMultiDigitMaxTriesFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mMultiDigitMaxTriesFile =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer_abc.dmp"
                                + TEST_PID
                                + ".try"
                                + MULTI_DIGIT_MAX_TRIES_ALLOWED);
        mMultiDigitMaxTriesFile.createNewFile();
        mMultiDigitMaxTriesFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mUpFile1 = new File(mTestRule.getCrashDir(), "123_abcd.up.try0");
        mUpFile1.createNewFile();
        mUpFile1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mUpFile2 =
                new File(mTestRule.getCrashDir(), "chromium-renderer_abcd.up" + TEST_PID + ".try0");
        mUpFile2.createNewFile();
        mUpFile2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        mLogfile = new File(mTestRule.getCrashDir(), CrashFileManager.CRASH_DUMP_LOGFILE);
        mLogfile.createNewFile();
        mLogfile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testReadyForUploadForFirstTime() {
        Assert.assertTrue(
                CrashFileManager.isReadyUploadForFirstTime(
                        new File(mTestRule.getCrashDir(), "foo.try0")));
        Assert.assertFalse(
                CrashFileManager.isReadyUploadForFirstTime(
                        new File(mTestRule.getCrashDir(), "foo.try1")));
        Assert.assertFalse(
                CrashFileManager.isReadyUploadForFirstTime(
                        new File(mTestRule.getCrashDir(), "foo")));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCrashFileManagerWithNull() {
        try {
            new CrashFileManager(null);
            Assert.fail("Constructor should throw NullPointerException with null context.");
        } catch (NullPointerException npe) {
            return;
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMatchingFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        // Three files begin with 123.
        File[] expectedFiles = new File[] {mUpFile1, mDmpFile1, mDmpSansLogcatFile1, mTmpFile1};
        Pattern testPattern = Pattern.compile("^123");
        File[] actualFiles = crashFileManager.listCrashFiles(testPattern);
        Assert.assertNotNull(actualFiles);
        assertArrayEquals("Failed to match file by pattern", expectedFiles, actualFiles);
    }

    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testFileComparator() {
        File[] expectedFiles = new File[] {mTmpFile3, mTmpFile2, mTmpFile1};
        File[] originalFiles = new File[] {mTmpFile1, mTmpFile2, mTmpFile3};
        Arrays.sort(originalFiles, CrashFileManager.sFileComparator);
        Assert.assertNotNull(originalFiles);
        assertArrayEquals(
                "File comparator failed to prioritize last modified file",
                expectedFiles,
                originalFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllFilesSorted() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles =
                new File[] {
                    mLogfile,
                    mUpFile2,
                    mUpFile1,
                    mMultiDigitMaxTriesFile,
                    mOneBelowMultiDigitMaxTriesFile,
                    mMaxTriesFile,
                    mOneBelowMaxTriesFile,
                    mDmpFile2,
                    mDmpFile1,
                    mDmpSansLogcatFileForPid2,
                    mDmpSansLogcatFile2,
                    mDmpSansLogcatFile1,
                    mTmpFile3,
                    mTmpFile2,
                    mTmpFile1
                };
        File[] actualFiles = crashFileManager.listCrashFiles(null);
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to sort all files by modification time", expectedFiles, actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashDirectory() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File actualFile = crashFileManager.getCrashDirectory();
        Assert.assertTrue(actualFile.isDirectory());
        Assert.assertEquals(mTestRule.getCrashDir(), actualFile);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testDeleteFile() {
        Assert.assertTrue(mTmpFile1.exists());
        Assert.assertTrue(CrashFileManager.deleteFile(mTmpFile1));
        Assert.assertFalse(mTmpFile1.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testIsMinidumpSansLogcat() {
        Assert.assertTrue(CrashFileManager.isMinidumpSansLogcat("foo.dmp"));
        Assert.assertTrue(CrashFileManager.isMinidumpSansLogcat("foo.dmp" + TEST_PID));

        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.other"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmp.try0"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmp.try2"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmpblah"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmp.other"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmp" + TEST_PID + ".try0"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmp" + TEST_PID + ".try2"));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmpblah" + TEST_PID));
        Assert.assertFalse(CrashFileManager.isMinidumpSansLogcat("foo.dmp" + TEST_PID + ".other"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testSetReadyForUpload_MinidumpWithoutPid() throws IOException {
        File minidumpWithoutLogcat = new File(mTestRule.getCrashDir(), "foo.dmp");
        minidumpWithoutLogcat.createNewFile();

        File result = CrashFileManager.trySetReadyForUpload(minidumpWithoutLogcat);
        Assert.assertEquals("foo.dmp.try0", result.getName());
        Assert.assertTrue(result.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testSetReadyForUpload_MinidumpWithPid() throws IOException {
        File minidumpWithoutLogcat = new File(mTestRule.getCrashDir(), "foo.dmp" + TEST_PID);
        minidumpWithoutLogcat.createNewFile();

        File result = CrashFileManager.trySetReadyForUpload(minidumpWithoutLogcat);
        Assert.assertEquals("foo.dmp" + TEST_PID + ".try0", result.getName());
        Assert.assertTrue(result.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpSansLogcatForPid() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File expectedFile = mDmpSansLogcatFileForPid2;
        File actualFile = crashFileManager.getMinidumpSansLogcatForPid(TEST_PID2);
        Assert.assertNotNull(actualFile);
        Assert.assertEquals(expectedFile, actualFile);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpsSansLogcat() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles =
                new File[] {mDmpSansLogcatFileForPid2, mDmpSansLogcatFile2, mDmpSansLogcatFile1};
        File[] actualFiles = crashFileManager.getMinidumpsSansLogcat();
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct minidump files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpsReadyForUpload() throws IOException {
        File forcedFile = new File(mTestRule.getCrashDir(), "456_def.forced" + TEST_PID + ".try2");
        forcedFile.createNewFile();
        forcedFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles = new File[] {forcedFile, mOneBelowMaxTriesFile, mDmpFile2, mDmpFile1};
        File[] actualFiles = crashFileManager.getMinidumpsReadyForUpload(MAX_TRIES_ALLOWED);
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct minidump files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpsReadyForUpload_MultiDigitMaxTries() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles =
                new File[] {
                    mOneBelowMultiDigitMaxTriesFile,
                    mMaxTriesFile,
                    mOneBelowMaxTriesFile,
                    mDmpFile2,
                    mDmpFile1
                };
        File[] actualFiles =
                crashFileManager.getMinidumpsReadyForUpload(MULTI_DIGIT_MAX_TRIES_ALLOWED);
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct minidump files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpsNotForcedReadyForUpload() throws IOException {
        File forcedFile = new File(mTestRule.getCrashDir(), "456_def.forced" + TEST_PID + ".try2");
        forcedFile.createNewFile();
        forcedFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles =
                new File[] {
                    mMultiDigitMaxTriesFile,
                    mOneBelowMultiDigitMaxTriesFile,
                    mMaxTriesFile,
                    mOneBelowMaxTriesFile,
                    mDmpFile2,
                    mDmpFile1
                };
        File[] actualFiles = crashFileManager.getMinidumpsNotForcedReadyForUpload();
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct minidump files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpsSkippedUpload() throws IOException {
        File skippedFile =
                new File(mTestRule.getCrashDir(), "456_def.skipped" + TEST_PID + ".try2");
        skippedFile.createNewFile();
        skippedFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles = new File[] {skippedFile};
        File[] actualFiles = crashFileManager.getMinidumpsSkippedUpload();
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct minidump files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetMinidumpsForcedUpload() throws IOException {
        File forcedFile = new File(mTestRule.getCrashDir(), "456_def.forced" + TEST_PID + ".try2");
        forcedFile.createNewFile();
        forcedFile.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles = new File[] {forcedFile};
        File[] actualFiles = crashFileManager.getMinidumpsForcedUpload();
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct minidump files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetFilesBelowMaxTries() {
        // No files in input -> return empty
        assertArrayEquals(
                new File[0],
                CrashFileManager.getFilesBelowMaxTries(new File[0], MAX_TRIES_ALLOWED));
        // Only files above MAX_TRIES -> return empty
        assertArrayEquals(
                new File[0],
                CrashFileManager.getFilesBelowMaxTries(
                        new File[] {mMaxTriesFile}, MAX_TRIES_ALLOWED));
        // Keep only files below MAX_TRIES
        assertArrayEquals(
                new File[] {mDmpFile1, mDmpFile2, mOneBelowMaxTriesFile},
                CrashFileManager.getFilesBelowMaxTries(
                        new File[] {mDmpFile1, mDmpFile2, mOneBelowMaxTriesFile, mMaxTriesFile},
                        MAX_TRIES_ALLOWED));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetAllUploadedFiles() {
        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        File[] expectedFiles = new File[] {mUpFile2, mUpFile1};
        File[] actualFiles = crashFileManager.getAllUploadedFiles();
        Assert.assertNotNull(actualFiles);
        assertArrayEquals(
                "Failed to get the correct uploaded files in directory",
                expectedFiles,
                actualFiles);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testReadAttemptNumber() {
        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.dmp"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.dmp"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber(".try"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal(".try"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("try1"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("try1"));

        Assert.assertEquals(1, CrashFileManager.readAttemptNumber("file.try1.dmp"));
        Assert.assertEquals(1, CrashFileManager.readAttemptNumberInternal("file.try1.dmp"));

        Assert.assertEquals(1, CrashFileManager.readAttemptNumber("file.dmp.try1"));
        Assert.assertEquals(1, CrashFileManager.readAttemptNumberInternal("file.dmp.try1"));

        Assert.assertEquals(2, CrashFileManager.readAttemptNumber(".try2"));
        Assert.assertEquals(2, CrashFileManager.readAttemptNumberInternal(".try2"));

        Assert.assertEquals(2, CrashFileManager.readAttemptNumber("file.try2.dmp"));
        Assert.assertEquals(2, CrashFileManager.readAttemptNumberInternal("file.try2.dmp"));

        Assert.assertEquals(2, CrashFileManager.readAttemptNumber("file.dmp.try2"));
        Assert.assertEquals(2, CrashFileManager.readAttemptNumberInternal("file.dmp.try2"));

        Assert.assertEquals(2, CrashFileManager.readAttemptNumber(".try2"));
        Assert.assertEquals(2, CrashFileManager.readAttemptNumberInternal(".try2"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.tryN.dmp"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.tryN.dmp"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.tryN.dmp1"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.tryN.dmp1"));

        Assert.assertEquals(9, CrashFileManager.readAttemptNumber("file.try9.dmp"));
        Assert.assertEquals(9, CrashFileManager.readAttemptNumberInternal("file.try9.dmp"));

        Assert.assertEquals(10, CrashFileManager.readAttemptNumber("file.try10.dmp"));
        Assert.assertEquals(10, CrashFileManager.readAttemptNumberInternal("file.try10.dmp"));

        Assert.assertEquals(9, CrashFileManager.readAttemptNumber("file.dmp.try9"));
        Assert.assertEquals(9, CrashFileManager.readAttemptNumberInternal("file.dmp.try9"));

        Assert.assertEquals(10, CrashFileManager.readAttemptNumber("file.dmp.try10"));
        Assert.assertEquals(10, CrashFileManager.readAttemptNumberInternal("file.dmp.try10"));

        Assert.assertEquals(300, CrashFileManager.readAttemptNumber("file.dmp.try300"));
        Assert.assertEquals(300, CrashFileManager.readAttemptNumberInternal("file.dmp.try300"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.dmp202.try"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.dmp202.try"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.try.dmp1"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.try.dmp1"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.try-2.dmp1"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.try-2.dmp1"));

        Assert.assertEquals(0, CrashFileManager.readAttemptNumber("file.try-20.dmp1"));
        Assert.assertEquals(-1, CrashFileManager.readAttemptNumberInternal("file.try-20.dmp1"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testAttemptNumberRename() {
        Assert.assertEquals(
                "file.dmp.try1", CrashFileManager.filenameWithIncrementedAttemptNumber("file.dmp"));
        Assert.assertEquals(
                "f.dmp.try2", CrashFileManager.filenameWithIncrementedAttemptNumber("f.dmp.try1"));
        Assert.assertEquals(
                "f.dmp.try10", CrashFileManager.filenameWithIncrementedAttemptNumber("f.dmp.try9"));
        Assert.assertEquals(
                "f.dmp.try11",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.dmp.try10"));
        Assert.assertEquals(
                "f.try2.dmp", CrashFileManager.filenameWithIncrementedAttemptNumber("f.try1.dmp"));
        Assert.assertEquals(
                "f.tryN.dmp.try1",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.tryN.dmp"));
        // Cover the case where there exists a number after (but not immediately after) ".try".
        Assert.assertEquals(
                "f.tryN.dmp2.try1",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.tryN.dmp2"));
        Assert.assertEquals(
                "f.forced.try3",
                CrashFileManager.filenameWithIncrementedAttemptNumber("f.forced.try2"));
        Assert.assertEquals(
                "file.dmp.try1",
                CrashFileManager.filenameWithIncrementedAttemptNumber("file.dmp.try0"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFilenameWithForcedUploadState() {
        // The ".try0" suffix is sometimes implicit -- in particular, when logcat extraction fails.
        Assert.assertEquals(
                "file.forced", CrashFileManager.filenameWithForcedUploadState("file.dmp"));
        // A not-yet-attempted upload.
        Assert.assertEquals(
                "file.forced0.try0",
                CrashFileManager.filenameWithForcedUploadState("file.dmp0.try0"));
        // A failed upload.
        Assert.assertEquals(
                "file.forced12.try0",
                CrashFileManager.filenameWithForcedUploadState("file.dmp12.try3"));

        // The same set of tests as above, but for skipped uploads rather than failed or
        // not-yet-attempted uploads.
        Assert.assertEquals(
                "file.forced", CrashFileManager.filenameWithForcedUploadState("file.skipped"));
        Assert.assertEquals(
                "file.forced0.try0",
                CrashFileManager.filenameWithForcedUploadState("file.skipped0.try0"));
        Assert.assertEquals(
                "file.forced12.try0",
                CrashFileManager.filenameWithForcedUploadState("file.skipped12.try3"));

        // A failed previously-forced upload.
        Assert.assertEquals(
                "file.forced0.try0",
                CrashFileManager.filenameWithForcedUploadState("file.forced0.try3"));
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMarkUploadSuccess() {
        CrashFileManager.markUploadSuccess(mDmpFile1);
        Assert.assertFalse(mDmpFile1.exists());
        Assert.assertTrue(new File(mTestRule.getCrashDir(), "123_abc.up.try0").exists());

        CrashFileManager.markUploadSuccess(mDmpFile2);
        Assert.assertFalse(mDmpFile2.exists());
        Assert.assertTrue(
                new File(mTestRule.getCrashDir(), "chromium-renderer_abc.up" + TEST_PID + ".try1")
                        .exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMarkUploadSuccess_ForcedUpload() throws IOException {
        File forced = new File(mTestRule.getCrashDir(), "123_abc.forced" + TEST_PID + ".try0");
        forced.createNewFile();
        CrashFileManager.markUploadSuccess(forced);
        Assert.assertFalse(forced.exists());
        Assert.assertTrue(
                new File(mTestRule.getCrashDir(), "123_abc.up" + TEST_PID + ".try0").exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMarkUploadSkipped() {
        CrashFileManager.markUploadSkipped(mDmpFile1);
        Assert.assertFalse(mDmpFile1.exists());
        Assert.assertTrue(new File(mTestRule.getCrashDir(), "123_abc.skipped.try0").exists());

        CrashFileManager.markUploadSkipped(mDmpFile2);
        Assert.assertFalse(mDmpFile2.exists());
        Assert.assertTrue(
                new File(
                                mTestRule.getCrashDir(),
                                "chromium-renderer_abc.skipped" + TEST_PID + ".try1")
                        .exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFilterMinidumpFilesOnUid() {
        assertArrayEquals(
                new File[] {new File("1_md.dmp.try0")},
                CrashFileManager.filterMinidumpFilesOnUid(new File[] {new File("1_md.dmp.try0")}, 1)
                        .toArray());
        assertArrayEquals(
                new File[0],
                CrashFileManager.filterMinidumpFilesOnUid(
                                new File[] {new File("1_md.dmp.try0")}, 12)
                        .toArray());
        assertArrayEquals(
                new File[0],
                CrashFileManager.filterMinidumpFilesOnUid(
                                new File[] {new File("12_md.dmp.try0")}, 1)
                        .toArray());
        assertArrayEquals(
                new File[] {new File("1000_md.dmp.try0")},
                CrashFileManager.filterMinidumpFilesOnUid(
                                new File[] {new File("1000_md.dmp.try0")}, 1000)
                        .toArray());
        assertArrayEquals(
                new File[0],
                CrashFileManager.filterMinidumpFilesOnUid(
                                new File[] {new File("-1000_md.dmp.try0")}, -10)
                        .toArray());
        assertArrayEquals(
                new File[] {new File("-10_md.dmp.try0")},
                CrashFileManager.filterMinidumpFilesOnUid(
                                new File[] {new File("-10_md.dmp.try0")}, -10)
                        .toArray());

        File[] expected =
                new File[] {
                    new File("12_md.dmp.try0"),
                    new File("12_.dmp.try0"),
                    new File("12_minidump.dmp.try0"),
                    new File("12_1_md.dmp.try0")
                };
        assertArrayEquals(
                expected,
                CrashFileManager.filterMinidumpFilesOnUid(
                                new File[] {
                                    new File("12_md.dmp.try0"),
                                    new File("100_.dmp.try0"),
                                    new File("4000_.dmp.try0"),
                                    new File("12_.dmp.try0"),
                                    new File("12_minidump.dmp.try0"),
                                    new File("23_.dmp.try0"),
                                    new File("12-.dmp.try0"),
                                    new File("12*.dmp.try0"),
                                    new File("12<.dmp.try0"),
                                    new File("12=.dmp.try0"),
                                    new File("12+.dmp.try0"),
                                    new File("12_1_md.dmp.try0"),
                                    new File("1_12_md.dmp.try0")
                                },
                                /* uid= */ 12)
                        .toArray());
    }

    /**
     * Ensure we handle minidump copying correctly when we have reached our limit on the number of
     * stored minidumps for a certain uid.
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMinidumpStorageRestrictionsPerUid() throws IOException {
        testMinidumpStorageRestrictions(/* perUid= */ true);
    }

    /*
     * Ensure we handle minidump copying correctly when we have reached our limit on the number of
     * stored minidumps.
     */
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testMinidumpStorageRestrictionsGlobal() throws IOException {
        testMinidumpStorageRestrictions(/* perUid= */ false);
    }

    private static void deleteFilesInDirIfExists(File directory) {
        if (directory.isDirectory()) {
            File[] files = directory.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (!file.delete()) {
                        throw new RuntimeException(
                                "Couldn't delete file " + file.getAbsolutePath());
                    }
                }
            }
        }
    }

    private void testMinidumpStorageRestrictions(boolean perUid) throws IOException {
        CrashFileManager fileManager = new CrashFileManager(mTestRule.getCacheDir());
        // Delete existing minidumps to ensure they don't interfere with this test.
        deleteFilesInDirIfExists(fileManager.getCrashDirectory());
        Assert.assertEquals(
                0, fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000).length);
        File tmpCopyDir = new File(mTestRule.getExistingCacheDir(), "tmpDir");

        // Note that these minidump files are set up directly in the cache dir - not in the crash
        // dir. This is to ensure the CrashFileManager doesn't see these minidumps without us first
        // copying them.
        File minidumpToCopy = new File(mTestRule.getExistingCacheDir(), "toCopy.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpToCopy, "BOUNDARY");
        // Ensure we didn't add any new minidumps to the crash directory.
        Assert.assertEquals(
                0, fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000).length);

        int minidumpLimit =
                perUid
                        ? CrashFileManager.MAX_CRASH_REPORTS_TO_UPLOAD_PER_UID
                        : CrashFileManager.MAX_CRASH_REPORTS_TO_UPLOAD;
        for (int n = 0; n < minidumpLimit; n++) {
            // If we are testing the app-throttling we want to use the same uid for each
            // minidump, otherwise use a different one for each minidump.
            createFdForandCopyFile(
                    fileManager,
                    minidumpToCopy,
                    tmpCopyDir,
                    /* uid= */ perUid ? 1 : n,
                    /* shouldSucceed= */ true);
        }

        // Update time-stamps of copied files.
        long initialTimestamp = new Date().getTime();
        File[] minidumps = fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000);
        for (int n = 0; n < minidumps.length; n++) {
            if (!minidumps[n].setLastModified(initialTimestamp + n * 1000L)) {
                throw new RuntimeException(
                        "Couldn't modify timestamp of " + minidumps[n].getAbsolutePath());
            }
        }

        File[] allMinidumps = fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000);
        Assert.assertEquals(minidumpLimit, allMinidumps.length);

        File oldestMinidump = getOldestFile(allMinidumps);

        // Now the crash directory is full - so copying a new minidump should cause the oldest
        // existing minidump to be deleted.
        createFdForandCopyFile(
                fileManager, minidumpToCopy, tmpCopyDir, /* uid= */ 1, /* shouldSucceed= */ true);
        Assert.assertEquals(
                minidumpLimit,
                fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000).length);
        // Ensure we removed the oldest file.
        Assert.assertFalse(oldestMinidump.exists());
    }

    /**
     * Utility method that creates (and closes) a file descriptor to {@param minidumpToCopy} and
     * calls CrashFileManager.copyMinidumpFromFD.
     */
    private static void createFdForandCopyFile(
            CrashFileManager fileManager,
            File minidumpToCopy,
            File tmpCopyDir,
            int uid,
            boolean shouldSucceed)
            throws IOException {
        ParcelFileDescriptor minidumpFd = null;
        try {
            minidumpFd =
                    ParcelFileDescriptor.open(minidumpToCopy, ParcelFileDescriptor.MODE_READ_ONLY);
            File copiedFile =
                    fileManager.copyMinidumpFromFD(minidumpFd.getFileDescriptor(), tmpCopyDir, uid);
            if (shouldSucceed) {
                Assert.assertNotNull(copiedFile);
            } else {
                Assert.assertNull(copiedFile);
            }
        } finally {
            if (minidumpFd != null) minidumpFd.close();
        }
    }

    /** Returns the oldest file in the set of files {@param files}. */
    private static File getOldestFile(File[] files) {
        File oldestFile = null;
        for (File file : files) {
            if (oldestFile == null || oldestFile.lastModified() > file.lastModified()) {
                oldestFile = file;
            }
        }
        return oldestFile;
    }

    /** Ensure that we won't copy minidumps that are too large. */
    @Test
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testCantCopyLargeFile() throws IOException {
        CrashFileManager fileManager = new CrashFileManager(mTestRule.getCacheDir());
        // Delete existing minidumps to ensure they don't interfere with this test.
        deleteFilesInDirIfExists(fileManager.getCrashDirectory());
        Assert.assertEquals(
                0, fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000).length);
        File tmpCopyDir = new File(mTestRule.getExistingCacheDir(), "tmpDir");

        // Note that these minidump files are set up directly in the cache dir - not in the crash
        // dir. This is to ensure the CrashFileManager doesn't see these minidumps without us first
        // copying them.
        File minidumpToCopy = new File(mTestRule.getExistingCacheDir(), "toCopy.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpToCopy, "BOUNDARY");
        // Write ~1MB data into the minidump file.
        final int kilo = 1024;
        byte[] kiloByteArray = new byte[kilo];
        FileOutputStream minidumpOutputStream =
                new FileOutputStream(minidumpToCopy, /* append= */ true);
        try {
            for (int n = 0; n < kilo; n++) {
                minidumpOutputStream.write(kiloByteArray);
            }
        } finally {
            minidumpOutputStream.close();
        }
        createFdForandCopyFile(
                fileManager, minidumpToCopy, tmpCopyDir, /* uid= */ 0, /* shouldSucceed= */ false);
        Assert.assertEquals(0, tmpCopyDir.listFiles().length);
        Assert.assertEquals(
                0, fileManager.getMinidumpsReadyForUpload(/* maxTries= */ 10000).length);
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCleanOutAllNonFreshMinidumpFiles() throws IOException {
        // Create some simulated old files.
        long oldTimestamp =
                mInitialModificationTimestamp - TimeUnit.MILLISECONDS.convert(31, TimeUnit.DAYS);
        File old1 = new File(mTestRule.getCrashDir(), "chromium-renderer-minidump-cooo10ff.dmp");
        File old2 = new File(mTestRule.getCrashDir(), "chromium-renderer-minidump-cooo10ff.up0");
        File old3 = new File(mTestRule.getCrashDir(), "chromium-renderer-minidump-cooo10ff.logcat");
        old1.setLastModified(oldTimestamp);
        old2.setLastModified(oldTimestamp - 1);
        old3.setLastModified(oldTimestamp - 2);

        // These will be the most recent files in the directory, after all successfully uploaded
        // files and all temp files are removed.
        File[] recentFiles = new File[CrashFileManager.MAX_CRASH_REPORTS_TO_KEEP];
        for (int i = 0; i < CrashFileManager.MAX_CRASH_REPORTS_TO_KEEP; ++i) {
            File recentMinidump =
                    new File(
                            mTestRule.getCrashDir(),
                            "chromium-renderer-minidump-deadbeef" + i + ".dmp");
            recentMinidump.createNewFile();
            recentMinidump.setLastModified(mModificationTimestamp);
            mModificationTimestamp += 1000;
            recentFiles[i] = recentMinidump;
        }

        // Create some additional successful uploads.
        File success1 =
                new File(mTestRule.getCrashDir(), "chromium-renderer-minidump-cafebebe1.up.try0");
        File success2 =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-cafebebe2.up" + TEST_PID + ".try1");
        File success3 =
                new File(
                        mTestRule.getCrashDir(),
                        "chromium-renderer-minidump-cafebebe3.up" + TEST_PID + ".try2");
        success1.createNewFile();
        success2.createNewFile();
        success3.createNewFile();
        success1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
        success2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
        success3.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        // Create some additional temp files.
        File temp1 = new File(mTestRule.getCrashDir(), "chromium-renderer-minidump-oooff1ce1.tmp");
        File temp2 = new File(mTestRule.getCrashDir(), "chromium-renderer-minidump-oooff1ce2.tmp");
        temp1.createNewFile();
        temp2.createNewFile();
        temp1.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;
        temp2.setLastModified(mModificationTimestamp);
        mModificationTimestamp += 1000;

        CrashFileManager crashFileManager = new CrashFileManager(mTestRule.getCacheDir());
        crashFileManager.cleanOutAllNonFreshMinidumpFiles();

        Assert.assertFalse(old1.exists());
        Assert.assertFalse(old2.exists());
        Assert.assertFalse(old3.exists());
        for (File f : recentFiles) {
            Assert.assertTrue(f.exists());
        }
        Assert.assertTrue(mLogfile.exists());
        Assert.assertFalse(mTmpFile1.exists());
        Assert.assertFalse(mTmpFile2.exists());
        Assert.assertFalse(mTmpFile3.exists());
        Assert.assertFalse(mDmpFile1.exists());
        Assert.assertFalse(mDmpFile2.exists());
        Assert.assertFalse(mOneBelowMaxTriesFile.exists());
        Assert.assertFalse(mMaxTriesFile.exists());
        Assert.assertFalse(mOneBelowMultiDigitMaxTriesFile.exists());
        Assert.assertFalse(mMultiDigitMaxTriesFile.exists());
        Assert.assertFalse(mUpFile1.exists());
        Assert.assertFalse(mUpFile2.exists());
        Assert.assertFalse(temp1.exists());
        Assert.assertFalse(temp2.exists());
        Assert.assertFalse(success1.exists());
        Assert.assertFalse(success2.exists());
        Assert.assertFalse(success3.exists());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testGetCrashLocalIdFromFileName() {
        Assert.assertEquals(
                "abc123d4",
                CrashFileManager.getCrashLocalIdFromFileName("pkg-process_1212-abc123d4.dmp"));
        Assert.assertEquals(
                "abc123_d4",
                CrashFileManager.getCrashLocalIdFromFileName(
                        "pkg-process_1212-abc123_d4.dmp.try001"));
        Assert.assertEquals(
                "abc123_d4",
                CrashFileManager.getCrashLocalIdFromFileName(
                        "pkg-process-1212,-abc123_d4.dmp.try001"));
        Assert.assertNull(
                CrashFileManager.getCrashLocalIdFromFileName("pkg-process-1234,5678.dmp-1.try001"));
        Assert.assertNull(
                CrashFileManager.getCrashLocalIdFromFileName("chromium_renderer.dmp.try001"));
    }
}
