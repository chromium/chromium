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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowParcelFileDescriptor;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {PdfContentProviderUnitTest.CustomShadowParcelFileDescriptor.class})
public class PdfContentProviderUnitTest {
    @Implements(ParcelFileDescriptor.class)
    public static class CustomShadowParcelFileDescriptor extends ShadowParcelFileDescriptor {
        @Implementation
        protected static ParcelFileDescriptor fromFd(int fd) throws IOException {
            File tempFile = File.createTempFile("shadow_pfd", ".pdf");
            tempFile.deleteOnExit();
            return ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_ONLY);
        }
    }

    private static final String TEST_UNIQUE_ID = "test_tab_id";
    private static final String TEST_FILE_NAME = "test_pdf.pdf";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private PdfContentProvider mProvider;
    @Mock private Context mContext;

    @Before
    public void setUp() throws IOException {
        mProvider = new PdfContentProvider();
        ContextUtils.initApplicationContextForTests(mContext);
        // Mock the package name for generating the URI
        when(mContext.getPackageName()).thenReturn("com.example.app");
    }

    @After
    public void tearDown() {
        PdfContentProvider.cleanUpForTesting();
    }

    private ParcelFileDescriptor createMockPfd() throws IOException {
        File tempFile = createTempFile();
        return ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Test(expected = FileNotFoundException.class)
    public void testCreateAndRemoveContentUri() throws IOException, FileNotFoundException {
        ParcelFileDescriptor pfd = createMockPfd();
        Uri uri =
                PdfContentProvider.createContentUri(
                        TEST_UNIQUE_ID, "dummy_path", pfd, TEST_FILE_NAME);
        assertNotNull("Content URI should not be null", uri);
        assertTrue(
                "Content URI should have the correct authority",
                uri.getAuthority().endsWith(".PdfContentProvider"));


        PdfContentProvider.removeContentUri(uri.toString());
        mProvider.openFile(uri, "r");
    }

    @Test
    public void testGetType() throws Exception {
        ParcelFileDescriptor pfd1 = createMockPfd();
        Uri uri =
                PdfContentProvider.createContentUri(
                        TEST_UNIQUE_ID, "dummy_path_1", pfd1, TEST_FILE_NAME);
        String type = mProvider.getType(uri);
        assertEquals("Mime type should be application/pdf", MimeTypeUtils.PDF_MIME_TYPE, type);

        // Create another uri.
        Thread.sleep(1);
        ParcelFileDescriptor pfd2 = createMockPfd();
        Uri uri2 = PdfContentProvider.createContentUri("another_id", "dummy_path_2", pfd2, "xyzs");
        type = mProvider.getType(uri2);
        assertEquals("Mime type should be application/pdf", MimeTypeUtils.PDF_MIME_TYPE, type);
        assertNotEquals("Content Uris should be different", uri, uri2);
    }

    @Test
    public void testGetStreamTypes() throws IOException {
        ParcelFileDescriptor pfd = createMockPfd();
        Uri uri =
                PdfContentProvider.createContentUri(
                        TEST_UNIQUE_ID, "dummy_path", pfd, TEST_FILE_NAME);
        String[] types = mProvider.getStreamTypes(uri, "*/*");
        assertNotNull("Stream types should not be null", types);
        assertEquals("There should be one stream type", 1, types.length);
        assertEquals(
                "Stream type should be application/pdf", MimeTypeUtils.PDF_MIME_TYPE, types[0]);

        String[] types2 = mProvider.getStreamTypes(uri, "*/pdf");
        String[] types3 = mProvider.getStreamTypes(uri, MimeTypeUtils.PDF_MIME_TYPE);
        String[] types4 = mProvider.getStreamTypes(uri, "application/*");
        assertEquals(types2, types);
        assertEquals(types3, types);
        assertEquals(types4, types);

        assertNull(mProvider.getStreamTypes(uri, "*/pdfx"));
        assertNull(mProvider.getStreamTypes(uri, "image/jpg"));
    }

    @Test
    public void testOpenFile() throws IOException, FileNotFoundException {
        ParcelFileDescriptor pfd = createMockPfd();
        Uri uri =
                PdfContentProvider.createContentUri(
                        TEST_UNIQUE_ID, "dummy_path", pfd, TEST_FILE_NAME);
        ParcelFileDescriptor openedPfd = mProvider.openFile(uri, "r");
        assertNotNull("ParcelFileDescriptor should not be null", openedPfd);
        assertNotEquals("Should return a duplicated PFD, not the same instance", pfd, openedPfd);
    }

    @Test(expected = FileNotFoundException.class)
    public void testOpenFile_FileNotFound() throws FileNotFoundException {
        mProvider.openFile(
                Uri.parse("content://com.example.app.PdfContentProvider/nonexistent"), "r");
    }

    @Test
    public void testOpenFile_WriteModeAllowed() throws IOException, FileNotFoundException {
        ParcelFileDescriptor pfd = createMockPfd();
        Uri uri =
                PdfContentProvider.createContentUri(
                        TEST_UNIQUE_ID, "dummy_path", pfd, TEST_FILE_NAME);
        ParcelFileDescriptor writePfd = mProvider.openFile(uri, "w");
        assertNotNull("Write ParcelFileDescriptor should not be null", writePfd);
    }

    @Test
    public void testQuery() throws IOException, FileNotFoundException {
        ParcelFileDescriptor pfd = createMockPfd();
        Uri uri =
                PdfContentProvider.createContentUri(
                        TEST_UNIQUE_ID, "dummy_path", pfd, TEST_FILE_NAME);

        Cursor cursor = mProvider.query(uri, null, null, null, null);
        assertNotNull("Cursor should not be null", cursor);
        assertTrue("Cursor should have results", cursor.moveToFirst());

        int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
        int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);

        assertFalse("Column indexes should be valid", nameIndex == -1 || sizeIndex == -1);
        assertEquals("File name should match", TEST_FILE_NAME, cursor.getString(nameIndex));
        assertTrue("File size should be greater than 0", cursor.getLong(sizeIndex) > 0);
    }

    @Test
    public void testRegisterStream_Success() {
        String incognitoPath = "/proc/self/fd/123";
        Uri uri = PdfContentProvider.registerStream(TEST_UNIQUE_ID, incognitoPath, TEST_FILE_NAME);
        assertNotNull("Uri should not be null", uri);
        assertTrue(
                "Uri should have PdfContentProvider authority",
                uri.getAuthority().endsWith(".PdfContentProvider"));

        // Test reuse for same tab and path
        Uri uri2 = PdfContentProvider.registerStream(TEST_UNIQUE_ID, incognitoPath, TEST_FILE_NAME);
        assertEquals("Should reuse URI for same tab and path", uri, uri2);

        // Different path should return a new URI
        String incognitoPath2 = "/proc/self/fd/124";
        Uri uri3 =
                PdfContentProvider.registerStream(TEST_UNIQUE_ID, incognitoPath2, TEST_FILE_NAME);
        assertNotNull("Uri for different path should not be null", uri3);
        assertNotEquals("Should not reuse URI for different path", uri, uri3);
    }

    @Test
    public void testRegisterStream_InvalidPath() {
        Uri uri =
                PdfContentProvider.registerStream(TEST_UNIQUE_ID, "/invalid/path", TEST_FILE_NAME);
        assertNull("Uri should be null for invalid path", uri);

        Uri uriNull = PdfContentProvider.registerStream(TEST_UNIQUE_ID, null, TEST_FILE_NAME);
        assertNull("Uri should be null for null path", uriNull);
    }

    private File createTempFile() {
        try {
            File tempFile = File.createTempFile("test_pdf", ".pdf");
            tempFile.deleteOnExit();
            FileOutputStream outputStream = new FileOutputStream(tempFile);
            outputStream.write(1234);
            outputStream.close();
            return tempFile;
        } catch (IOException e) {
            throw new AssertionError("Cannot create temporary file.", e);
        }
    }
}
