// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PdfContentProviderUnitTest {
    private static final String TEST_FILE_PATH = "/proc/5202/fd/344";
    private static final String TEST_FILE_NAME = "test_pdf.pdf";
    private PdfContentProvider mProvider;
    @Mock private Context mContext;

    @Before
    public void setUp() throws IOException {
        MockitoAnnotations.initMocks(this);
        mProvider = new PdfContentProvider();
        ContextUtils.initApplicationContextForTests(mContext);
        // Mock the package name for generating the URI
        when(mContext.getPackageName()).thenReturn("com.example.app");
    }

    @After
    public void tearDown() {
        // Ensure the test file is deleted
        PdfContentProvider.cleanUpForTesting();
    }

    @Test(expected = FileNotFoundException.class)
    public void testCreateAndRemoveContentUri() throws FileNotFoundException {
        Uri uri = PdfContentProvider.createContentUri(TEST_FILE_PATH, TEST_FILE_NAME);
        assertNotNull("Content URI should not be null", uri);
        assertTrue(
                "Content URI should have the correct authority",
                uri.getAuthority().endsWith(".PdfContentProvider"));
        PdfContentProvider.removeContentUri(uri.toString());
        mProvider.openFile(uri, "r");
    }

    @Test
    public void testCreateInvalidFilePath() throws FileNotFoundException {
        Uri uri = PdfContentProvider.createContentUri("/xyz/abc.pdf", TEST_FILE_NAME);
        assertNull(uri);
    }

    @Test
    public void testGetType() throws Exception {
        Uri uri = PdfContentProvider.createContentUri(TEST_FILE_PATH, TEST_FILE_NAME);
        String type = mProvider.getType(uri);
        assertEquals("Mime type should be application/pdf", "application/pdf", type);

        // Create another uri.
        Thread.sleep(1);
        Uri uri2 = PdfContentProvider.createContentUri(TEST_FILE_PATH, "xyzs");
        type = mProvider.getType(uri2);
        assertEquals("Mime type should be application/pdf", "application/pdf", type);
        assertNotEquals("Content Uris should be different", uri, uri2);
    }

    @Test
    public void testGetStreamTypes() {
        Uri uri = PdfContentProvider.createContentUri(TEST_FILE_PATH, TEST_FILE_NAME);
        String[] types = mProvider.getStreamTypes(uri, "*/*");
        assertNotNull("Stream types should not be null", types);
        assertEquals("There should be one stream type", 1, types.length);
        assertEquals("Stream type should be application/pdf", "application/pdf", types[0]);

        String[] types2 = mProvider.getStreamTypes(uri, "*/pdf");
        String[] types3 = mProvider.getStreamTypes(uri, "application/pdf");
        String[] types4 = mProvider.getStreamTypes(uri, "application/*");
        assertEquals(types2, types);
        assertEquals(types3, types);
        assertEquals(types4, types);

        assertNull(mProvider.getStreamTypes(uri, "*/pdfx"));
        assertNull(mProvider.getStreamTypes(uri, "image/jpg"));
    }

    @Test
    public void testOpenFile() throws FileNotFoundException {
        Uri uri = PdfContentProvider.createContentUri(TEST_FILE_PATH, TEST_FILE_NAME);
        File tempFile = createTempFile();
        PdfContentProvider.setPdfFileInfoForTesting(
                uri,
                new PdfContentProvider.PdfFileInfo(
                        TEST_FILE_PATH,
                        TEST_FILE_NAME,
                        ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_ONLY)));
        ParcelFileDescriptor pfd = mProvider.openFile(uri, "r");
        assertNotNull("ParcelFileDescriptor should not be null", pfd);
    }

    @Test(expected = FileNotFoundException.class)
    public void testOpenFile_FileNotFound() throws FileNotFoundException {
        mProvider.openFile(Uri.parse("content://nonexistent"), "r");
    }

    @Test
    public void testQuery() throws FileNotFoundException {
        Uri uri = PdfContentProvider.createContentUri(TEST_FILE_PATH, TEST_FILE_NAME);
        File tempFile = createTempFile();
        PdfContentProvider.setPdfFileInfoForTesting(
                uri,
                new PdfContentProvider.PdfFileInfo(
                        TEST_FILE_PATH,
                        TEST_FILE_NAME,
                        ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_ONLY)));

        Cursor cursor = mProvider.query(uri, null, null, null, null);
        assertNotNull("Cursor should not be null", cursor);
        assertTrue("Cursor should have results", cursor.moveToFirst());

        int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
        int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);

        assertFalse("Column indexes should be valid", nameIndex == -1 || sizeIndex == -1);
        assertEquals("File name should match", TEST_FILE_NAME, cursor.getString(nameIndex));
        assertTrue("File size should be greater than 0", cursor.getLong(sizeIndex) > 0);
    }

    private File createTempFile() {
        try {
            File tempFile = File.createTempFile("test_pdf", ".pdf");
            tempFile.deleteOnExit();
            FileOutputStream outputStream = new FileOutputStream(tempFile);
            outputStream.write(1234);
            return tempFile;
        } catch (IOException e) {
            throw new AssertionError("Cannot create temporary file.", e);
        }
    }
}
