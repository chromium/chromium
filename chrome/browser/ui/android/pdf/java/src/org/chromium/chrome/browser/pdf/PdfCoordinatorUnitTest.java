// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;
import android.view.ViewGroup;

import androidx.fragment.app.FragmentActivity;
import androidx.pdf.PdfDocument;
import androidx.pdf.PdfDocument.PageInfo;
import androidx.pdf.PdfPoint;
import androidx.pdf.view.PdfView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import kotlin.coroutines.Continuation;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
@Config(sdk = 35)
public class PdfCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mNativePageHost;
    @Mock private Profile mProfile;
    @Mock private PdfFragmentViewTracker mPdfFragmentViewTracker;

    private FragmentActivity mActivity;
    private PdfCoordinator mPdfCoordinator;
    private PdfView mPdfView;
    private static final String PDF_URL =
            "chrome-native://pdf/link?url=https%3A%2F%2Fwww.irs.gov%2Fpub%2Firs-pdf%2Ffw4.pdf";
    private static final String PDF_TITLE = "fw4.pdf";
    private static final String LINK_URL = "https://www.bar.com";
    private static final String FILE_PATH =
            "/data/user/10/com.google.android.apps.chrome/cache/pdfs/fw4.pdf";
    private static final int TAB_ID = 123;
    private static final int PDF_CONTENT_HEIGHT = 1000;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        PdfCoordinator.skipLoadPdfForTesting(true);
        ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(PDF_URL));
    }

    @After
    public void tearDown() {
        ChromeFileProvider.setGeneratedUriForTesting(null);
    }

    private void createPdfCoordinator() {
        // For the purpose of testing, we are using the transient file path and url above when in
        // reality, the file path will not be available for a transient pdf when this constructor
        // is called.
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        FILE_PATH,
                        PDF_TITLE,
                        TAB_ID,
                        PDF_URL,
                        mPdfFragmentViewTracker);
        mPdfView = new PdfView(mActivity);
        mPdfView.layout(0, 0, /* width= */ 500, /* height= */ PDF_CONTENT_HEIGHT);
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(mPdfView);
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(mPdfCoordinator.getView());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_RegularProfile() {
        runOnLinkClickedTest(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_Incognito() {
        runOnLinkClickedTest(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testNavigateToPage() {
        createPdfCoordinator();
        int pageIndex = 2;

        // Test
        mPdfCoordinator.navigateToPage(pageIndex);

        // Assert
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        float expectedYOffsetPoints = (PDF_CONTENT_HEIGHT / 2f) / shadowPdfView.mZoom;
        assertEquals(new PdfPoint(pageIndex, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testNavigateToPage_PdfViewNull() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(null);

        // Verify that no exception is thrown when mPdfView is null.
        mPdfCoordinator.navigateToPage(2);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testChangeZoomLevel() {
        createPdfCoordinator();
        float zoomLevel = 2.0f;

        // Test
        mPdfCoordinator.changeZoomLevel(zoomLevel);

        // Assert
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertEquals(zoomLevel, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testChangeZoomLevel_PdfViewNull() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(null);

        // Verify that no exception is thrown when mPdfView is null.
        mPdfCoordinator.changeZoomLevel(2.0f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_RejectsDangerousSchemes() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();

        String[] blockedUris = {
            "javascript:alert('XSS-from-PDF')",
            "intent://scan/#Intent;scheme=zxing;package=com.evil.app;end",
            "file:///etc/hosts",
            "content://com.android.contacts/contacts",
            "chrome://settings/",
            "chrome-untrusted://feedback/",
            "devtools://devtools/bundled/inspector.html",
            "data:text/html,<script>alert(1)</script>",
            "about:blank",
            "market://details?id=com.evil.app",
        };

        for (String raw : blockedUris) {
            assertFalse(
                    "onLinkClicked should reject " + raw,
                    mPdfCoordinator.onLinkClicked(Uri.parse(raw)));
        }
        verify(mNativePageHost, never()).loadUrl(any(LoadUrlParams.class), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_RejectsSchemelessUri() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();

        assertFalse(
                "onLinkClicked should reject schemeless URI.",
                mPdfCoordinator.onLinkClicked(Uri.parse("//www.example.com/foo")));
        verify(mNativePageHost, never()).loadUrl(any(LoadUrlParams.class), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_AcceptsAllowedSchemes() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();

        String[] allowedUris = {
            "http://www.example.com/",
            "https://www.example.com/",
            "HTTPS://MixedCase.Example.com/",
            "mailto:user@example.com",
            "tel:+10000000000",
            "ftp://ftp.example.com/file",
        };

        for (String raw : allowedUris) {
            assertTrue(
                    "onLinkClicked should accept " + raw,
                    mPdfCoordinator.onLinkClicked(Uri.parse(raw)));
        }
        verify(mNativePageHost, times(allowedUris.length))
                .loadUrl(any(LoadUrlParams.class), eq(false));
    }

    private void runOnLinkClickedTest(boolean isIncognito) {
        when(mProfile.isOffTheRecord()).thenReturn(isIncognito);
        createPdfCoordinator();
        Uri linkUri = Uri.parse(LINK_URL);
        boolean result = mPdfCoordinator.onLinkClicked(linkUri);
        assertTrue("name should verify true", result);
        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mNativePageHost).loadUrl(captor.capture(), eq(isIncognito));
        LoadUrlParams params = captor.getValue();
        assertEquals("URL should match.", LINK_URL, params.getUrl());
        assertEquals(
                "Transition type should be LINK.", PageTransition.LINK, params.getTransitionType());
        assertTrue("isRendererInitiated should be true.", params.getIsRendererInitiated());
        assertEquals(
                Origin.create(new GURL(PDF_URL)).toString(),
                params.getInitiatorOrigin().toString());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testGetFileUri() {
        createPdfCoordinator();

        Uri uri =
                mPdfCoordinator.getFileUri(
                        /* isWorkProfile= */ false, "com.google.android.googlequicksearchbox");
        assertNotNull(uri);
        assertEquals(mPdfCoordinator.getUri(), uri);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testGetFileUri_NullUri() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        // Signature: NativePageHost, Profile, Activity, @Nullable String filepath, String title,
        // int tabId, String url
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        null,
                        PDF_TITLE,
                        TAB_ID,
                        PDF_URL,
                        mPdfFragmentViewTracker);

        Uri uri =
                mPdfCoordinator.getFileUri(
                        /* isWorkProfile= */ false, "com.google.android.googlequicksearchbox");
        assertEquals(null, uri);
    }
    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testCalculateFitToPageZoom() {
        createPdfCoordinator();

        // Use real PageInfo since it is a final class (cannot mock). Pass empty list for
        // FormWidgetInfo.
        androidx.pdf.PdfDocument.PageInfo realPageInfo =
                new androidx.pdf.PdfDocument.PageInfo(
                        0, 400, 200, java.util.Collections.emptyList());

        // mPdfView width = 500, height = 1000
        // Fit to page height
        float zoomHeight =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        realPageInfo, true, mPdfView, /* zoomRatio= */ 1.0f);
        // viewportSize = 1000, contentSize = 400. zoom = 1000 / 400 = 2.5f
        assertEquals(2.5f, zoomHeight, 0.001f);

        // Fit to page width
        float zoomWidth =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        realPageInfo, false, mPdfView, /* zoomRatio= */ 1.0f);
        // viewportSize = 500, contentSize = 200. zoom = 500 / 200 = 2.5f
        assertEquals(2.5f, zoomWidth, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testToggleFitToPage_PdfViewNull() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(null);
        // Should return gracefully without NullPointerException.
        mPdfCoordinator.toggleFitToPage(true, 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testToggleTwoPagesPerRow() {
        createPdfCoordinator();
        float zoomLevel = 1.5f;
        int currentPageIndex = 2;

        // Test toggling to two pages per row.
        mPdfCoordinator.toggleTwoPagesPerRow(true, zoomLevel, currentPageIndex);

        // Assert
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertEquals(2, shadowPdfView.mPagesPerRow);
        assertEquals(zoomLevel, shadowPdfView.mZoom, 0.001f);

        float expectedYOffsetPoints = (PDF_CONTENT_HEIGHT / 2f) / zoomLevel;
        assertEquals(
                new PdfPoint(currentPageIndex, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);

        // Test toggling back to one page per row.
        mPdfCoordinator.toggleTwoPagesPerRow(false, zoomLevel, currentPageIndex);

        // Assert
        assertEquals(1, shadowPdfView.mPagesPerRow);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testToggleTwoPagesPerRow_PdfViewNull() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(null);

        // Verify that no exception is thrown when mPdfView is null.
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.5f, 2);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testCalculateFitToPageZoomWithRatio() {
        createPdfCoordinator();

        androidx.pdf.PdfDocument.PageInfo realPageInfo =
                new androidx.pdf.PdfDocument.PageInfo(
                        0, 400, 200, java.util.Collections.emptyList());

        // mPdfView width = 500, height = 1000
        // Fit to page width with 0.8 ratio
        float zoomWidthRatio =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        realPageInfo, false, mPdfView, 0.8f);
        // viewportSize = 500, contentSize = 200. zoom = (500 * 0.8) / 200 = 400 / 200 = 2.0f
        assertEquals(2.0f, zoomWidthRatio, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testDefaultZoomLargeViewport() {
        // Need to create coordinator with larger width
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        FILE_PATH,
                        PDF_TITLE,
                        TAB_ID,
                        PDF_URL,
                        mPdfFragmentViewTracker);
        mPdfView = new PdfView(mActivity);
        mPdfView.layout(0, 0, /* width= */ 1000, /* height= */ PDF_CONTENT_HEIGHT);
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(mPdfView);
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(mPdfCoordinator.getView());
        contentView.addView(mPdfView);

        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        PdfDocument mockPdfDocument =
                (PdfDocument)
                        java.lang.reflect.Proxy.newProxyInstance(
                                PdfDocument.class.getClassLoader(),
                                new Class[] {PdfDocument.class},
                                (proxy, method, args) -> {
                                    if (method.getName().equals("getPageInfo")
                                            && args != null
                                            && args.length == 2) {
                                        Continuation<PageInfo> continuation =
                                                (Continuation<PageInfo>) args[1];
                                        PageInfo realPageInfo =
                                                new PageInfo(
                                                        0,
                                                        400,
                                                        200,
                                                        java.util.Collections.emptyList());
                                        continuation.resumeWith(realPageInfo);
                                        return null;
                                    }
                                    Class<?> returnType = method.getReturnType();
                                    if (returnType.equals(Void.TYPE)) return null;
                                    if (returnType.equals(Boolean.TYPE)) return false;
                                    if (returnType.equals(Integer.TYPE)) return 0;
                                    if (returnType.equals(Long.TYPE)) return 0L;
                                    if (returnType.equals(Float.TYPE)) return 0f;
                                    return null;
                                });
        shadowPdfView.mPdfDocument = mockPdfDocument;

        // Trigger default zoom
        mPdfCoordinator.onViewportChanged(0, 3.76f);

        // Since setDefaultZoom posts to pdfView, we must idle the looper.
        ShadowLooper.idleMainLooper();

        // viewportWidth = 1000. contentWidth = 200.
        // expectedZoom = (1000 * 0.8) / 200 = 800 / 200 = 4.0f
        assertEquals(4.0f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLoadDocumentError_MakesContainerVisible() {
        createPdfCoordinator();

        android.view.View container =
                mPdfCoordinator.getView().findViewById(mPdfCoordinator.mFragmentContainerViewId);
        assertEquals(android.view.View.INVISIBLE, container.getVisibility());

        // Set document load start timestamp to simulate that load started.
        mPdfCoordinator.mChromePdfViewerFragment.mDocumentLoadStartTimestamp = 12345L;

        // Trigger error.
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentError(
                new RuntimeException("Test error"));

        // Verify container is now VISIBLE.
        assertEquals(android.view.View.VISIBLE, container.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testPrint() {
        createPdfCoordinator();
        mPdfCoordinator.print();
        verify(mNativePageHost).print();
    }

    @Implements(PdfView.class)
    public static class ShadowPdfView extends ShadowView {
        public PdfPoint mPdfPoint;
        public float mZoom = 1.0f;
        public PdfDocument mPdfDocument;
        public int mPagesPerRow = 1;

        public ShadowPdfView() {}

        @Implementation
        public void scrollToPosition(PdfPoint pdfPoint) {
            mPdfPoint = pdfPoint;
        }

        @Implementation
        public void setZoom(float zoomLevel) {
            mZoom = zoomLevel;
        }

        @Implementation
        public void setPagesPerRow(int pagesPerRow) {
            mPagesPerRow = pagesPerRow;
        }

        @Implementation
        public float getZoom() {
            return mZoom;
        }

        @Implementation
        public float getMinZoom() {
            return 0.1f;
        }

        @Implementation
        public float getMaxZoom() {
            return 25.0f;
        }

        @Implementation
        public PdfDocument getPdfDocument() {
            return mPdfDocument;
        }
    }
}
