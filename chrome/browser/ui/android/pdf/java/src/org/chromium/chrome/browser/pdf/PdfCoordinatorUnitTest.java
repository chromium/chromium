// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;
import androidx.pdf.PdfDocument;
import androidx.pdf.PdfDocument.PageInfo;
import androidx.pdf.PdfPoint;
import androidx.pdf.PdfWriteHandle;
import androidx.pdf.ink.EditablePdfViewerFragment;
import androidx.pdf.view.PdfView;
import androidx.pdf.viewer.fragment.PdfViewerFragment;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import kotlin.coroutines.Continuation;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.TriState;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfHyperlinkClickResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Proxy;

@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
@Config(
        sdk = 35,
        instrumentedPackages = {"androidx.fragment.app", "androidx.pdf"},
        shadows = {
            PdfCoordinatorUnitTest.ShadowPdfViewerFragment.class,
            PdfCoordinatorUnitTest.ShadowEditablePdfViewerFragment.class
        })
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
    private static final String TEST_CONTENT_URI =
            "content://com.android.chrome.provider/fw4.pdf";
    private static final int TAB_ID = 123;
    private static final int PDF_CONTENT_HEIGHT = 1000;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        PdfCoordinator.skipLoadPdfForTesting(true);
        PdfUtils.setInlinePdfV2EditEnabledForTesting(true);
        ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(TEST_CONTENT_URI));
        PostTask.setPrenativeThreadPoolExecutorForTesting(Runnable::run);
    }

    @After
    public void tearDown() {
        ChromeFileProvider.setGeneratedUriForTesting(null);
        PostTask.setPrenativeThreadPoolExecutorForTesting(null);
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
        ShadowLooper.idleMainLooper();
        if (mPdfCoordinator.getUri() != null) {
            mPdfCoordinator.mChromePdfViewerFragment.setDocumentUri(mPdfCoordinator.getUri());
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked() {
        createPdfCoordinator();
        Uri linkUri = Uri.parse(LINK_URL);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.Hyperlink.ClickResult",
                        PdfHyperlinkClickResult.SUCCESS_LOAD_INITIATED);
        boolean result = mPdfCoordinator.onLinkClicked(linkUri);
        assertTrue("name should verify true", result);
        histogramExpectation.assertExpected();
        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mNativePageHost).openNewTab(captor.capture());
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
    @Config(shadows = {ShadowPdfView.class})
    public void testNavigateToPage() {
        createPdfCoordinator();
        int pageIndex = 2;

        // Test
        mPdfCoordinator.navigateToPage(pageIndex);

        // Assert
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        float expectedYOffsetPoints = (mPdfView.getHeight() / 2f) / shadowPdfView.mZoom;
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
    @Config(shadows = {ShadowPdfView.class})
    public void testChangeZoomLevel_Boolean() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);

        // Current zoom level is 1.0f.
        // Decrease zoom level (decrease = true) -> should go to 0.9f.
        assertTrue(mPdfCoordinator.changeZoomLevel(/* decrease= */ true));
        assertEquals(0.9f, shadowPdfView.mZoom, 0.001f);

        // Simulate viewport update so the model reflects the current zoom level.
        mPdfCoordinator.onViewportChanged(0, 0.9f);

        // Increase zoom level (decrease = false) -> should go back to 1.0f.
        assertTrue(mPdfCoordinator.changeZoomLevel(/* decrease= */ false));
        assertEquals(1.0f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testResetZoomLevel() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);

        // Set default zoom level.
        mPdfCoordinator.getToolbarCoordinatorForTesting().setDefaultZoomLevel(1.5f);

        // Zoom into some other level.
        mPdfCoordinator.changeZoomLevel(2.0f);
        assertEquals(2.0f, shadowPdfView.mZoom, 0.001f);

        // Reset zoom level.
        assertTrue(mPdfCoordinator.resetZoomLevel());
        assertEquals(1.5f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testResetZoomLevel_ToolbarNull() {
        createPdfCoordinator();
        assertNull(mPdfCoordinator.getToolbarCoordinatorForTesting());
        assertFalse(mPdfCoordinator.resetZoomLevel());
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
            HistogramWatcher histogramExpectation =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Android.Pdf.Hyperlink.ClickResult",
                            PdfHyperlinkClickResult.BLOCKED_INVALID_SCHEME);
            assertFalse(
                    "onLinkClicked should reject " + raw,
                    mPdfCoordinator.onLinkClicked(Uri.parse(raw)));
            histogramExpectation.assertExpected();
        }
        verify(mNativePageHost, never()).openNewTab(any(LoadUrlParams.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_RejectsSchemelessUri() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.Hyperlink.ClickResult",
                        PdfHyperlinkClickResult.BLOCKED_INVALID_SCHEME);
        assertFalse(
                "onLinkClicked should reject schemeless URI.",
                mPdfCoordinator.onLinkClicked(Uri.parse("//www.example.com/foo")));
        histogramExpectation.assertExpected();
        verify(mNativePageHost, never()).openNewTab(any(LoadUrlParams.class));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLinkClicked_V2Disabled() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.Hyperlink.ClickResult",
                        PdfHyperlinkClickResult.IGNORED_V2_DISABLED);
        assertFalse(
                "onLinkClicked should return false when inline PDF V2 is disabled.",
                mPdfCoordinator.onLinkClicked(Uri.parse("https://www.example.com/")));
        histogramExpectation.assertExpected();
        verify(mNativePageHost, never()).openNewTab(any(LoadUrlParams.class));
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
            HistogramWatcher histogramExpectation =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Android.Pdf.Hyperlink.ClickResult",
                            PdfHyperlinkClickResult.SUCCESS_LOAD_INITIATED);
            assertTrue(
                    "onLinkClicked should accept " + raw,
                    mPdfCoordinator.onLinkClicked(Uri.parse(raw)));
            histogramExpectation.assertExpected();
        }
        verify(mNativePageHost, times(allowedUris.length)).openNewTab(any(LoadUrlParams.class));
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
        // mPdfView width = 500, height = 1000

        // 1. Equal aspect ratio: content 400 height, 200 width
        // zoomHeight = 1000 / 400 = 2.5f, zoomWidth = 500 / 200 = 2.5f
        androidx.pdf.PdfDocument.PageInfo equalPageInfo =
                new androidx.pdf.PdfDocument.PageInfo(
                        0, 400, 200, java.util.Collections.emptyList());
        float zoomPage =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        equalPageInfo, /* fitToPage= */ true, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(2.5f, zoomPage, 0.001f);

        // 2. Tall page (height-constrained): content 800 height, 200 width
        // zoomHeight = 1000 / 800 = 1.25f, zoomWidth = 500 / 200 = 2.5f -> min = 1.25f
        androidx.pdf.PdfDocument.PageInfo tallPageInfo =
                new androidx.pdf.PdfDocument.PageInfo(
                        0, 800, 200, java.util.Collections.emptyList());
        float zoomTallPage =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        tallPageInfo, /* fitToPage= */ true, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(1.25f, zoomTallPage, 0.001f);

        // 3. Wide page (width-constrained): content 400 height, 400 width
        // zoomHeight = 1000 / 400 = 2.5f, zoomWidth = 500 / 400 = 1.25f -> min = 1.25f
        androidx.pdf.PdfDocument.PageInfo widePageInfo =
                new androidx.pdf.PdfDocument.PageInfo(
                        0, 400, 400, java.util.Collections.emptyList());
        float zoomWidePage =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        widePageInfo, /* fitToPage= */ true, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(1.25f, zoomWidePage, 0.001f);

        // 4. Fit to width: content 800 height, 200 width -> zoom = 500 / 200 = 2.5f
        float zoomWidth =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        tallPageInfo, /* fitToPage= */ false, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(2.5f, zoomWidth, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testCalculateFitToPageZoom_TwoPagesPerRow() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.setPagesPerRow(true);

        // mPdfView width = 500, height = 1000
        // Tall page: content width = 200, height = 800
        // Two pages per row -> total width = 200 * 2 = 400
        androidx.pdf.PdfDocument.PageInfo tallPageInfo =
                new androidx.pdf.PdfDocument.PageInfo(
                        0, 800, 200, java.util.Collections.emptyList());

        // 1. Fit to width: total content width = 400 -> zoom = 500 / 400 = 1.25f
        float zoomWidth =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        tallPageInfo, /* fitToPage= */ false, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(1.25f, zoomWidth, 0.001f);

        // 2. Fit to page: zoomWidth = 500 / 400 = 1.25f, zoomHeight = 1000 / 800 = 1.25f => min = 1.25f
        float zoomPage =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        tallPageInfo, /* fitToPage= */ true, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(1.25f, zoomPage, 0.001f);
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

        float expectedYOffsetPoints = (mPdfView.getHeight() / 2f) / zoomLevel;
        assertEquals(
                new PdfPoint(currentPageIndex, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);

        // Test toggling back to one page per row.
        mPdfCoordinator.toggleTwoPagesPerRow(false, zoomLevel, currentPageIndex);

        // Assert
        assertEquals(1, shadowPdfView.mPagesPerRow);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testToggleTwoPagesPerRow_FitActive() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // First, toggle Fit to Width (fitToPage = false) in single page mode:
        // viewportWidth = 500, contentWidth = 200 => zoom = 500 / 200 = 2.5f
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ false, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(2.5f, shadowPdfView.mZoom, 0.001f);

        // When toggling to two pages per row with fit active, it should re-fit for two pages:
        // total content width = 200 * 2 = 400 => zoom = 500 / 400 = 1.25f (not 2.5f)
        mPdfCoordinator.toggleTwoPagesPerRow(true, 2.5f, 0);
        ShadowLooper.idleMainLooper();

        assertEquals(2, shadowPdfView.mPagesPerRow);
        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testResetLoadState_ResetsTwoPagesPerRow() {
        createPdfCoordinator();
        mPdfCoordinator.setIsFitToPageActiveForTesting(TriState.TRUE);
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.5f, 2);
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertEquals(2, shadowPdfView.mPagesPerRow);

        mPdfCoordinator.resetLoadState();

        assertEquals(1, shadowPdfView.mPagesPerRow);
        assertEquals(TriState.NOT_SET, mPdfCoordinator.getIsFitToPageActiveForTesting());
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
    @Config(shadows = {ShadowPdfView.class})
    public void testToggleTwoPagesPerRow_negativePageIndex() {
        createPdfCoordinator();
        float zoomLevel = 1.5f;

        // Negative page index should be clamped to 0 and not throw IllegalArgumentException.
        mPdfCoordinator.toggleTwoPagesPerRow(true, zoomLevel, -1);

        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertEquals(2, shadowPdfView.mPagesPerRow);
        float expectedYOffsetPoints = (mPdfView.getHeight() / 2f) / zoomLevel;
        assertEquals(new PdfPoint(0, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testToggleFitToPage_negativePageIndex() {
        createPdfCoordinator();
        boolean[] getPageInfoCalled = new boolean[1];
        PdfDocument mockPdfDocument =
                (PdfDocument)
                        Proxy.newProxyInstance(
                                PdfDocument.class.getClassLoader(),
                                new Class[] {PdfDocument.class},
                                (proxy, method, args) -> {
                                    if (method.getName().equals("getPageInfo")
                                            && args != null
                                            && args.length == 2) {
                                        // Verify page index -1 was clamped to 0.
                                        assertEquals(0, args[0]);
                                        getPageInfoCalled[0] = true;
                                        return null;
                                    }
                                    if (method.getName().equals("getPageCount")) {
                                        return 5;
                                    }
                                    Class<?> returnType = method.getReturnType();
                                    if (returnType.equals(Void.TYPE)) return null;
                                    if (returnType.equals(Boolean.TYPE)) return false;
                                    if (returnType.equals(Integer.TYPE)) return 0;
                                    if (returnType.equals(Long.TYPE)) return 0L;
                                    if (returnType.equals(Float.TYPE)) return 0f;
                                    return null;
                                });
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        shadowPdfView.mPdfDocument = mockPdfDocument;

        // Negative page index should be clamped to 0 and not throw an exception.
        mPdfCoordinator.toggleFitToPage(true, -1);
        assertTrue("getPageInfo should be called with clamped index 0", getPageInfoCalled[0]);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testToggleFitToPage() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 800, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // Toggle Fit to Page (fitToPage = true) for page index 2 (height 800, width 200).
        // viewportWidth = 500, viewportHeight = 1000
        // zoomWidth = 500 / 200 = 2.5f, zoomHeight = 1000 / 800 = 1.25f => min = 1.25f
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ true, 2);
        ShadowLooper.idleMainLooper();

        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        float expectedYOffsetPoints = (mPdfView.getHeight() / 2f) / 1.25f;
        assertEquals(new PdfPoint(2, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testToggleFitToPage_FitToWidth() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // Toggle Fit to Width (fitToPage = false) for page index 2.
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ false, 2);
        ShadowLooper.idleMainLooper();

        // viewportWidth = 500, contentWidth = 200 => zoom = 500 / 200 = 2.5f
        assertEquals(2.5f, shadowPdfView.mZoom, 0.001f);
        float expectedYOffsetPoints = (mPdfView.getHeight() / 2f) / 2.5f;
        assertEquals(new PdfPoint(2, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);
        assertEquals(TriState.FALSE, mPdfCoordinator.getIsFitToPageActiveForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testToggleFitToPage_TwoPagesPerRow_FitToWidth() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // Enable two-page view first
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);

        // Toggle Fit to Width (fitToPage = false) for page index 0.
        // In two-page view, total content width = 200 * 2 = 400.
        // viewportWidth = 500, totalWidth = 400 => zoom = 500 / 400 = 1.25f
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ false, 0);
        ShadowLooper.idleMainLooper();

        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.FALSE, mPdfCoordinator.getIsFitToPageActiveForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testToggleFitToPage_TwoPagesPerRow_FitToPage() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 800, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // Enable two-page view first
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);

        // Toggle Fit to Page (fitToPage = true) for page index 0.
        // In two-page view, total width = 200 * 2 = 400, height = 800.
        // viewportWidth = 500, viewportHeight = 1000.
        // zoomWidth = 500 / 400 = 1.25f, zoomHeight = 1000 / 800 = 1.25f => min = 1.25f
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ true, 0);
        ShadowLooper.idleMainLooper();

        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.TRUE, mPdfCoordinator.getIsFitToPageActiveForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testChangeZoomLevel_ClearsFitActive() {
        createPdfCoordinator();
        mPdfCoordinator.setIsFitToPageActiveForTesting(TriState.TRUE);
        assertEquals(TriState.TRUE, mPdfCoordinator.getIsFitToPageActiveForTesting());

        mPdfCoordinator.changeZoomLevel(1.5f);

        assertEquals(TriState.NOT_SET, mPdfCoordinator.getIsFitToPageActiveForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testTwoPageView_FitToWidth_PinchZoom_ToggleSinglePageView_PreservesPinchZoom() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // 1. User enables two-page view.
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);

        // 2. User enables fit to width (fitToPage = false).
        // Two pages: 200 * 2 = 400 content width in 500 viewport -> zoom = 500 / 400 = 1.25f.
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ false, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.FALSE, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 3. User manually pinches to zoom (e.g. zooms in to 1.8f).
        // Viewport listener triggers onViewportChanged with the new zoom.
        mPdfCoordinator.onViewportChanged(0, 1.8f);

        // Fit active is now cleared because zoom was manually adjusted.
        assertEquals(TriState.NOT_SET, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 4. User enables single page view.
        // The manual pinched zoom (1.8f) must be maintained!
        mPdfCoordinator.toggleTwoPagesPerRow(false, 1.8f, 0);
        ShadowLooper.idleMainLooper();

        assertEquals(1, shadowPdfView.mPagesPerRow);
        assertEquals(1.8f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testTwoPageView_FitToWidth_ToggleSinglePageView_RefitsToWidth() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // 1. User enables two-page view.
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);

        // 2. User enables fit to width (fitToPage = false).
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ false, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.FALSE, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 3. User scrolls (viewport changed with SAME zoom level).
        mPdfCoordinator.onViewportChanged(1, 1.25f);
        assertEquals(TriState.FALSE, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 4. User enables single page view (fit is still active).
        // It should re-fit to single-page width: 500 / 200 = 2.5f.
        mPdfCoordinator.toggleTwoPagesPerRow(false, 1.25f, 1);
        ShadowLooper.idleMainLooper();

        assertEquals(1, shadowPdfView.mPagesPerRow);
        assertEquals(2.5f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testTwoPageView_FitToPage_ToggleSinglePageView_RefitsToPage() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        // Page: height 800, width 200. Viewport: width 500, height 1000.
        setupMockPdfDocumentForPageInfo(shadowPdfView, 800, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // 1. User enables two-page view.
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);

        // 2. User enables fit to page (fitToPage = true).
        // Two pages: total width 200 * 2 = 400, height 800.
        // zoomWidth = 500 / 400 = 1.25f, zoomHeight = 1000 / 800 = 1.25f => zoom = 1.25f.
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ true, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.TRUE, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 3. User enables single page view without pinch zooming.
        // Single page: width 200, height 800.
        // zoomWidth = 500 / 200 = 2.5f, zoomHeight = 1000 / 800 = 1.25f => min = 1.25f (100% viewport height).
        mPdfCoordinator.toggleTwoPagesPerRow(false, 1.25f, 0);
        ShadowLooper.idleMainLooper();

        assertEquals(1, shadowPdfView.mPagesPerRow);
        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.TRUE, mPdfCoordinator.getIsFitToPageActiveForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testTwoPageView_FitToPage_PinchZoom_ToggleSinglePageView_PreservesPinchZoom() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 800, 200);
        mPdfCoordinator.mIsInitialZoomPass = false;

        // 1. User enables two-page view.
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);

        // 2. User enables fit to page (fitToPage = true).
        mPdfCoordinator.toggleFitToPage(/* fitToPage= */ true, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(1.25f, shadowPdfView.mZoom, 0.001f);
        assertEquals(TriState.TRUE, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 3. User manually pinches to zoom (e.g. zooms to 2.2f).
        mPdfCoordinator.onViewportChanged(0, 2.2f);
        assertEquals(TriState.NOT_SET, mPdfCoordinator.getIsFitToPageActiveForTesting());

        // 4. User enables single page view -> pinched zoom (2.2f) is preserved!
        mPdfCoordinator.toggleTwoPagesPerRow(false, 2.2f, 0);
        ShadowLooper.idleMainLooper();

        assertEquals(1, shadowPdfView.mPagesPerRow);
        assertEquals(2.2f, shadowPdfView.mZoom, 0.001f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testTwoPageView_ToggleSinglePageView_LandsOnFirstOfTwoPages() {
        createPdfCoordinator();
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);

        // 1. User enables two-page view.
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.0f, 0);
        assertEquals(2, shadowPdfView.mPagesPerRow);

        // 2. Viewport changes where pages 2 and 3 (pages 3 and 4) are visible in row 1.
        SparseArray<RectF> twoPageLocations = new SparseArray<>();
        twoPageLocations.put(2, new RectF(0, 0, 400, 800));
        twoPageLocations.put(3, new RectF(400, 0, 800, 800));
        int calculatedPage =
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mPdfView, 2, twoPageLocations);
        assertEquals(2, calculatedPage);

        mPdfCoordinator.onViewportChanged(calculatedPage, 1.0f);

        // 3. User toggles to single page view from toolbar using current page index 2 (page 3).
        mPdfCoordinator.toggleTwoPagesPerRow(false, 1.0f, calculatedPage);

        assertEquals(1, shadowPdfView.mPagesPerRow);
        float expectedYOffsetPoints = (mPdfView.getHeight() / 2f) / 1.0f;
        assertEquals(new PdfPoint(2, 0f, expectedYOffsetPoints), shadowPdfView.mPdfPoint);
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
                        Proxy.newProxyInstance(
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

        // Simulate an intermediate unscaled viewport change arriving while default zoom is pending
        mPdfCoordinator.onViewportChanged(0, 1.0f);

        // Since setDefaultZoom posts to the UI thread, we must idle the looper.
        ShadowLooper.idleMainLooper();

        // viewportWidth = 1000. contentWidth = 200.
        // expectedZoom = (1000 * 0.5) / 200 = 500 / 200 = 2.5f
        assertEquals(2.5f, shadowPdfView.mZoom, 0.001f);
        assertNotNull(mPdfCoordinator.getToolbarCoordinatorForTesting());
        assertEquals(
                2.5f,
                mPdfCoordinator.getToolbarCoordinatorForTesting().getDefaultZoomLevel(),
                0.001f);
    }

    @SuppressWarnings("unchecked")
    private void setupMockPdfDocumentForPageInfo(
            ShadowPdfView shadowPdfView, int height, int width) {
        PdfDocument mockPdfDocument =
                (PdfDocument)
                        Proxy.newProxyInstance(
                                PdfDocument.class.getClassLoader(),
                                new Class[] {PdfDocument.class},
                                (proxy, method, args) -> {
                                    if (method.getName().equals("getPageInfo")
                                            && args != null
                                            && args.length == 2) {
                                        int pageIdx = (Integer) args[0];
                                        Continuation<PageInfo> continuation =
                                                (Continuation<PageInfo>) args[1];
                                        PageInfo realPageInfo =
                                                new PageInfo(
                                                        pageIdx,
                                                        height,
                                                        width,
                                                        java.util.Collections.emptyList());
                                        continuation.resumeWith(realPageInfo);
                                        return null;
                                    }
                                    if (method.getName().equals("getPageCount")) {
                                        return 5;
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
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testOnViewportChanged_ZeroWidthDoesNotBlockSubsequentDefaultZoom() {
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
        // Initially set layout width = 0 (before view measurement)
        mPdfView.layout(0, 0, /* width= */ 0, /* height= */ PDF_CONTENT_HEIGHT);
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(mPdfView);
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(mPdfCoordinator.getView());
        contentView.addView(mPdfView, new ViewGroup.LayoutParams(0, PDF_CONTENT_HEIGHT));

        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);

        // Early viewport event while width == 0 should not initiate default zoom nor lock pending
        // state
        mPdfCoordinator.onViewportChanged(0, 1.0f);
        ShadowLooper.idleMainLooper();
        assertEquals(
                -1.0f,
                mPdfCoordinator.getToolbarCoordinatorForTesting().getDefaultZoomLevel(),
                0.001f);

        // Resize layout width > 0 and fire viewport update; default zoom calculation should trigger
        mPdfView.layout(0, 0, /* width= */ 1000, /* height= */ PDF_CONTENT_HEIGHT);
        mPdfCoordinator.onViewportChanged(0, 1.0f);
        ShadowLooper.idleMainLooper();

        // viewportWidth = 1000. contentWidth = 200.
        // expectedZoom = (1000 * 0.5) / 200 = 500 / 200 = 2.5f
        assertEquals(2.5f, shadowPdfView.mZoom, 0.001f);
        assertEquals(
                2.5f,
                mPdfCoordinator.getToolbarCoordinatorForTesting().getDefaultZoomLevel(),
                0.001f);
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

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testFragmentOnEnterExitEditMode() {
        createPdfCoordinator();

        View editButton = mPdfCoordinator.getView().findViewById(R.id.edit_button);
        assertNotNull("Edit button should exist", editButton);
        assertFalse("Edit button should not be selected initially", editButton.isSelected());

        // Simulate fragment entering edit mode
        mPdfCoordinator.mChromePdfViewerFragment.onEnterEditMode();
        assertTrue("Edit button should be selected after onEnterEditMode", editButton.isSelected());

        // Simulate fragment exiting edit mode
        mPdfCoordinator.mChromePdfViewerFragment.onExitEditMode();
        assertFalse(
                "Edit button should not be selected after onExitEditMode", editButton.isSelected());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2 + ":enable_form_filling/true")
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testFormFillingEnabledWhenInlinePdfV2IsEnabled() {
        createPdfCoordinator();

        // Initially, when view is created, form filling should be enabled
        mPdfCoordinator.mChromePdfViewerFragment.onPdfViewCreated(mPdfView);
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertTrue(
                "Form filling should be enabled initially",
                shadowPdfView.isFormFillingEnabled());

        PdfDocument pdfDocument = Mockito.mock(PdfDocument.class);

        // Simulate document load success
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentSuccess(pdfDocument);
        assertTrue(
                "Form filling should still be enabled after document load success",
                shadowPdfView.isFormFillingEnabled());

        // Simulate document reload success
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentSuccess(pdfDocument);
        assertTrue(
                "Form filling should remain enabled after reload",
                shadowPdfView.isFormFillingEnabled());

        // Verify that if edit mode is true, document reload does not enable form filling
        mPdfCoordinator.mChromePdfViewerFragment.setEditModeEnabled(true);
        shadowPdfView.setFormFillingEnabled(false);
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentSuccess(pdfDocument);
        assertFalse(
                "Form filling should not be enabled on reload when edit mode is true",
                shadowPdfView.isFormFillingEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2 + ":enable_form_filling/false")
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testFormFillingDisabledWhenFormFillingParamIsDisabled() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.onPdfViewCreated(mPdfView);
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertFalse(
                "Form filling should not be enabled when enable_form_filling is" + " false",
                shadowPdfView.isFormFillingEnabled());

        PdfDocument pdfDocument = Mockito.mock(PdfDocument.class);
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentSuccess(pdfDocument);
        assertFalse(
                "Form filling should still not be enabled after document load success when"
                        + " enable_form_filling is false",
                shadowPdfView.isFormFillingEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testFormFillingDisabledWhenInlinePdfV2IsDisabled() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.onPdfViewCreated(mPdfView);
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertFalse(
                "Form filling should not be enabled when InlinePdfV2 is disabled",
                shadowPdfView.isFormFillingEnabled());

        PdfDocument pdfDocument = Mockito.mock(PdfDocument.class);
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentSuccess(pdfDocument);
        assertFalse(
                "Form filling should still not be enabled after document load success when"
                    + " InlinePdfV2 is disabled",
                shadowPdfView.isFormFillingEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testOnDestroyViewResetsPdfViewSetup() {
        createPdfCoordinator();
        mPdfCoordinator.mChromePdfViewerFragment.onPdfViewCreated(mPdfView);
        assertTrue(
                "mIsPdfViewSetup should be true after onPdfViewCreated",
                mPdfCoordinator.mChromePdfViewerFragment.mIsPdfViewSetup);
        assertNotNull(
                "mPdfView should be set after onPdfViewCreated",
                mPdfCoordinator.mChromePdfViewerFragment.mPdfView);

        mPdfCoordinator.mChromePdfViewerFragment.onDestroyView();
        assertFalse(
                "mIsPdfViewSetup should be false after onDestroyView",
                mPdfCoordinator.mChromePdfViewerFragment.mIsPdfViewSetup);
        assertNull(
                "mPdfView should be null after onDestroyView",
                mPdfCoordinator.mChromePdfViewerFragment.mPdfView);

        // Simulate navigating back and recreating the view.
        PdfView newPdfView = new PdfView(mActivity);
        mPdfCoordinator.mChromePdfViewerFragment.onPdfViewCreated(newPdfView);
        assertTrue(
                "mIsPdfViewSetup should be true after recreating PdfView",
                mPdfCoordinator.mChromePdfViewerFragment.mIsPdfViewSetup);
        assertEquals(
                "mPdfView should reference newPdfView",
                newPdfView,
                mPdfCoordinator.mChromePdfViewerFragment.mPdfView);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testOnPdfViewCreated_WithExistingDocument_CallsDocumentLoadedOnce() {
        PdfActionsDelegate mockDelegate = Mockito.mock(PdfActionsDelegate.class);
        when(mockDelegate.isPageNavAndEditVisible()).thenReturn(true);
        PdfCoordinator.ChromePdfViewerFragment fragment =
                new PdfCoordinator.ChromePdfViewerFragment(mockDelegate);

        PdfView pdfView = new PdfView(mActivity);
        ShadowPdfView shadowPdfView = Shadow.extract(pdfView);
        PdfDocument mockDocument = Mockito.mock(PdfDocument.class);
        when(mockDocument.getPageCount()).thenReturn(5);
        shadowPdfView.mPdfDocument = mockDocument;

        fragment.onPdfViewCreated(pdfView);

        verify(mockDelegate, times(1)).onDocumentLoaded(5);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testOnPdfViewCreated_WithExistingDocument_DocumentClosedException() {
        PdfActionsDelegate mockDelegate = Mockito.mock(PdfActionsDelegate.class);
        when(mockDelegate.isPageNavAndEditVisible()).thenReturn(true);
        PdfCoordinator.ChromePdfViewerFragment fragment =
                new PdfCoordinator.ChromePdfViewerFragment(mockDelegate);

        PdfView pdfView = new PdfView(mActivity);
        ShadowPdfView shadowPdfView = Shadow.extract(pdfView);
        PdfDocument mockDocument = Mockito.mock(PdfDocument.class);
        when(mockDocument.getPageCount()).thenThrow(new PdfDocument.DocumentClosedException());
        shadowPdfView.mPdfDocument = mockDocument;

        fragment.onPdfViewCreated(pdfView);

        verify(mockDelegate, never()).onDocumentLoaded(anyInt());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.INLINE_PDF_V2, ChromeFeatureList.PDF_REUSE_FRAGMENT})
    @Config(shadows = {ShadowEditablePdfViewerFragment.class, ShadowPdfView.class})
    public void testCreatePdfCoordinator_ReusesFragmentWithLoadedDocument_NoAssertionError() {
        TestChromePdfViewerFragment existingFragment = new TestChromePdfViewerFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(existingFragment, String.valueOf(TAB_ID))
                .commitNow();
        PdfView pdfView = new PdfView(mActivity);
        ShadowPdfView shadowPdfView = Shadow.extract(pdfView);
        PdfDocument mockDocument = Mockito.mock(PdfDocument.class);
        when(mockDocument.getPageCount()).thenReturn(3);
        shadowPdfView.mPdfDocument = mockDocument;
        existingFragment.onPdfViewCreated(pdfView);

        createPdfCoordinator();
        assertNotNull(mPdfCoordinator.getView());
        assertTrue(mPdfCoordinator.mChromePdfViewerFragment.mIsPdfViewSetup);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(qualifiers = "w800dp")
    public void testToolBoxViewVisibility() {
        createPdfCoordinator();

        // Inject test fragment
        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mPdfCoordinator);
        mPdfCoordinator.mChromePdfViewerFragment = fragment;

        // Setup view hierarchy for fragment
        FrameLayout fragmentView = new FrameLayout(mActivity);
        View toolBoxView = new View(mActivity);
        toolBoxView.setId(R.id.toolBoxView);
        fragmentView.addView(toolBoxView);

        // Manually trigger onViewCreated (our overridden version that skips JNI)
        fragment.onViewCreated(fragmentView, null);

        // Initially, isPageNavAndEditVisible is true (default), so toolBoxView should be removed
        // (hidden)
        assertNull(
                "ToolBoxView should be removed initially because top toolbar is visible",
                toolBoxView.getParent());

        // Hide top toolbar -> toolBoxView should be added (visible)
        mPdfCoordinator.onPageNavAndEditVisibilityChanged(false);
        assertNotNull(
                "ToolBoxView should be added when top toolbar is hidden", toolBoxView.getParent());

        // Show top toolbar again -> toolBoxView should be removed (hidden)
        mPdfCoordinator.onPageNavAndEditVisibilityChanged(true);
        assertNull(
                "ToolBoxView should be removed when top toolbar is visible",
                toolBoxView.getParent());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testFragmentLifecycleSetsClassLoader() {
        PdfCoordinator.ChromePdfViewerFragment fragment =
                new PdfCoordinator.ChromePdfViewerFragment();

        // Test onAttach
        Bundle arguments = new Bundle();
        fragment.setArguments(arguments);
        try {
            fragment.onAttach(mActivity);
        } catch (Throwable t) {
            // Ignore exceptions to test classloader setup.
        }
        assertEquals(
                PdfCoordinator.ChromePdfViewerFragment.class.getClassLoader(),
                fragment.getArguments().getClassLoader());

        // Test onCreate
        Bundle savedInstanceState = new Bundle();
        try {
            fragment.onCreate(savedInstanceState);
        } catch (Throwable t) {
            // Ignore exceptions to test classloader setup.
        }
        assertEquals(
                PdfCoordinator.ChromePdfViewerFragment.class.getClassLoader(),
                savedInstanceState.getClassLoader());

        // Test onViewCreated
        Bundle savedInstanceState2 = new Bundle();
        View dummyView = new View(mActivity);
        try {
            fragment.onViewCreated(dummyView, savedInstanceState2);
        } catch (Throwable t) {
            // Ignore exceptions to test classloader setup.
        }
        assertEquals(
                PdfCoordinator.ChromePdfViewerFragment.class.getClassLoader(),
                savedInstanceState2.getClassLoader());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    public void testReloadRestoresPosition() throws Exception {
        PdfCoordinator.skipLoadPdfForTesting(true);
        createPdfCoordinator();

        // Set position on the initial fragment's pdfView.
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        shadowPdfView.mFirstVisiblePage = 5;
        shadowPdfView.mZoom = 2.5f;

        // Call reload. This should recreate the fragment and set arguments.
        mPdfCoordinator.reload();

        // Get the new fragment.
        PdfCoordinator.ChromePdfViewerFragment newFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        assertNotNull(newFragment);

        // Verify arguments were set on the new fragment.
        Bundle args = newFragment.getArguments();
        assertNotNull(args);
        assertEquals(5, args.getInt(PdfCoordinator.ChromePdfViewerFragment.KEY_SAVED_PAGE_INDEX));
        assertEquals(
                2.5f, args.getFloat(PdfCoordinator.ChromePdfViewerFragment.KEY_SAVED_ZOOM), 0.001f);
        assertTrue(
                args.getBoolean(
                        PdfCoordinator.ChromePdfViewerFragment.KEY_RESTORE_POSITION_PENDING));

        // Now trigger onViewCreated manually on the new fragment and verify it restores the
        // position.
        View dummyView = new View(mActivity);
        try {
            newFragment.onViewCreated(dummyView, null);
        } catch (Throwable t) {
            // Ignore exceptions from super.onViewCreated.
        }

        // Verify new fragment has restored values using reflection.
        java.lang.reflect.Field pageField =
                PdfCoordinator.ChromePdfViewerFragment.class.getDeclaredField("mSavedPageIndex");
        pageField.setAccessible(true);
        int savedPageIndex = (int) pageField.get(newFragment);
        assertEquals(5, savedPageIndex);

        java.lang.reflect.Field zoomField =
                PdfCoordinator.ChromePdfViewerFragment.class.getDeclaredField("mSavedZoom");
        zoomField.setAccessible(true);
        float savedZoom = (float) zoomField.get(newFragment);
        assertEquals(2.5f, savedZoom, 0.001f);

        java.lang.reflect.Field pendingField =
                PdfCoordinator.ChromePdfViewerFragment.class.getDeclaredField(
                        "mRestorePositionPending");
        pendingField.setAccessible(true);
        boolean restorePositionPending = (boolean) pendingField.get(newFragment);
        assertTrue(restorePositionPending);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnDownloadComplete_WhenLoaded_ReloadsWithContentUri() {
        createPdfCoordinator();
        assertTrue(mPdfCoordinator.getIsPdfLoadedForTesting());

        String newFilePath = "/data/user/10/com.google.android.apps.chrome/cache/pdfs/new_fw4.pdf";
        String newFileName = "new_fw4.pdf";
        mPdfCoordinator.onDownloadComplete(newFilePath, newFileName);

        assertEquals(newFilePath, mPdfCoordinator.getFilepath());
        Uri expectedUri =
                PdfUtils.getContentUri(newFilePath, newFileName, String.valueOf(TAB_ID), false);
        assertEquals(expectedUri, mPdfCoordinator.getUri());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testLoadPdfFile_SameUri_SetsDocumentUri() {
        createPdfCoordinator();
        assertTrue(mPdfCoordinator.getIsPdfLoadedForTesting());
        Uri originalUri = mPdfCoordinator.getUri();

        mPdfCoordinator.resetLoadState();
        assertFalse(mPdfCoordinator.getIsPdfLoadedForTesting());

        mPdfCoordinator.onDownloadComplete(FILE_PATH, PDF_TITLE);
        mPdfCoordinator.mChromePdfViewerFragment.setDocumentUri(mPdfCoordinator.getUri());
        assertTrue(mPdfCoordinator.getIsPdfLoadedForTesting());
        assertEquals(originalUri, mPdfCoordinator.getUri());
        assertEquals(originalUri, mPdfCoordinator.mChromePdfViewerFragment.getDocumentUri());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReloadWhenViewDetached() {
        createPdfCoordinator();
        assertTrue(mPdfCoordinator.getIsPdfLoadedForTesting());

        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.removeView(mPdfCoordinator.getView());
        assertNull(mPdfCoordinator.getView().getParent());

        mPdfCoordinator.reload();
        assertFalse(mPdfCoordinator.getIsPdfLoadedForTesting());

        contentView.addView(mPdfCoordinator.getView());
        ShadowLooper.idleMainLooper();
        assertTrue(mPdfCoordinator.getIsPdfLoadedForTesting());
    }

    public static class TestModalDialogActivity extends org.chromium.ui.base.TestActivity
            implements org.chromium.ui.modaldialog.ModalDialogManagerHolder {
        private org.chromium.ui.modaldialog.ModalDialogManager mModalDialogManager;

        public void setModalDialogManager(org.chromium.ui.modaldialog.ModalDialogManager manager) {
            mModalDialogManager = manager;
        }

        @Override
        public org.chromium.ui.modaldialog.ModalDialogManager getModalDialogManager() {
            return mModalDialogManager;
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testShowDocumentProperties_AlertDialog() throws Exception {
        File tempFile = File.createTempFile("test_pdf", ".pdf");
        tempFile.deleteOnExit();
        FileWriter writer = new FileWriter(tempFile);
        writer.write("test pdf content");
        writer.close();

        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        tempFile.getAbsolutePath(),
                        PDF_TITLE,
                        TAB_ID,
                        PDF_URL,
                        mPdfFragmentViewTracker);
        mPdfView = new PdfView(mActivity);
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(mPdfView);
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        contentView.addView(mPdfCoordinator.getView());
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        setupMockPdfDocumentForPageInfo(shadowPdfView, 400, 200);

        // Run posted tasks (loadPdfFile) before showing properties
        ShadowLooper.idleMainLooper();

        mPdfCoordinator.showDocumentProperties();

        // Run background thread properties loader and then post to UI thread
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ShadowLooper.idleMainLooper();

        androidx.appcompat.app.AlertDialog latestDialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Dialog should be shown", latestDialog);
        assertTrue("Dialog should be showing", latestDialog.isShowing());

        TextView fileNameValue = latestDialog.findViewById(R.id.file_name_value);
        TextView fileSizeValue = latestDialog.findViewById(R.id.file_size_value);
        TextView pageCountValue = latestDialog.findViewById(R.id.page_count_value);
        TextView pageSizeValue = latestDialog.findViewById(R.id.page_size_value);

        assertNotNull(fileNameValue);
        assertNotNull(fileSizeValue);
        assertNotNull(pageCountValue);
        assertNotNull(pageSizeValue);

        assertEquals(tempFile.getName(), fileNameValue.getText().toString());
        assertEquals("5", pageCountValue.getText().toString());
        assertEquals("16 B", fileSizeValue.getText().toString());
        assertEquals("2.78 × 5.56 in (71 × 141 mm)", pageSizeValue.getText().toString());

        mPdfCoordinator.destroy();
        assertFalse("Dialog should be dismissed on destroy", latestDialog.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowPdfView.class})
    @SuppressWarnings("unchecked")
    public void testShowDocumentProperties_ModalDialog() throws Exception {
        try (var controller =
                org.robolectric.Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.APP);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            File tempFile = File.createTempFile("test_pdf", ".pdf");
            tempFile.deleteOnExit();
            FileWriter writer = new FileWriter(tempFile);
            writer.write("test pdf content");
            writer.close();

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            tempFile.getAbsolutePath(),
                            PDF_TITLE,
                            TAB_ID,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowPdfView shadowPdfView = Shadow.extract(pdfView);
            PdfDocument mockPdfDocument =
                    (PdfDocument)
                            Proxy.newProxyInstance(
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
                                        if (method.getName().equals("getPageCount")) {
                                            return 5;
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

            // Run posted tasks (loadPdfFile) before showing properties
            ShadowLooper.idleMainLooper();

            pdfCoordinator.showDocumentProperties();

            // Run background thread properties loader and then post to UI thread
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            ShadowLooper.idleMainLooper();

            PropertyModel dialogModel = fakeModalDialogManager.getShownDialogModel();
            assertNotNull("Modal dialog should be shown", dialogModel);
            assertEquals(
                    customActivity.getString(R.string.pdf_document_properties),
                    dialogModel.get(ModalDialogProperties.TITLE));

            android.view.View dialogCustomView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
            assertNotNull(dialogCustomView);

            TextView fileNameValue = dialogCustomView.findViewById(R.id.file_name_value);
            TextView fileSizeValue = dialogCustomView.findViewById(R.id.file_size_value);
            TextView pageCountValue = dialogCustomView.findViewById(R.id.page_count_value);
            TextView pageSizeValue = dialogCustomView.findViewById(R.id.page_size_value);

            assertNotNull(fileNameValue);
            assertNotNull(fileSizeValue);
            assertNotNull(pageCountValue);
            assertNotNull(pageSizeValue);

            assertEquals(tempFile.getName(), fileNameValue.getText().toString());
            assertEquals("5", pageCountValue.getText().toString());
            assertEquals("16 B", fileSizeValue.getText().toString());
            assertEquals("2.78 × 5.56 in (71 × 141 mm)", pageSizeValue.getText().toString());
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_True() {
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);

        mPdfCoordinator.setEditMode(true);

        assertTrue(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_EditDisabled() {
        PdfUtils.setInlinePdfV2EditEnabledForTesting(false);
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        mPdfCoordinator.setEditMode(true);
        assertNull(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());

        mPdfCoordinator.setEditMode(false);
        assertFalse(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_False_NoUnsavedChanges() {
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(false);

        mPdfCoordinator.setEditMode(false);

        assertFalse(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_False_WithUnsavedChanges_Flow() throws Exception {
        // Use a content URI to test the save flow
        ChromeFileProvider.setGeneratedUriForTesting(
                Uri.parse("content://com.android.chrome.provider/test.pdf"));
        createPdfCoordinator();

        // Manually attach the fragment because loadPdfInternal skips it in tests due to
        // sSkipLoadPdfForTesting.
        // This is now safe because setupTouchListeners is shadowed to do nothing.
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        File tempFile = File.createTempFile("test_pdf", ".pdf");
        tempFile.deleteOnExit();
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);

        // Register TestContentProvider
        TestContentProvider provider = new TestContentProvider(pfd);
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = "com.android.chrome.provider";
        provider.attachInfo(mActivity, providerInfo);
        ShadowContentResolver.registerProviderInternal("com.android.chrome.provider", provider);

        mPdfCoordinator.setEditMode(false);

        assertTrue(shadowFragment.wasApplyDraftEditsCalled());
        assertEquals(null, shadowFragment.getEditModeEnabled());

        // Simulate success callback with fake
        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        assertTrue(fakeHandle.mWriteToCalled);
        assertNotNull(fakeHandle.mContinuation);

        // Resume continuation to finish write
        fakeHandle.mContinuation.resumeWith(kotlin.Unit.INSTANCE);

        // Run posted tasks on UI thread (finishExitingEditMode is posted)
        ShadowLooper.idleMainLooper();

        // Now it should be disabled
        assertFalse(shadowFragment.getEditModeEnabled());
        assertTrue(fakeHandle.mClosed);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_False_WithUnsavedChanges_AsyncFailureFlow() throws Exception {
        ChromeFileProvider.setGeneratedUriForTesting(
                Uri.parse("content://com.android.chrome.provider/test.pdf"));
        createPdfCoordinator();

        // Manually attach the fragment because loadPdfInternal skips it in tests due to
        // sSkipLoadPdfForTesting.
        // This is now safe because setupTouchListeners is shadowed to do nothing.
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        File tempFile = File.createTempFile("test_pdf", ".pdf");
        tempFile.deleteOnExit();
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);

        TestContentProvider provider = new TestContentProvider(pfd);
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = "com.android.chrome.provider";
        provider.attachInfo(mActivity, providerInfo);
        ShadowContentResolver.registerProviderInternal("com.android.chrome.provider", provider);

        mPdfCoordinator.setEditMode(false);

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Resume continuation with failure
        Object failure = new IOException("Test exception");
        fakeHandle.mContinuation.resumeWith(failure);

        ShadowLooper.idleMainLooper();

        // It should still disable edit mode and close handles
        assertFalse(shadowFragment.getEditModeEnabled());
        assertTrue(fakeHandle.mClosed);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_False_WithUnsavedChanges_SyncSuccessFlow() throws Exception {
        ChromeFileProvider.setGeneratedUriForTesting(
                Uri.parse("content://com.android.chrome.provider/test.pdf"));
        createPdfCoordinator();

        // Manually attach the fragment because loadPdfInternal skips it in tests due to
        // sSkipLoadPdfForTesting.
        // This is now safe because setupTouchListeners is shadowed to do nothing.
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        File tempFile = File.createTempFile("test_pdf", ".pdf");
        tempFile.deleteOnExit();
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);

        TestContentProvider provider = new TestContentProvider(pfd);
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = "com.android.chrome.provider";
        provider.attachInfo(mActivity, providerInfo);
        ShadowContentResolver.registerProviderInternal("com.android.chrome.provider", provider);

        mPdfCoordinator.setEditMode(false);

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        // Make it return Unit.INSTANCE to simulate sync completion
        fakeHandle.mResult = kotlin.Unit.INSTANCE;

        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Run posted tasks (finishExitingEditMode is posted)
        ShadowLooper.idleMainLooper();

        // For sync completion, it should finish immediately
        assertFalse(shadowFragment.getEditModeEnabled());
        assertTrue(fakeHandle.mClosed);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_NoUnsavedChanges() {
        createPdfCoordinator();
        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(false).when(spyFragment).hasUnsavedChanges();

        mPdfCoordinator.reload();

        assertNotSame(originalFragment, mPdfCoordinator.mChromePdfViewerFragment);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithUnsavedChanges_Dismiss() throws Exception {
        try (var controller =
                org.robolectric.Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.APP);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            FILE_PATH,
                            PDF_TITLE,
                            TAB_ID,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            pdfCoordinator.reload();

            // Dialog should be shown
            PropertyModel dialogModel = fakeModalDialogManager.getShownDialogModel();
            assertNotNull("Modal dialog should be shown", dialogModel);
            assertEquals(
                    customActivity.getString(R.string.pdf_unsaved_changes_dialog_reload_title),
                    dialogModel.get(ModalDialogProperties.TITLE));
            assertEquals(
                    customActivity.getString(R.string.pdf_unsaved_changes_dialog_message),
                    dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));

            // Click Cancel (Negative button)
            fakeModalDialogManager.clickNegativeButton();

            // Verify dialog is dismissed and reload did NOT happen
            assertNull(fakeModalDialogManager.getShownDialogModel());
            assertSame(spyFragment, pdfCoordinator.mChromePdfViewerFragment);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithUnsavedChanges_Confirm() throws Exception {
        try (var controller =
                org.robolectric.Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.APP);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            FILE_PATH,
                            PDF_TITLE,
                            TAB_ID,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            pdfCoordinator.reload();

            // Dialog should be shown
            PropertyModel dialogModel = fakeModalDialogManager.getShownDialogModel();
            assertNotNull("Modal dialog should be shown", dialogModel);

            // Click Reload (Positive button)
            fakeModalDialogManager.clickPositiveButton();

            // Verify dialog is dismissed and reload DID happen (fragment changed)
            assertNull(fakeModalDialogManager.getShownDialogModel());
            assertNotSame(spyFragment, pdfCoordinator.mChromePdfViewerFragment);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testSetEditMode_False_WithUnsavedChanges_RuntimeExceptionFlow() throws Exception {
        ChromeFileProvider.setGeneratedUriForTesting(
                Uri.parse("content://com.android.chrome.provider/test.pdf"));
        createPdfCoordinator();

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        File tempFile = File.createTempFile("test_pdf", ".pdf");
        tempFile.deleteOnExit();
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);

        TestContentProvider provider = new TestContentProvider(pfd);
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = "com.android.chrome.provider";
        provider.attachInfo(mActivity, providerInfo);
        ShadowContentResolver.registerProviderInternal("com.android.chrome.provider", provider);

        mPdfCoordinator.setEditMode(false);

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle() {
            @Override
            public Object writeTo(
                    ParcelFileDescriptor destination, Continuation<? super kotlin.Unit> continuation) {
                super.writeTo(destination, continuation);
                throw new RuntimeException("Test runtime exception during writeTo");
            }
        };

        boolean exceptionThrown = false;
        try {
            mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);
        } catch (RuntimeException e) {
            if (e.getMessage().equals("Test runtime exception during writeTo")) {
                exceptionThrown = true;
            } else {
                throw e;
            }
        }

        assertTrue("Expected RuntimeException was not thrown", exceptionThrown);

        // Even with RuntimeException, it should close handles and disable edit mode
        assertTrue(fakeHandle.mClosed);
        assertFalse(shadowFragment.getEditModeEnabled());

        // Also check if pfd is closed.
        boolean pfdClosed = false;
        try {
            pfd.getFd();
        } catch (IllegalStateException e) {
            pfdClosed = true;
        }
        assertTrue("ParcelFileDescriptor should be closed", pfdClosed);
    }

    private static final String ACTION_ANNOTATE = "android.intent.action.ANNOTATE";

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLoadDocumentSuccess_V2Disabled_HidesToolboxWhenNoAnnotator() {
        createPdfCoordinator();

        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mPdfCoordinator);
        mPdfCoordinator.mChromePdfViewerFragment = fragment;
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "test_pdf_tag_1")
                .commitNow();

        FrameLayout fragmentView = new FrameLayout(mActivity);
        View toolBoxView = new View(mActivity);
        toolBoxView.setId(R.id.toolBoxView);
        fragmentView.addView(toolBoxView);
        fragment.onViewCreated(fragmentView, null);

        assertEquals(View.VISIBLE, toolBoxView.getVisibility());

        PdfDocument pdfDocument = Mockito.mock(PdfDocument.class);
        fragment.onLoadDocumentSuccess(pdfDocument);

        assertEquals(View.GONE, toolBoxView.getVisibility());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOnLoadDocumentSuccess_V2Disabled_KeepsToolboxWhenAnnotatorExists() {
        createPdfCoordinator();

        Intent intent = new Intent(ACTION_ANNOTATE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setDataAndType(mPdfCoordinator.getUri(), "application/pdf");

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.pdfannotator";
        resolveInfo.activityInfo.name = "com.example.pdfannotator.AnnotateActivity";
        org.robolectric.Shadows.shadowOf(mActivity.getPackageManager())
                .addResolveInfoForIntent(intent, resolveInfo);

        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mPdfCoordinator);
        mPdfCoordinator.mChromePdfViewerFragment = fragment;
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "test_pdf_tag_2")
                .commitNow();

        FrameLayout fragmentView = new FrameLayout(mActivity);
        View toolBoxView = new View(mActivity);
        toolBoxView.setId(R.id.toolBoxView);
        fragmentView.addView(toolBoxView);
        fragment.onViewCreated(fragmentView, null);

        PdfDocument pdfDocument = Mockito.mock(PdfDocument.class);
        fragment.onLoadDocumentSuccess(pdfDocument);

        assertEquals(View.VISIBLE, toolBoxView.getVisibility());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testOpenPdfInExternalEditor_OnClick() {
        createPdfCoordinator();

        Intent intent = new Intent(ACTION_ANNOTATE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setDataAndType(mPdfCoordinator.getUri(), "application/pdf");

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.pdfannotator";
        resolveInfo.activityInfo.name = "com.example.pdfannotator.AnnotateActivity";
        org.robolectric.Shadows.shadowOf(mActivity.getPackageManager())
                .addResolveInfoForIntent(intent, resolveInfo);

        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mPdfCoordinator);
        mPdfCoordinator.mChromePdfViewerFragment = fragment;
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "test_pdf_tag_3")
                .commitNow();

        FrameLayout fragmentView = new FrameLayout(mActivity);
        View toolBoxView = new View(mActivity);
        toolBoxView.setId(R.id.toolBoxView);
        fragmentView.addView(toolBoxView);
        fragment.onViewCreated(fragmentView, null);

        toolBoxView.performClick();

        Intent startedIntent = org.robolectric.Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(ACTION_ANNOTATE, startedIntent.getAction());
        assertEquals(mPdfCoordinator.getUri(), startedIntent.getData());
        assertEquals("application/pdf", startedIntent.getType());
    }

    @Test
    public void testCalculateCurrentPage() {
        PdfView mockPdfView = org.mockito.Mockito.mock(PdfView.class);
        when(mockPdfView.getHeight()).thenReturn(1000); // 50% threshold is y = 500

        // Case 1: pageLocations is null - fallback to firstVisiblePage
        assertEquals(
                0,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(mockPdfView, 0, null));

        // Case 2: Page 1 top (rect.top = 600) is below 50% viewport height (threshold 500)
        SparseArray<RectF> pageLocations = new SparseArray<>();
        pageLocations.put(0, new RectF(0, -200, 800, 600));
        pageLocations.put(1, new RectF(0, 600, 800, 1400));
        assertEquals(
                0,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mockPdfView, 0, pageLocations));

        // Case 3: Page 1 top (rect.top = 450) crosses 50% viewport height (threshold 500)
        pageLocations.put(0, new RectF(0, -350, 800, 450));
        pageLocations.put(1, new RectF(0, 450, 800, 1250));
        assertEquals(
                1,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mockPdfView, 0, pageLocations));

        // Case 4: Multiple pages visible, page 2 crosses threshold, page 3 is below threshold
        SparseArray<RectF> multiPageLocations = new SparseArray<>();
        multiPageLocations.put(1, new RectF(0, -600, 800, 200));
        multiPageLocations.put(2, new RectF(0, 200, 800, 1000));
        multiPageLocations.put(3, new RectF(0, 1000, 800, 1800));
        assertEquals(
                2,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mockPdfView, 1, multiPageLocations));

        // Case 5: Two pages per row, pages 0 and 1 visible at the top (top = 0)
        SparseArray<RectF> twoPageLocations = new SparseArray<>();
        twoPageLocations.put(0, new RectF(0, 0, 400, 800));
        twoPageLocations.put(1, new RectF(400, 0, 800, 800));
        assertEquals(
                0,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mockPdfView, 0, twoPageLocations));

        // Case 6: Two pages per row, pages 2 and 3 (pages 3 and 4) visible and crossing threshold
        SparseArray<RectF> twoPageRow1Locations = new SparseArray<>();
        twoPageRow1Locations.put(0, new RectF(0, -600, 400, 200));
        twoPageRow1Locations.put(1, new RectF(400, -600, 800, 200));
        twoPageRow1Locations.put(2, new RectF(0, 200, 400, 1000));
        twoPageRow1Locations.put(3, new RectF(400, 200, 800, 1000));
        twoPageRow1Locations.put(4, new RectF(0, 1000, 400, 1800));
        twoPageRow1Locations.put(5, new RectF(400, 1000, 800, 1800));
        assertEquals(
                2,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mockPdfView, 0, twoPageRow1Locations));

        // Case 7: Two pages per row, row 1 has not crossed threshold
        SparseArray<RectF> twoPageRow0Locations = new SparseArray<>();
        twoPageRow0Locations.put(0, new RectF(0, -200, 400, 600));
        twoPageRow0Locations.put(1, new RectF(400, -200, 800, 600));
        twoPageRow0Locations.put(2, new RectF(0, 600, 400, 1400));
        twoPageRow0Locations.put(3, new RectF(400, 600, 800, 1400));
        assertEquals(
                0,
                PdfCoordinator.ChromePdfViewerFragment.calculateCurrentPage(
                        mockPdfView, 0, twoPageRow0Locations));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithAppliedChanges_ShowsConfirmation() throws Exception {
        ChromeFileProvider.setGeneratedUriForTesting(
                Uri.parse("content://com.android.chrome.provider/test.pdf"));
        createPdfCoordinator();

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        File tempFile = File.createTempFile("test_pdf", ".pdf");
        tempFile.deleteOnExit();
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_WRITE_ONLY);

        TestContentProvider provider = new TestContentProvider(pfd);
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = "com.android.chrome.provider";
        provider.attachInfo(mActivity, providerInfo);
        ShadowContentResolver.registerProviderInternal("com.android.chrome.provider", provider);

        // Exit edit mode, which calls applyDraftEdits
        mPdfCoordinator.setEditMode(false);

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();

        // Trigger onApplyEditsSuccess
        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Resume continuation to simulate success
        assertNotNull(fakeHandle.mContinuation);
        fakeHandle.mContinuation.resumeWith(kotlin.Unit.INSTANCE);

        // Wait for post tasks to run (finishExitingEditMode is posted)
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Now reload should show confirmation dialog.
        mPdfCoordinator.reload();

        // Verify dialog is shown.
        androidx.appcompat.app.AlertDialog latestDialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Dialog should be shown", latestDialog);
        assertTrue("Dialog should be showing", latestDialog.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithoutChanges_DoesNotShowConfirmation() throws Exception {
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(false);

        mPdfCoordinator.reload();

        androidx.appcompat.app.AlertDialog latestDialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        if (latestDialog != null) {
            assertFalse("Dialog should not be showing", latestDialog.isShowing());
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDestroy_DismissesAlertDialog() {
        createPdfCoordinator();
        mPdfCoordinator.showReloadConfirmationDialog(() -> {});

        androidx.appcompat.app.AlertDialog latestDialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Dialog should be shown", latestDialog);
        assertTrue("Dialog should be showing", latestDialog.isShowing());
        assertSame(latestDialog, mPdfCoordinator.getAlertDialogForTesting());

        mPdfCoordinator.destroy();

        assertFalse("Dialog should be dismissed on destroy", latestDialog.isShowing());
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testShowAlertDialog_DismissesPreviousAlertDialog() {
        createPdfCoordinator();
        mPdfCoordinator.showReloadConfirmationDialog(() -> {});

        androidx.appcompat.app.AlertDialog firstDialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("First dialog should be shown", firstDialog);
        assertTrue("First dialog should be showing", firstDialog.isShowing());
        assertSame(firstDialog, mPdfCoordinator.getAlertDialogForTesting());

        mPdfCoordinator.showReloadConfirmationDialog(() -> {});

        androidx.appcompat.app.AlertDialog secondDialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Second dialog should be shown", secondDialog);
        assertNotSame(firstDialog, secondDialog);
        assertFalse("First dialog should be dismissed", firstDialog.isShowing());
        assertTrue("Second dialog should be showing", secondDialog.isShowing());
        assertSame(secondDialog, mPdfCoordinator.getAlertDialogForTesting());

        mPdfCoordinator.destroy();

        assertFalse("Second dialog should be dismissed on destroy", secondDialog.isShowing());
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testAlertDialog_DismissResetsAlertDialogField() {
        createPdfCoordinator();
        mPdfCoordinator.showReloadConfirmationDialog(() -> {});

        androidx.appcompat.app.AlertDialog dialog =
                (androidx.appcompat.app.AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull(dialog);
        assertSame(dialog, mPdfCoordinator.getAlertDialogForTesting());

        dialog.dismiss();
        ShadowLooper.idleMainLooper();
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
    }

    @Implements(PdfView.class)
    public static class ShadowPdfView extends ShadowView {
        public PdfPoint mPdfPoint;
        public float mZoom = 1.0f;
        public PdfDocument mPdfDocument;
        public int mPagesPerRow = 1;
        public int mFirstVisiblePage;
        public boolean mFormFillingEnabled;

        public ShadowPdfView() {}

        @Implementation
        public void setFormFillingEnabled(boolean enabled) {
            mFormFillingEnabled = enabled;
        }

        @Implementation
        public boolean isFormFillingEnabled() {
            return mFormFillingEnabled;
        }

        @Implementation
        public int getFirstVisiblePage() {
            return mFirstVisiblePage;
        }

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

    public static class TestChromePdfViewerFragment extends PdfCoordinator.ChromePdfViewerFragment {
        public TestChromePdfViewerFragment() {
            super();
        }

        public TestChromePdfViewerFragment(PdfActionsDelegate delegate) {
            super(delegate);
        }

        @Override
        public void onViewCreated(View view, Bundle savedInstanceState) {
            // Skip super.onViewCreated to avoid JNI initialization.
            setUpToolBoxView(view);
        }
    }

    @Implements(EditablePdfViewerFragment.class)
    public static class ShadowEditablePdfViewerFragment extends ShadowPdfViewerFragment {
        @RealObject private EditablePdfViewerFragment mRealFragment;
        private boolean mUnsavedChanges;
        private boolean mApplyDraftEditsCalled;
        private Boolean mEditModeEnabled;

        @Implementation
        public View onCreateView(
                android.view.LayoutInflater inflater,
                ViewGroup container,
                android.os.Bundle savedInstanceState) {
            return new FrameLayout(inflater.getContext());
        }

        @Implementation
        public void onViewCreated(View view, android.os.Bundle savedInstanceState) {
            // Do nothing to avoid findViewById crashes on dummy view
        }

        @Implementation
        public boolean hasUnsavedChanges() {
            return mUnsavedChanges;
        }

        @Implementation
        public void applyDraftEdits() {
            mApplyDraftEditsCalled = true;
        }

        @Implementation
        public void setEditModeEnabled(boolean enabled) {
            mEditModeEnabled = enabled;
        }

        @Implementation
        public boolean isEditModeEnabled() {
            return mEditModeEnabled != null ? mEditModeEnabled : false;
        }

        @Implementation
        public void setupTouchListeners() {
            // Do nothing to avoid native Ink initialization
        }

        @Implementation
        public void onDestroyView() {
            // Bypass EditablePdfViewerFragment.onDestroyView to avoid lateinit crash.
            // This requires "androidx.fragment.app" to be in instrumentedPackages in class Config.
            Shadow.directlyOn(mRealFragment, androidx.fragment.app.Fragment.class, "onDestroyView");
        }

        public void setHasUnsavedChanges(boolean hasChanges) {
            mUnsavedChanges = hasChanges;
        }

        public boolean wasApplyDraftEditsCalled() {
            return mApplyDraftEditsCalled;
        }

        public Boolean getEditModeEnabled() {
            return mEditModeEnabled;
        }
    }

    @Implements(PdfViewerFragment.class)
    public static class ShadowPdfViewerFragment {
        @RealObject private PdfViewerFragment mRealFragment;
        private Uri mDocumentUri;

        @Implementation
        public void setDocumentUri(Uri uri) {
            mDocumentUri = uri;
        }

        @Implementation
        public Uri getDocumentUri() {
            return mDocumentUri;
        }

        @Implementation
        public void onStart() {
            Shadow.directlyOn(mRealFragment, androidx.fragment.app.Fragment.class, "onStart");
        }

        @Implementation
        public void onResume() {
            Shadow.directlyOn(mRealFragment, androidx.fragment.app.Fragment.class, "onResume");
        }

        @Implementation
        public void onPause() {
            Shadow.directlyOn(mRealFragment, androidx.fragment.app.Fragment.class, "onPause");
        }

        @Implementation
        public void onStop() {
            Shadow.directlyOn(mRealFragment, androidx.fragment.app.Fragment.class, "onStop");
        }
    }

    public static class FakePdfWriteHandle implements PdfWriteHandle {
        public boolean mClosed;
        public boolean mWriteToCalled;
        public Continuation<? super kotlin.Unit> mContinuation;
        public Object mResult = kotlin.coroutines.intrinsics.IntrinsicsKt.getCOROUTINE_SUSPENDED();

        @Override
        public Object writeTo(
                ParcelFileDescriptor destination, Continuation<? super kotlin.Unit> continuation) {
            mWriteToCalled = true;
            mContinuation = continuation;
            return mResult;
        }

        @Override
        public void close() throws IOException {
            mClosed = true;
        }
    }

    private static class TestContentProvider extends ContentProvider {
        private final ParcelFileDescriptor mPfd;

        TestContentProvider(ParcelFileDescriptor pfd) {
            mPfd = pfd;
        }

        @Override
        public boolean onCreate() {
            return true;
        }

        @Override
        public Cursor query(
                Uri uri,
                String[] projection,
                String selection,
                String[] selectionArgs,
                String sortOrder) {
            return null;
        }

        @Override
        public String getType(Uri uri) {
            return null;
        }

        @Override
        public Uri insert(Uri uri, ContentValues values) {
            return null;
        }

        @Override
        public int delete(Uri uri, String selection, String[] selectionArgs) {
            return 0;
        }

        @Override
        public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
            return 0;
        }

        @Override
        public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
            if ("w".equals(mode) || "rw".equals(mode)) {
                return mPfd;
            }
            return super.openFile(uri, mode);
        }
    }
}
