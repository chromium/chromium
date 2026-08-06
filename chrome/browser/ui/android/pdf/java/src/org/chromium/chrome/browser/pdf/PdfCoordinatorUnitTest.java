// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
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
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
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
        verify(mNativePageHost, never()).loadUrl(any(LoadUrlParams.class), anyBoolean());
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
        verify(mNativePageHost, never()).loadUrl(any(LoadUrlParams.class), anyBoolean());
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
            HistogramWatcher histogramExpectation =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Android.Pdf.Hyperlink.ClickResult",
                            PdfHyperlinkClickResult.SUCCESS_LOAD_INITIATED);
            assertTrue(
                    "onLinkClicked should accept " + raw,
                    mPdfCoordinator.onLinkClicked(Uri.parse(raw)));
            histogramExpectation.assertExpected();
        }
        verify(mNativePageHost, times(allowedUris.length))
                .loadUrl(any(LoadUrlParams.class), eq(false));
    }

    private void runOnLinkClickedTest(boolean isIncognito) {
        when(mProfile.isOffTheRecord()).thenReturn(isIncognito);
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
    public void testResetLoadState_ResetsTwoPagesPerRow() {
        createPdfCoordinator();
        mPdfCoordinator.toggleTwoPagesPerRow(true, 1.5f, 2);
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertEquals(2, shadowPdfView.mPagesPerRow);

        mPdfCoordinator.resetLoadState();

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
        writer.write("dummy pdf content");
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
        TextView titleValue = latestDialog.findViewById(R.id.title_value);
        TextView pageCountValue = latestDialog.findViewById(R.id.page_count_value);
        TextView pageSizeValue = latestDialog.findViewById(R.id.page_size_value);

        assertNotNull(fileNameValue);
        assertNotNull(fileSizeValue);
        assertNotNull(titleValue);
        assertNotNull(pageCountValue);
        assertNotNull(pageSizeValue);

        assertEquals(tempFile.getName(), fileNameValue.getText().toString());
        assertEquals(PDF_TITLE, titleValue.getText().toString());
        assertEquals("5", pageCountValue.getText().toString());
        assertEquals("17 B", fileSizeValue.getText().toString());
        assertEquals("2.78 × 5.56 in (71 × 141 mm)", pageSizeValue.getText().toString());
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
            writer.write("dummy pdf content");
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
            TextView titleValue = dialogCustomView.findViewById(R.id.title_value);
            TextView pageCountValue = dialogCustomView.findViewById(R.id.page_count_value);
            TextView pageSizeValue = dialogCustomView.findViewById(R.id.page_size_value);

            assertNotNull(fileNameValue);
            assertNotNull(fileSizeValue);
            assertNotNull(titleValue);
            assertNotNull(pageCountValue);
            assertNotNull(pageSizeValue);

            assertEquals(tempFile.getName(), fileNameValue.getText().toString());
            assertEquals(PDF_TITLE, titleValue.getText().toString());
            assertEquals("5", pageCountValue.getText().toString());
            assertEquals("17 B", fileSizeValue.getText().toString());
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
        Intent intent = new Intent(ACTION_ANNOTATE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setDataAndType(Uri.parse(TEST_CONTENT_URI), "application/pdf");

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.pdfannotator";
        resolveInfo.activityInfo.name = "com.example.pdfannotator.AnnotateActivity";
        org.robolectric.Shadows.shadowOf(mActivity.getPackageManager())
                .addResolveInfoForIntent(intent, resolveInfo);

        createPdfCoordinator();

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
        Intent intent = new Intent(ACTION_ANNOTATE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setDataAndType(Uri.parse(TEST_CONTENT_URI), "application/pdf");

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.pdfannotator";
        resolveInfo.activityInfo.name = "com.example.pdfannotator.AnnotateActivity";
        org.robolectric.Shadows.shadowOf(mActivity.getPackageManager())
                .addResolveInfoForIntent(intent, resolveInfo);

        createPdfCoordinator();

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
        assertEquals(Uri.parse(TEST_CONTENT_URI), startedIntent.getData());
        assertEquals("application/pdf", startedIntent.getType());
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
            if ("w".equals(mode)) {
                return mPfd;
            }
            return super.openFile(uri, mode);
        }
    }
}
