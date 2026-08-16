// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.net.Uri;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.io.File;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
@DisableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
@Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class PdfPageUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mMockNativePageHost;
    @Mock private Profile mMockProfile;
    @Mock private Destroyable mMarginSupplier;
    @Mock private PdfFragmentViewTracker mPdfFragmentViewTracker;
    @Mock private Tab mMockTab;

    private Activity mActivity;
    private UserDataHost mUserDataHost;
    private PdfInfo mPdfInfo;
    private String mPdfPageUrl;
    private String mPdfPageBlobUrl;

    private static final String DEFAULT_TAB_TITLE = "Loading PDF…";
    private static final int TAB_ID = 123;
    private static final String CONTENT_URL = "content://media/external/downloads/1000000022";
    private static final String FILE_URL = "file:///media/external/downloads/sample.pdf";
    private static final String PDF_LINK = "https://www.foo.com/testfiles/pdf/sample.pdf";
    private static final String PDF_BLOB_URL = "blob:https://www.foo.com/abc";
    private static final String EXAMPLE_URL = "https://www.example.com/";
    private static final String FILE_PATH = "/media/external/downloads/sample.pdf";
    private static final String FILE_NAME = "sample.pdf";

    @Before
    public void setUp() {
        PostTask.setPrenativeThreadPoolExecutorForTesting(Runnable::run);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            doReturn(activity).when(mMockNativePageHost).getContext();
                        });
        doReturn(mMarginSupplier).when(mMockNativePageHost).createDefaultMarginAdapter(any());
        mPdfInfo = new PdfInfo();
        ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(CONTENT_URL));
        PdfCoordinator.skipLoadPdfForTesting(true);
        mPdfPageUrl = PdfUtils.encodePdfPageUrl(PDF_LINK);
        mPdfPageBlobUrl = PdfUtils.encodePdfPageUrl(PDF_BLOB_URL);
        mUserDataHost = new UserDataHost();
        doReturn(mUserDataHost).when(mMockTab).getUserDataHost();
        doReturn(mMockProfile).when(mMockTab).getProfile();
        doReturn(TAB_ID).when(mMockTab).getId();
    }

    @After
    public void tearDown() throws Exception {
        ChromeFileProvider.setGeneratedUriForTesting(null);
        PdfCoordinator.skipLoadPdfForTesting(false);
    }

    @Test
    public void testCreatePdfPage_WithContentUri() throws Exception {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.AssistContent.IsWorkProfile", true)
                        .build();
        String encodedUrl = PdfUtils.encodePdfPageUrl(CONTENT_URL);
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        Assert.assertNotNull(pdfPage);
        Assert.assertEquals(
                "Pdf page host should match.", UrlConstants.PDF_HOST, pdfPage.getHost());
        Assert.assertEquals("Pdf page url should match.", encodedUrl, pdfPage.getUrl());
        Assert.assertFalse(
                "Pdf should not be loaded when the view is not attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        // Simulate tab brought from background to foreground
        View view = pdfPage.mPdfCoordinator.getView();
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(view);
        ShadowLooper.idleMainLooper();
        Assert.assertTrue(
                "Pdf should be loaded when the view is attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());
        String jsonString = pdfPage.requestAssistContent(/* isWorkProfile= */ true);
        Assert.assertNotNull(
                "Assist content should be generated when the pdf is ready to load", jsonString);
        JSONObject jsonObject = new JSONObject(jsonString);
        JSONObject metadata = (JSONObject) jsonObject.get(PdfCoordinator.JSON_KEY_FILE_METADATA);
        Assert.assertEquals(
                "File uri should match.",
                pdfPage.mPdfCoordinator.getUri().toString(),
                metadata.get(PdfCoordinator.JSON_KEY_FILE_URI));
        Assert.assertEquals(
                "File name should match.",
                pdfPage.getTitle(),
                metadata.get(PdfCoordinator.JSON_KEY_FILE_NAME));
        Assert.assertEquals(
                "Mime type should match.",
                MimeTypeUtils.PDF_MIME_TYPE,
                metadata.get(PdfCoordinator.JSON_KEY_MIME_TYPE));
        Assert.assertEquals(
                "Work profile should match.",
                true,
                metadata.get(PdfCoordinator.JSON_KEY_IS_WORK_PROFILE));
        histogramExpectation.assertExpected();

        Uri fileUri =
                pdfPage.getFileUri(
                        /* isWorkProfile= */ true, "com.google.android.googlequicksearchbox");
        Assert.assertNotNull("File uri should be generated when the pdf is ready to load", fileUri);
        Assert.assertEquals(
                "File uri should match.",
                pdfPage.mPdfCoordinator.getUri().toString(),
                fileUri.toString());

        contentView.removeView(view);
        pdfPage.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_HttpPdf() throws Exception {
        File tempFile = File.createTempFile("test_pdf", ".pdf");
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        mPdfPageUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        pdfPage.onDownloadComplete(FILE_NAME, tempFile.getAbsolutePath(), true);
        assertTrue("Transient file should exist before reload", tempFile.exists());

        pdfPage.reload();

        verify(mMockNativePageHost)
                .loadUrl(
                        argThat(
                                params ->
                                        params.getUrl().equals(PDF_LINK)
                                                && params.getShouldReplaceCurrentEntry()),
                        eq(false));
        assertFalse("Transient file should be deleted on reload", tempFile.exists());

        pdfPage.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_HttpPdf_RawUrl() throws Exception {
        File tempFile = File.createTempFile("test_pdf", ".pdf");
        // Test when PdfPage is initialized with raw HTTP(S) URL (e.g. after tab reparenting / drag
        // & drop)
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        PDF_LINK,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        pdfPage.onDownloadComplete(FILE_NAME, tempFile.getAbsolutePath(), true);
        assertTrue("Transient file should exist before reload", tempFile.exists());

        pdfPage.reload();

        verify(mMockNativePageHost)
                .loadUrl(
                        argThat(
                                params ->
                                        params.getUrl().equals(PDF_LINK)
                                                && params.getShouldReplaceCurrentEntry()),
                        eq(false));
        assertFalse("Transient file should be deleted on reload", tempFile.exists());

        pdfPage.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_ContentUri() throws Exception {
        String encodedUrl = PdfUtils.encodePdfPageUrl(CONTENT_URL);
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        Assert.assertNotNull(pdfPage);

        // Simulate tab brought from background to foreground to load PDF
        View view = pdfPage.mPdfCoordinator.getView();
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(view);
        ShadowLooper.idleMainLooper();
        Assert.assertTrue(
                "Pdf should be loaded when the view is attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        PdfCoordinator.ChromePdfViewerFragment oldFragment =
                ((PdfCoordinator) pdfPage.mPdfCoordinator).mChromePdfViewerFragment;
        Assert.assertNotNull("Fragment should not be null initially", oldFragment);

        pdfPage.reload();

        Assert.assertNotSame(
                "Fragment should be recreated",
                oldFragment,
                ((PdfCoordinator) pdfPage.mPdfCoordinator).mChromePdfViewerFragment);

        contentView.removeView(view);
        pdfPage.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDestroy_WebPdf_DeletesFile() throws Exception {
        File tempFile = File.createTempFile("test_pdf", ".pdf");
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        mPdfPageUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        pdfPage.onDownloadComplete(FILE_NAME, tempFile.getAbsolutePath(), true);
        assertTrue("Transient file should exist before destroy", tempFile.exists());

        pdfPage.destroy();

        assertFalse("Transient file should be deleted on destroy", tempFile.exists());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDestroy_PreservesFile_WhenTabDetachedFromActivity() throws Exception {
        doReturn(true).when(mMockTab).isDetachedFromActivity();
        doReturn(new GURL(mPdfPageUrl)).when(mMockTab).getUrl();
        File tempFile = File.createTempFile("test_pdf", ".pdf");
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        mPdfPageUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        pdfPage.onDownloadComplete(FILE_NAME, tempFile.getAbsolutePath(), true);
        assertTrue("Transient file should exist before destroy", tempFile.exists());

        pdfPage.destroy();

        assertTrue(
                "Transient file should be preserved when tab is transferred across activities",
                tempFile.exists());
        tempFile.delete();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDestroy_DeletesFile_WhenTabFrozen() throws Exception {
        doReturn(true).when(mMockTab).isFrozen();
        doReturn(new GURL(mPdfPageUrl)).when(mMockTab).getUrl();
        File tempFile = File.createTempFile("test_pdf", ".pdf");
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        mPdfPageUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        pdfPage.onDownloadComplete(FILE_NAME, tempFile.getAbsolutePath(), true);
        assertTrue("Transient file should exist before destroy", tempFile.exists());

        pdfPage.destroy();

        assertFalse("Transient file should be deleted when tab is frozen", tempFile.exists());
    }

    @Test
    public void testCreatePdfPage_WithFileUri() {
        String encodedUrl = PdfUtils.encodePdfPageUrl(FILE_URL);
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        Assert.assertNotNull(pdfPage);
        Assert.assertEquals("Pdf page title should match.", FILE_NAME, pdfPage.getTitle());
        Assert.assertEquals(
                "Pdf page host should match.", UrlConstants.PDF_HOST, pdfPage.getHost());
        Assert.assertEquals("Pdf page url should match.", encodedUrl, pdfPage.getUrl());
        Assert.assertFalse(
                "Pdf should not be loaded when the view is not attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        // Simulate tab brought from background to foreground
        View view = pdfPage.mPdfCoordinator.getView();
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(view);
        ShadowLooper.idleMainLooper();
        Assert.assertTrue(
                "Pdf should be loaded when the view is attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());
        contentView.removeView(view);
    }

    @Test
    public void testChangeZoomLevel() {
        String encodedUrl = PdfUtils.encodePdfPageUrl(CONTENT_URL);
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        Assert.assertNotNull(pdfPage);

        // Current zoom level is 1.0f.
        // Decrease zoom level (decrease = true) -> next level should be 0.9f, which is valid, so it
        // returns true.
        Assert.assertTrue(pdfPage.changeZoomLevel(/* decrease= */ true));

        // Simulate viewport update so the model reflects the current zoom level.
        ((PdfCoordinator) pdfPage.mPdfCoordinator).onViewportChanged(0, 0.9f);

        // Increase zoom level (decrease = false) -> next level should be 1.0f, which is valid, so
        // it returns true.
        Assert.assertTrue(pdfPage.changeZoomLevel(/* decrease= */ false));
    }

    @Test
    public void testCreatePdfPage_WithPdfLink_Https() throws Exception {
        testCreatePdfPage_WithPdfLink(mPdfPageUrl);
    }

    @Test
    public void testCreatePdfPage_WithPdfLink_Blob() throws Exception {
        testCreatePdfPage_WithPdfLink(mPdfPageBlobUrl);
    }

    // Test the current NativePage for a local pdf is reused for various pdf URLs.
    @Test
    @EnableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
    public void testShouldReusePage_LocalPdf() throws Exception {
        String encodedUrl = PdfUtils.encodePdfPageUrl(CONTENT_URL);
        String currentUrl = encodedUrl;
        String nextUrl = "content://media/external/downloads/1000000088";
        String nextUrlExternal = PDF_LINK;
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        assertTrue(
                "Entering a local pdf URL should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrl, /* preferReuse= */ true));
        assertTrue(
                "Navigating to a local pdf should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrl, /* preferReuse= */ false));
        assertFalse(
                "Reloading a local pdf URL on activity restart should create a new the page",
                pdfPage.shouldReusePage(currentUrl, currentUrl, /* preferReuse= */ false));

        assertTrue(
                "Entering an external pdf URL should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrlExternal, /* preferReuse= */ true));
        assertTrue(
                "Navigating to an external pdf should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrlExternal, /* preferReuse= */ false));
    }

    // Test the current NativePage for an external pdf is reused for various pdf URLs.
    @Test
    @EnableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
    public void testShouldReusePage_ExternalPdf() throws Exception {
        String encodedUrl = PdfUtils.encodePdfPageUrl(PDF_LINK);
        String currentUrl = encodedUrl;
        String nextUrlLocal = "content://media/external/downloads/1000000088";
        String nextUrlExternal = "https://abc.xyz/report.pdf";
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        assertTrue(
                "Entering a local pdf URL should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrlLocal, /* preferReuse= */ true));
        assertTrue(
                "Navigating to a local pdf should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrlLocal, /* preferReuse= */ false));
        assertFalse(
                "Reloading the current pdf URL on activity restart should create a new the page",
                pdfPage.shouldReusePage(currentUrl, PDF_LINK, /* preferReuse= */ false));

        assertTrue(
                "Entering an external pdf URL should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrlExternal, /* preferReuse= */ true));
        assertTrue(
                "Navigating to an external pdf should reuse the page",
                pdfPage.shouldReusePage(currentUrl, nextUrlExternal, /* preferReuse= */ false));
    }

    private void testCreatePdfPage_WithPdfLink(String pdfPageUrl) throws Exception {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.Pdf.AssistContent.IsWorkProfile", false)
                        .build();
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        pdfPageUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);
        Assert.assertNotNull(pdfPage);
        Assert.assertFalse(
                "Pdf should not be loaded when the download is not completed.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());
        Assert.assertNull(
                "Assist content cannot be generated when the pdf is not ready to load",
                pdfPage.requestAssistContent(/* isWorkProfile= */ false));

        // Simulate download complete
        pdfPage.onDownloadComplete(FILE_NAME, FILE_PATH, true);
        Assert.assertEquals("Pdf page title should match.", FILE_NAME, pdfPage.getTitle());
        Assert.assertEquals(
                "Pdf page host should match.", UrlConstants.PDF_HOST, pdfPage.getHost());
        Assert.assertEquals("Pdf page url should match.", pdfPageUrl, pdfPage.getUrl());
        Assert.assertFalse(
                "Pdf should not be loaded when the view is not attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        // Simulate tab brought from background to foreground
        View view = pdfPage.mPdfCoordinator.getView();
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(view);
        ShadowLooper.idleMainLooper();
        Assert.assertTrue(
                "Pdf should be loaded when the view is attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());
        String jsonString = pdfPage.requestAssistContent(/* isWorkProfile= */ false);
        Assert.assertNotNull(
                "Assist content should be generated when the pdf is ready to load", jsonString);
        JSONObject jsonObject = new JSONObject(jsonString);
        JSONObject metadata = (JSONObject) jsonObject.get(PdfCoordinator.JSON_KEY_FILE_METADATA);
        Assert.assertEquals(
                "File uri should match.",
                pdfPage.mPdfCoordinator.getUri().toString(),
                metadata.get(PdfCoordinator.JSON_KEY_FILE_URI));
        Assert.assertEquals(
                "File name should match.",
                pdfPage.getTitle(),
                metadata.get(PdfCoordinator.JSON_KEY_FILE_NAME));
        Assert.assertEquals(
                "Mime type should match.",
                MimeTypeUtils.PDF_MIME_TYPE,
                metadata.get(PdfCoordinator.JSON_KEY_MIME_TYPE));
        Assert.assertEquals(
                "Work profile should match.",
                false,
                metadata.get(PdfCoordinator.JSON_KEY_IS_WORK_PROFILE));
        histogramExpectation.assertExpected();

        Uri fileUri =
                pdfPage.getFileUri(
                        /* isWorkProfile= */ false, "com.google.android.googlequicksearchbox");
        Assert.assertNotNull("File uri should be generated when the pdf is ready to load", fileUri);
        Assert.assertEquals(
                "File uri should match.",
                pdfPage.mPdfCoordinator.getUri().toString(),
                fileUri.toString());

        contentView.removeView(view);
        pdfPage.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
    public void testUpdateForUrl_NonLocalPdf_ResetsLoadState() throws Exception {
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        mPdfPageUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);

        pdfPage.onDownloadComplete(FILE_NAME, FILE_PATH, true);
        View view = pdfPage.mPdfCoordinator.getView();
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(view);
        ShadowLooper.idleMainLooper();

        Assert.assertTrue(
                "Pdf should be loaded after download complete and attached.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        pdfPage.updateForUrl(mPdfPageUrl);

        Assert.assertFalse(
                "Pdf load state should be reset for non-local pdf in updateForUrl.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        pdfPage.onDownloadComplete(FILE_NAME, FILE_PATH, true);
        ShadowLooper.idleMainLooper();

        Assert.assertTrue(
                "Pdf should be loaded after new download complete.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        contentView.removeView(view);
        pdfPage.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
    public void testUpdateForUrl_LocalPdf_ResetsLoadState() throws Exception {
        String encodedUrl = PdfUtils.encodePdfPageUrl(CONTENT_URL);
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockTab,
                        mActivity,
                        encodedUrl,
                        mPdfInfo,
                        DEFAULT_TAB_TITLE,
                        mPdfFragmentViewTracker);

        View view = pdfPage.mPdfCoordinator.getView();
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(view);
        ShadowLooper.idleMainLooper();

        Assert.assertTrue(
                "Pdf should be loaded when attached to window.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        pdfPage.updateForUrl(encodedUrl);
        ShadowLooper.idleMainLooper();

        Assert.assertTrue(
                "Pdf should be reloaded after updateForUrl on local PDF.",
                ((PdfCoordinator) pdfPage.mPdfCoordinator).getIsPdfLoadedForTesting());

        contentView.removeView(view);
        pdfPage.destroy();
    }
}
