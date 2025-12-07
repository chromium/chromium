// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.text.TextUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.MimeTypeUtils;

@RunWith(BaseRobolectricTestRunner.class)
public class PdfUtilsUnitTest {
    @Mock private LoadUrlParams mLoadUrlParams;
    @Mock private NativePage mNativePage;
    @Mock private Context mContext;
    @Mock private ContentResolver mContentResolver;
    private AutoCloseable mCloseableMocks;
    private String mPdfPageUrl;
    private String mPdfPageBlobUrl;

    private static final String DEFAULT_TAB_TITLE = "Loading PDF…";
    private static final String CONTENT_URL = "content://media/external/downloads/1000000022";
    private static final String CONTENT_URL_SPECIAL_CHARACTER =
            "content://com.android.chrome.DownloadFileProvider/external_volume?file=abc%20(1).pdf";
    private static final String FILE_URL = "file:///media/external/downloads/sample.pdf";
    private static final String PDF_LINK = "https://www.foo.com/testfiles/pdf/sample.pdf";
    private static final String PDF_BLOB_URL = "blob:https://www.foo.com/abc";
    private static final String PDF_LINK_ENCODED =
            "chrome-native://pdf/link?url=https%3A%2F%2Fwww.foo.com%2Ftestfiles%2Fpdf%2Fsample.pdf";
    private static final String FILE_PATH = "/media/external/downloads/sample.pdf";
    private static final String FILE_NAME = "sample.pdf";
    private static final String IMAGE_FILE_URL = "file:///media/external/downloads/sample.jpg";

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        PdfUtils.setShouldOpenPdfInlineForTesting(true);
        ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(CONTENT_URL));
        mPdfPageUrl = PdfUtils.encodePdfPageUrl(PDF_LINK);
        mPdfPageBlobUrl = PdfUtils.encodePdfPageUrl(PDF_BLOB_URL);
        when(mContext.getContentResolver()).thenReturn(mContentResolver);
    }

    @After
    public void tearDown() throws Exception {
        PdfUtils.setShouldOpenPdfInlineForTesting(false);
        mCloseableMocks.close();
        ChromeFileProvider.setGeneratedUriForTesting(null);
    }

    @Test
    public void testGetUriFromFilePath() {
        Uri uri = PdfUtils.getUriFromFilePath(FILE_PATH);
        Assert.assertNotNull("Uri should not be null.", uri);
        uri = PdfUtils.getUriFromFilePath(CONTENT_URL);
        Assert.assertNotNull("Uri should not be null.", uri);
        Assert.assertEquals(
                "Uri should match the input when it is already a content uri",
                CONTENT_URL,
                uri.toString());
        uri = PdfUtils.getUriFromFilePath(FILE_URL);
        Assert.assertNotNull("Uri should not be null.", uri);
        Assert.assertEquals(
                "Uri should match the input when it is already a file uri",
                FILE_URL,
                uri.toString());
    }

    @Test
    public void testGetFileNameFromUrl() {
        String filename = PdfUtils.getFileNameFromUrl(FILE_URL, DEFAULT_TAB_TITLE);
        Assert.assertEquals("Filename does not match for file url.", FILE_NAME, filename);

        filename = PdfUtils.getFileNameFromUrl(PDF_LINK, DEFAULT_TAB_TITLE);
        Assert.assertEquals("Filename does not match for pdf link.", DEFAULT_TAB_TITLE, filename);
    }

    @Test
    public void testIsPdfNavigation_FileScheme() {
        boolean result = PdfUtils.isPdfNavigation(FILE_URL, null);
        Assert.assertFalse(
                "It is not pdf navigation when the url does not start with chrome-native://pdf/.",
                result);

        result = PdfUtils.isPdfNavigation(PdfUtils.encodePdfPageUrl(FILE_URL), null);
        Assert.assertTrue("It is pdf navigation when the decoded url is file scheme.", result);
    }

    @Test
    public void testIsPdfNavigation_PdfLink() {
        doReturn(true).when(mLoadUrlParams).getIsPdf();
        boolean result = PdfUtils.isPdfNavigation(mPdfPageUrl, mLoadUrlParams);
        Assert.assertTrue("It is pdf navigation when IsPdf is set in LoadUrlParams.", result);

        doReturn(false).when(mLoadUrlParams).getIsPdf();
        result = PdfUtils.isPdfNavigation(mPdfPageUrl, mLoadUrlParams);
        Assert.assertFalse(
                "It is not pdf navigation when IsPdf is not set in LoadUrlParams.", result);

        result = PdfUtils.isPdfNavigation(mPdfPageUrl, null);
        Assert.assertFalse("It is not pdf navigation when LoadUrlParams is null.", result);
    }

    @Test
    public void testIsPdfNavigation_PdfLink_BlobUrl() {
        doReturn(true).when(mLoadUrlParams).getIsPdf();
        boolean result = PdfUtils.isPdfNavigation(mPdfPageBlobUrl, mLoadUrlParams);
        Assert.assertTrue("It is pdf navigation when IsPdf is set in LoadUrlParams.", result);

        doReturn(false).when(mLoadUrlParams).getIsPdf();
        result = PdfUtils.isPdfNavigation(mPdfPageBlobUrl, mLoadUrlParams);
        Assert.assertFalse(
                "It is not pdf navigation when IsPdf is not set in LoadUrlParams.", result);

        result = PdfUtils.isPdfNavigation(mPdfPageBlobUrl, null);
        Assert.assertFalse("It is not pdf navigation when LoadUrlParams is null.", result);
    }

    @Test
    public void testIsPdfNavigation_SchemeNotMatch() {
        boolean result =
                PdfUtils.isPdfNavigation(PdfUtils.encodePdfPageUrl(UrlConstants.HISTORY_URL), null);
        Assert.assertFalse(
                "It is not pdf navigation when the scheme is not one of content/file/http/https.",
                result);
    }

    @Test
    public void testIsDownloadedPdf_ContentScheme() {
        boolean result = PdfUtils.isDownloadedPdf(CONTENT_URL);
        Assert.assertFalse(
                "It is not downloaded pdf when the url does not start with chrome-native://pdf/.",
                result);

        result = PdfUtils.isDownloadedPdf(PdfUtils.encodePdfPageUrl(CONTENT_URL));
        Assert.assertTrue("It is downloaded pdf when the decoded url is content scheme.", result);
    }

    @Test
    public void testIsDownloadedPdf_SchemeNotMatch() {
        boolean result = PdfUtils.isDownloadedPdf(mPdfPageUrl);
        Assert.assertFalse(
                "It is not downloaded pdf when the scheme is not content or file.", result);
    }

    @Test
    public void testRecordIsPdfFrozen_NativePageNull() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Pdf.IsFrozenWhenDisplayed")
                        .build();
        PdfUtils.recordIsPdfFrozen(null);
        histogramExpectation.assertExpected(
                "The histogram should not be recorded when the native page is null.");
    }

    @Test
    public void testRecordIsPdfFrozen_NativePageIsNotPdf() {
        doReturn(false).when(mNativePage).isPdf();
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Pdf.IsFrozenWhenDisplayed")
                        .build();
        PdfUtils.recordIsPdfFrozen(mNativePage);
        histogramExpectation.assertExpected(
                "The histogram should not be recorded when the native page is not pdf page.");
    }

    @Test
    public void testRecordIsPdfFrozen_PdfIsFrozen() {
        doReturn(true).when(mNativePage).isPdf();
        doReturn(true).when(mNativePage).isFrozen();
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.IsFrozenWhenDisplayed", true)
                        .build();
        PdfUtils.recordIsPdfFrozen(mNativePage);
        histogramExpectation.assertExpected(
                "The recorded value should be true when the pdf page is frozen.");
    }

    @Test
    public void testRecordIsPdfFrozen_PdfIsNotFrozen() {
        doReturn(true).when(mNativePage).isPdf();
        doReturn(false).when(mNativePage).isFrozen();
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.IsFrozenWhenDisplayed", false)
                        .build();
        PdfUtils.recordIsPdfFrozen(mNativePage);
        histogramExpectation.assertExpected(
                "The recorded value should be false when the pdf page is not frozen.");
    }

    @Test
    public void testEncodePdfPageUrl() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.DownloadUrlEncoded", true)
                        .build();
        String encodedUrl = PdfUtils.encodePdfPageUrl(PDF_LINK);
        Assert.assertEquals("The encoded url should match", PDF_LINK_ENCODED, encodedUrl);
        histogramExpectation.assertExpected();
    }

    @Test
    public void testDecodePdfPageUrl() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.DownloadUrlDecoded", true)
                        .build();
        String decodedUrl = PdfUtils.decodePdfPageUrl(PDF_LINK_ENCODED);
        Assert.assertEquals("The decoded url should match", PDF_LINK, decodedUrl);
        histogramExpectation.assertExpected();
    }

    @Test
    public void testEncodeDecodeUrlWithSpecialCharacter() {
        String encodedUrl = PdfUtils.encodePdfPageUrl(CONTENT_URL_SPECIAL_CHARACTER);
        String decodedUrl = PdfUtils.decodePdfPageUrl(encodedUrl);
        Assert.assertEquals(
                "The decoded url should match", CONTENT_URL_SPECIAL_CHARACTER, decodedUrl);
    }

    @Test
    public void testGetEncodedContentUri_PDF() {
        when(mContentResolver.getType(any())).thenReturn(MimeTypeUtils.PDF_MIME_TYPE);
        String encodedUrl = PdfUtils.getEncodedContentUri(CONTENT_URL, mContext);
        Assert.assertFalse("The encoded url should exist", TextUtils.isEmpty(encodedUrl));
    }

    @Test
    public void testGetEncodedContentUri_Image() {
        when(mContentResolver.getType(any())).thenReturn("image/jpeg");
        String encodedUrl = PdfUtils.getEncodedContentUri(CONTENT_URL, mContext);
        Assert.assertTrue("The encoded url should not exist", TextUtils.isEmpty(encodedUrl));
    }

    @Test
    public void testGetEncodedContentUri_Https() {
        String encodedUrl = PdfUtils.getEncodedContentUri(PDF_LINK, mContext);
        Assert.assertTrue("The encoded url should not exist", TextUtils.isEmpty(encodedUrl));
    }
}
