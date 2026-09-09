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
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.DialogInterface;
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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.pdf.PdfDocument;
import androidx.pdf.PdfDocument.PageInfo;
import androidx.pdf.PdfPoint;
import androidx.pdf.PdfWriteHandle;
import androidx.pdf.content.ExternalLink;
import androidx.pdf.ink.EditablePdfViewerFragment;
import androidx.pdf.view.PdfView;
import androidx.pdf.viewer.fragment.PdfViewerFragment;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import kotlin.Unit;
import kotlin.coroutines.Continuation;
import kotlin.coroutines.intrinsics.IntrinsicsKt;

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
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowParcelFileDescriptor;
import org.robolectric.shadows.ShadowToast;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.TriState;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfHyperlinkClickResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.BeforeUnloadCallback;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Proxy;
import java.util.Collections;
import java.util.concurrent.atomic.AtomicBoolean;

@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.PDF_REUSE_FRAGMENT)
@Config(
        sdk = 35,
        instrumentedPackages = {"androidx.fragment.app", "androidx.pdf"},
        shadows = {
            PdfCoordinatorUnitTest.ShadowPdfViewerFragment.class,
            PdfCoordinatorUnitTest.ShadowEditablePdfViewerFragment.class,
            PdfCoordinatorUnitTest.CustomShadowParcelFileDescriptor.class
        })
public class PdfCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mNativePageHost;
    @Mock private Profile mProfile;
    @Mock private PdfFragmentViewTracker mPdfFragmentViewTracker;
    @Mock private Tab mTab;

    private FragmentActivity mActivity;
    private PdfCoordinator mPdfCoordinator;
    private PdfView mPdfView;
    private static final String PDF_URL =
            "chrome-native://pdf/link?url=https%3A%2F%2Fwww.irs.gov%2Fpub%2Firs-pdf%2Ffw4.pdf";
    private static final String PDF_DOWNLOAD_URL = "https://www.irs.gov/pub/irs-pdf/fw4.pdf";
    private static final String PDF_TITLE = "fw4.pdf";
    private static final String LINK_URL = "https://www.bar.com";
    private String mFilePath;
    private static final String TEST_CONTENT_URI =
            "content://com.android.chrome.provider/fw4.pdf";
    private static final int TAB_ID = 123;
    private static final int PDF_CONTENT_HEIGHT = 1000;

    private UserDataHost mUserDataHost;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        PdfCoordinator.skipLoadPdfForTesting(true);
        PdfUtils.setInlinePdfV2EditEnabledForTesting(true);
        ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(TEST_CONTENT_URI));
        PostTask.setPrenativeThreadPoolExecutorForTesting(Runnable::run);
        when(mTab.getId()).thenReturn(TAB_ID);
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        File tempFile = File.createTempFile("fw4", ".pdf");
        tempFile.deleteOnExit();
        mFilePath = tempFile.getPath();
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        ChromeFileProvider.setGeneratedUriForTesting(null);
        PostTask.setPrenativeThreadPoolExecutorForTesting(null);
        if (mUserActionTester != null) {
            mUserActionTester.tearDown();
            new File(mFilePath).delete();
        }
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
                        mFilePath,
                        PDF_TITLE,
                        mTab,
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

        Uri linkUri = Uri.parse(LINK_URL);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Pdf.Hyperlink.ClickResult",
                        PdfHyperlinkClickResult.SUCCESS_LOAD_INITIATED);
        boolean result = mPdfCoordinator.onLinkClicked(linkUri);
        assertTrue(
                "onLinkClicked should return true and load via NativePageHost when inline PDF V2"
                        + " is disabled.",
                result);
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
    public void testFragmentOnLinkClicked_AlwaysConsumesEvent() {
        createPdfCoordinator();
        ExternalLink link = mock(ExternalLink.class);
        when(link.getUri()).thenReturn(Uri.parse(LINK_URL));

        boolean handled = mPdfCoordinator.mChromePdfViewerFragment.onLinkClicked(link);
        assertTrue("ChromePdfViewerFragment.onLinkClicked must always return true.", handled);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mNativePageHost).openNewTab(captor.capture());
        assertEquals(LINK_URL, captor.getValue().getUrl());
        assertTrue(captor.getValue().getIsRendererInitiated());
        assertNull(
                "No external activity should be started directly",
                Shadows.shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testFragmentOnLinkClicked_V2Disabled_LoadsThroughNativePageHost() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();
        ExternalLink link = mock(ExternalLink.class);
        when(link.getUri()).thenReturn(Uri.parse(LINK_URL));

        boolean handled = mPdfCoordinator.mChromePdfViewerFragment.onLinkClicked(link);
        assertTrue(
                "ChromePdfViewerFragment.onLinkClicked must return true when V2 is disabled.",
                handled);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mNativePageHost).openNewTab(captor.capture());
        assertEquals(LINK_URL, captor.getValue().getUrl());
        assertTrue(captor.getValue().getIsRendererInitiated());
        assertNull(
                "No external activity should be started directly",
                Shadows.shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    public void testFragmentOnLinkClicked_DisallowedScheme_ConsumesEventAndDropsNavigation() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        createPdfCoordinator();

        String[] dangerousUris = {
            "javascript:alert('XSS')",
            "intent://scan/#Intent;scheme=zxing;package=com.evil.app;end",
            "file:///etc/hosts",
            "content://com.android.contacts/contacts",
            "chrome://flags",
            "data:text/html,test",
        };

        for (String raw : dangerousUris) {
            HistogramWatcher histogramExpectation =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Android.Pdf.Hyperlink.ClickResult",
                            PdfHyperlinkClickResult.BLOCKED_INVALID_SCHEME);
            ExternalLink link = mock(ExternalLink.class);
            when(link.getUri()).thenReturn(Uri.parse(raw));

            boolean handled = mPdfCoordinator.mChromePdfViewerFragment.onLinkClicked(link);
            assertTrue(
                    "ChromePdfViewerFragment.onLinkClicked must consume event even for dangerous"
                            + " URI: "
                            + raw,
                    handled);
            histogramExpectation.assertExpected();
        }
        verify(mNativePageHost, never()).openNewTab(any(LoadUrlParams.class));
        assertNull(
                "No external activity should be started directly",
                Shadows.shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    public void testFragmentOnLinkClicked_NoDelegate_ConsumesEventWithoutError() {
        PdfCoordinator.ChromePdfViewerFragment fragment =
                new PdfCoordinator.ChromePdfViewerFragment();
        ExternalLink link = mock(ExternalLink.class);
        when(link.getUri()).thenReturn(Uri.parse(LINK_URL));

        boolean handled = fragment.onLinkClicked(link);
        assertTrue(
                "ChromePdfViewerFragment.onLinkClicked must return true even with null delegate.",
                handled);
        assertNull(
                "No external activity should be started directly",
                Shadows.shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    public void testFragmentOnLinkClicked_DelegateSetAfterRecreation_Works() {
        createPdfCoordinator();
        PdfCoordinator.ChromePdfViewerFragment fragment =
                new PdfCoordinator.ChromePdfViewerFragment();
        ExternalLink link = mock(ExternalLink.class);
        when(link.getUri()).thenReturn(Uri.parse(LINK_URL));

        // Before delegate is attached: consumes event, does not crash.
        assertTrue(fragment.onLinkClicked(link));
        verify(mNativePageHost, never()).openNewTab(any(LoadUrlParams.class));

        // Attach delegate.
        fragment.setDelegate(mPdfCoordinator);

        // After delegate is attached: consumes event and routes navigation.
        assertTrue(fragment.onLinkClicked(link));
        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mNativePageHost).openNewTab(captor.capture());
        assertEquals(LINK_URL, captor.getValue().getUrl());
        assertNull(
                "No external activity should be started directly",
                Shadows.shadowOf(mActivity).getNextStartedActivity());
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
                        mTab,
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
        PageInfo equalPageInfo = new PageInfo(0, 400, 200, Collections.emptyList());
        float zoomPage =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        equalPageInfo, /* fitToPage= */ true, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(2.5f, zoomPage, 0.001f);

        // 2. Tall page (height-constrained): content 800 height, 200 width
        // zoomHeight = 1000 / 800 = 1.25f, zoomWidth = 500 / 200 = 2.5f -> min = 1.25f
        PageInfo tallPageInfo = new PageInfo(0, 800, 200, Collections.emptyList());
        float zoomTallPage =
                mPdfCoordinator.mChromePdfViewerFragment.calculateFitToPageZoom(
                        tallPageInfo, /* fitToPage= */ true, mPdfView, /* zoomRatio= */ 1.0f);
        assertEquals(1.25f, zoomTallPage, 0.001f);

        // 3. Wide page (width-constrained): content 400 height, 400 width
        // zoomHeight = 1000 / 400 = 2.5f, zoomWidth = 500 / 400 = 1.25f -> min = 1.25f
        PageInfo widePageInfo = new PageInfo(0, 400, 400, Collections.emptyList());
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
        PageInfo tallPageInfo = new PageInfo(0, 800, 200, Collections.emptyList());

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

        PageInfo realPageInfo = new PageInfo(0, 400, 200, Collections.emptyList());

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
                        mFilePath,
                        PDF_TITLE,
                        mTab,
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
                                                new PageInfo(0, 400, 200, Collections.emptyList());
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
                                                        Collections.emptyList());
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
                        mFilePath,
                        PDF_TITLE,
                        mTab,
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

        View container =
                mPdfCoordinator.getView().findViewById(mPdfCoordinator.mFragmentContainerViewId);
        assertEquals(View.INVISIBLE, container.getVisibility());

        // Set document load start timestamp to simulate that load started.
        mPdfCoordinator.mChromePdfViewerFragment.mDocumentLoadStartTimestamp = 12345L;

        // Trigger error.
        mPdfCoordinator.mChromePdfViewerFragment.onLoadDocumentError(
                new RuntimeException("Test error"));

        // Verify container is now VISIBLE.
        assertEquals(View.VISIBLE, container.getVisibility());
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

        // Simulate fragment entering edit mode (from native FAB)
        mPdfCoordinator.mChromePdfViewerFragment.onEnterEditMode();
        assertTrue("Edit button should be selected after onEnterEditMode", editButton.isSelected());
        assertTrue(mUserActionTester.getActions().contains("Android.Pdf.EditFab"));

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
        View placeholderView = new View(mActivity);
        try {
            fragment.onViewCreated(placeholderView, savedInstanceState2);
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
        View placeholderView = new View(mActivity);
        try {
            newFragment.onViewCreated(placeholderView, null);
        } catch (Throwable t) {
            // Ignore exceptions from super.onViewCreated.
        }

        // Verify new fragment has restored values using reflection.
        Field pageField =
                PdfCoordinator.ChromePdfViewerFragment.class.getDeclaredField("mSavedPageIndex");
        pageField.setAccessible(true);
        int savedPageIndex = (int) pageField.get(newFragment);
        assertEquals(5, savedPageIndex);

        Field zoomField =
                PdfCoordinator.ChromePdfViewerFragment.class.getDeclaredField("mSavedZoom");
        zoomField.setAccessible(true);
        float savedZoom = (float) zoomField.get(newFragment);
        assertEquals(2.5f, savedZoom, 0.001f);

        Field pendingField =
                PdfCoordinator.ChromePdfViewerFragment.class.getDeclaredField(
                        "mRestorePositionPending");
        pendingField.setAccessible(true);
        boolean restorePositionPending = (boolean) pendingField.get(newFragment);
        assertTrue(restorePositionPending);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_NoChanges() {
        createPdfCoordinator();
        BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
        assertNotNull(callback);

        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(false).when(spyFragment).hasUnsavedChanges();

        boolean[] proceedCalled = new boolean[1];
        boolean[] cancelCalled = new boolean[1];
        Runnable onProceed = () -> proceedCalled[0] = true;
        Runnable onCancel = () -> cancelCalled[0] = true;

        boolean intercepted = callback.handleBeforeUnload(onProceed, onCancel);
        assertFalse(intercepted);
        assertFalse(proceedCalled[0]);
        assertFalse(cancelCalled[0]);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_WithUnsavedChanges_Proceed() throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(TEST_CONTENT_URI));
            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
            assertNotNull(callback);

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).isAdded();
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            boolean[] proceedCalled = new boolean[1];
            boolean[] cancelCalled = new boolean[1];
            Runnable onProceed = () -> proceedCalled[0] = true;
            Runnable onCancel = () -> cancelCalled[0] = true;

            boolean intercepted = callback.handleBeforeUnload(onProceed, onCancel);
            assertTrue(intercepted);

            PropertyModel dialogModel = fakeModalDialogManager.getShownDialogModel();
            assertNotNull("Modal dialog should be shown", dialogModel);
            assertEquals("Leave site?", dialogModel.get(ModalDialogProperties.TITLE));

            fakeModalDialogManager.clickPositiveButton();

            assertTrue(proceedCalled[0]);
            assertFalse(cancelCalled[0]);
            assertNull(fakeModalDialogManager.getShownDialogModel());
            assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_WithAppliedChanges_Proceed() throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
            assertNotNull(callback);

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(false).when(spyFragment).hasUnsavedChanges();

            pdfCoordinator.onEditsApplied();
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

            boolean[] proceedCalled = new boolean[1];
            boolean[] cancelCalled = new boolean[1];
            Runnable onProceed = () -> proceedCalled[0] = true;
            Runnable onCancel = () -> cancelCalled[0] = true;

            boolean intercepted = callback.handleBeforeUnload(onProceed, onCancel);
            assertTrue(intercepted);

            PropertyModel dialogModel = fakeModalDialogManager.getShownDialogModel();
            assertNotNull("Modal dialog should be shown", dialogModel);
            assertEquals("Leave site?", dialogModel.get(ModalDialogProperties.TITLE));

            fakeModalDialogManager.clickPositiveButton();

            assertTrue(proceedCalled[0]);
            assertFalse(cancelCalled[0]);
            assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_WithUnsavedChanges_Cancel() throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(TEST_CONTENT_URI));
            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
            assertNotNull(callback);

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).isAdded();
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            boolean[] proceedCalled = new boolean[1];
            boolean[] cancelCalled = new boolean[1];
            Runnable onProceed = () -> proceedCalled[0] = true;
            Runnable onCancel = () -> cancelCalled[0] = true;

            boolean intercepted = callback.handleBeforeUnload(onProceed, onCancel);
            assertTrue(intercepted);

            PropertyModel dialogModel = fakeModalDialogManager.getShownDialogModel();
            assertNotNull("Modal dialog should be shown", dialogModel);

            fakeModalDialogManager.clickNegativeButton();

            assertFalse(proceedCalled[0]);
            assertTrue(cancelCalled[0]);
            assertNull(fakeModalDialogManager.getShownDialogModel());
            assertFalse(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_DestroyDismissesModalDialog() throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
            assertNotNull(callback);

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).isAdded();
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            boolean intercepted = callback.handleBeforeUnload(() -> {}, () -> {});
            assertTrue(intercepted);
            assertNotNull(fakeModalDialogManager.getShownDialogModel());

            pdfCoordinator.destroy();
            assertNull(fakeModalDialogManager.getShownDialogModel());
        }
    }

    @Test
    public void testBeforeUnload_DestroyRemovesCallbackFromUserDataHost() {
        createPdfCoordinator();
        assertNotNull(mUserDataHost.getUserData(BeforeUnloadCallback.class));

        mPdfCoordinator.destroy();
        assertNull(mUserDataHost.getUserData(BeforeUnloadCallback.class));
    }

    @Test
    public void testBeforeUnload_DestroyWhenTabDestroyed_DoesNotThrow() {
        createPdfCoordinator();
        when(mTab.isDestroyed()).thenReturn(true);
        mUserDataHost.destroy();

        // Should not throw IllegalStateException when destroying while tab is destroyed.
        mPdfCoordinator.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_DestroyDoesNotCallCancelRunnable() throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
            assertNotNull(callback);

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).isAdded();
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            boolean[] cancelCalled = new boolean[1];
            boolean intercepted =
                    callback.handleBeforeUnload(() -> {}, () -> cancelCalled[0] = true);
            assertTrue(intercepted);
            assertNotNull(fakeModalDialogManager.getShownDialogModel());

            pdfCoordinator.destroy();
            assertFalse(
                    "onCancel should not be called when activity/tab is destroyed",
                    cancelCalled[0]);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_MultipleInvocations_ReplacesDialogAndCancelsPrevious()
            throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
                            PDF_URL,
                            mPdfFragmentViewTracker);
            PdfView pdfView = new PdfView(customActivity);
            pdfView.layout(0, 0, 500, PDF_CONTENT_HEIGHT);
            pdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(pdfView);
            ViewGroup contentView = customActivity.findViewById(android.R.id.content);
            contentView.addView(pdfCoordinator.getView());
            ShadowLooper.idleMainLooper();

            BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
            assertNotNull(callback);

            PdfCoordinator.ChromePdfViewerFragment originalFragment =
                    pdfCoordinator.mChromePdfViewerFragment;
            PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
            pdfCoordinator.mChromePdfViewerFragment = spyFragment;
            doReturn(true).when(spyFragment).isAdded();
            doReturn(true).when(spyFragment).hasUnsavedChanges();

            boolean[] proceed1Called = new boolean[1];
            boolean[] cancel1Called = new boolean[1];
            boolean[] proceed2Called = new boolean[1];
            boolean[] cancel2Called = new boolean[1];

            // First beforeunload trigger
            callback.handleBeforeUnload(
                    () -> proceed1Called[0] = true, () -> cancel1Called[0] = true);
            assertNotNull(fakeModalDialogManager.getShownDialogModel());

            // Second beforeunload trigger while first is still showing
            callback.handleBeforeUnload(
                    () -> proceed2Called[0] = true, () -> cancel2Called[0] = true);
            assertTrue(
                    "Previous invocation onCancel should be called when replaced",
                    cancel1Called[0]);
            assertFalse(proceed1Called[0]);

            // Now confirm the second dialog
            fakeModalDialogManager.clickPositiveButton();
            assertTrue("Second invocation onProceed should be called", proceed2Called[0]);
            assertFalse(cancel2Called[0]);
            assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_WithUnsavedChanges_AlertDialog() {
        createPdfCoordinator();
        BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
        assertNotNull(callback);

        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(true).when(spyFragment).isAdded();
        doReturn(true).when(spyFragment).hasUnsavedChanges();

        boolean[] proceedCalled = new boolean[1];
        boolean[] cancelCalled = new boolean[1];
        Runnable onProceed = () -> proceedCalled[0] = true;
        Runnable onCancel = () -> cancelCalled[0] = true;

        boolean intercepted = callback.handleBeforeUnload(onProceed, onCancel);
        assertTrue(intercepted);

        AlertDialog alertDialog = mPdfCoordinator.getAlertDialogForTesting();
        assertNotNull(alertDialog);
        assertTrue(alertDialog.isShowing());

        alertDialog.getButton(DialogInterface.BUTTON_POSITIVE).performClick();
        ShadowLooper.idleMainLooper();

        assertTrue(proceedCalled[0]);
        assertFalse(cancelCalled[0]);
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
        assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testBeforeUnload_WithUnsavedChanges_AlertDialog_Cancel() {
        createPdfCoordinator();
        BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
        assertNotNull(callback);

        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(true).when(spyFragment).isAdded();
        doReturn(true).when(spyFragment).hasUnsavedChanges();

        boolean[] proceedCalled = new boolean[1];
        boolean[] cancelCalled = new boolean[1];
        Runnable onProceed = () -> proceedCalled[0] = true;
        Runnable onCancel = () -> cancelCalled[0] = true;

        boolean intercepted = callback.handleBeforeUnload(onProceed, onCancel);
        assertTrue(intercepted);

        AlertDialog alertDialog = mPdfCoordinator.getAlertDialogForTesting();
        assertNotNull(alertDialog);
        assertTrue(alertDialog.isShowing());

        alertDialog.getButton(DialogInterface.BUTTON_NEGATIVE).performClick();
        ShadowLooper.idleMainLooper();

        assertFalse(proceedCalled[0]);
        assertTrue(cancelCalled[0]);
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
        assertFalse(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void
            testBeforeUnload_MultipleInvocations_AlertDialog_ReplacesDialogAndCancelsPrevious() {
        createPdfCoordinator();
        BeforeUnloadCallback callback = mUserDataHost.getUserData(BeforeUnloadCallback.class);
        assertNotNull(callback);

        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(true).when(spyFragment).isAdded();
        doReturn(true).when(spyFragment).hasUnsavedChanges();

        boolean[] proceed1Called = new boolean[1];
        boolean[] cancel1Called = new boolean[1];
        boolean[] proceed2Called = new boolean[1];
        boolean[] cancel2Called = new boolean[1];

        // First beforeunload trigger
        callback.handleBeforeUnload(() -> proceed1Called[0] = true, () -> cancel1Called[0] = true);
        AlertDialog alertDialog1 = mPdfCoordinator.getAlertDialogForTesting();
        assertNotNull(alertDialog1);
        assertTrue(alertDialog1.isShowing());

        // Second beforeunload trigger while first is still showing
        callback.handleBeforeUnload(() -> proceed2Called[0] = true, () -> cancel2Called[0] = true);
        assertTrue("Previous invocation onCancel should be called when replaced", cancel1Called[0]);
        assertFalse(proceed1Called[0]);

        AlertDialog alertDialog2 = mPdfCoordinator.getAlertDialogForTesting();
        assertNotNull(alertDialog2);
        assertTrue(alertDialog2.isShowing());

        // Now confirm the second dialog
        alertDialog2.getButton(DialogInterface.BUTTON_POSITIVE).performClick();
        ShadowLooper.idleMainLooper();

        assertTrue("Second invocation onProceed should be called", proceed2Called[0]);
        assertFalse(cancel2Called[0]);
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
        assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
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

        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);
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

    public static class TestModalDialogActivity extends TestActivity
            implements ModalDialogManagerHolder {
        private ModalDialogManager mModalDialogManager;

        public void setModalDialogManager(ModalDialogManager manager) {
            mModalDialogManager = manager;
        }

        @Override
        public ModalDialogManager getModalDialogManager() {
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
                        mTab,
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

        AlertDialog latestDialog = (AlertDialog) ShadowDialog.getLatestDialog();
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
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
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
                            mTab,
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
                                                            0, 400, 200, Collections.emptyList());
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

            View dialogCustomView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
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
    public void testEnterEditMode() {
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);

        mPdfCoordinator.enterEditMode();

        assertTrue(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());

        mPdfCoordinator.mChromePdfViewerFragment.onEnterEditMode();
        assertTrue(mUserActionTester.getActions().contains("Android.Pdf.EditFab"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testEnterExitEditMode_EditDisabled() {
        PdfUtils.setInlinePdfV2EditEnabledForTesting(false);
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        mPdfCoordinator.enterEditMode();
        assertNull(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());

        mPdfCoordinator.exitEditMode();
        assertNull(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testExitEditMode_NoUnsavedChanges() {
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(false);

        mPdfCoordinator.exitEditMode();

        assertFalse(shadowFragment.getEditModeEnabled());
        assertFalse(shadowFragment.wasApplyDraftEditsCalled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testExitEditMode_WithUnsavedChanges_Flow() throws Exception {
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

        mPdfCoordinator.exitEditMode();

        assertTrue(shadowFragment.wasApplyDraftEditsCalled());
        assertEquals(null, shadowFragment.getEditModeEnabled());

        // Simulate success callback with fake
        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        assertTrue(fakeHandle.mWriteToCalled);
        assertNotNull(fakeHandle.mContinuation);

        // Resume continuation to finish write
        fakeHandle.mContinuation.resumeWith(Unit.INSTANCE);

        // Run posted tasks on UI thread (finishExitingEditMode is posted)
        ShadowLooper.idleMainLooper();

        // Now it should be disabled
        assertFalse(shadowFragment.getEditModeEnabled());
        assertTrue(fakeHandle.mClosed);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testExitEditMode_WithUnsavedChanges_AsyncFailureFlow() throws Exception {
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

        mPdfCoordinator.exitEditMode();

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Resume continuation with failure
        Object failure = new IOException("Test exception");
        fakeHandle.mContinuation.resumeWith(failure);

        ShadowLooper.idleMainLooper();

        // It should still disable edit mode and close handles
        assertFalse(shadowFragment.getEditModeEnabled());
        assertTrue(fakeHandle.mClosed);
        pfd.close();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testExitEditMode_WithUnsavedChanges_SyncSuccessFlow() throws Exception {
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

        mPdfCoordinator.exitEditMode();

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        // Make it return Unit.INSTANCE to simulate sync completion
        fakeHandle.mResult = Unit.INSTANCE;

        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Run posted tasks (finishExitingEditMode is posted)
        ShadowLooper.idleMainLooper();

        // For sync completion, it should finish immediately
        assertFalse(shadowFragment.getEditModeEnabled());
        assertTrue(fakeHandle.mClosed);
        pfd.close();
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
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(TEST_CONTENT_URI));
            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
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
            doReturn(true).when(spyFragment).isAdded();
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
            assertFalse(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithUnsavedChanges_Confirm() throws Exception {
        try (var controller = Robolectric.buildActivity(TestModalDialogActivity.class)) {
            TestModalDialogActivity customActivity = controller.get();
            customActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
            controller.setup();
            FakeModalDialogManager fakeModalDialogManager =
                    new FakeModalDialogManager(ModalDialogType.TAB);
            customActivity.setModalDialogManager(fakeModalDialogManager);

            ChromeFileProvider.setGeneratedUriForTesting(Uri.parse(TEST_CONTENT_URI));
            PdfCoordinator pdfCoordinator =
                    new PdfCoordinator(
                            mNativePageHost,
                            mProfile,
                            customActivity,
                            mFilePath,
                            PDF_TITLE,
                            mTab,
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
            doReturn(true).when(spyFragment).isAdded();
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
            assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithUnsavedChanges_AlertDialog_Confirm() {
        createPdfCoordinator();
        AtomicBoolean confirmed = new AtomicBoolean(false);
        mPdfCoordinator.showReloadConfirmationDialog(() -> confirmed.set(true));

        AlertDialog latestDialog = (AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Dialog should be shown", latestDialog);
        assertTrue("Dialog should be showing", latestDialog.isShowing());

        latestDialog.getButton(DialogInterface.BUTTON_POSITIVE).performClick();
        ShadowLooper.idleMainLooper();

        assertTrue(confirmed.get());
        assertTrue(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithUnsavedChanges_AlertDialog_Cancel() {
        createPdfCoordinator();
        AtomicBoolean confirmed = new AtomicBoolean(false);
        mPdfCoordinator.showReloadConfirmationDialog(() -> confirmed.set(true));

        AlertDialog latestDialog = (AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Dialog should be shown", latestDialog);
        assertTrue("Dialog should be showing", latestDialog.isShowing());

        latestDialog.getButton(DialogInterface.BUTTON_NEGATIVE).performClick();
        ShadowLooper.idleMainLooper();

        assertFalse(confirmed.get());
        assertFalse(mUserActionTester.getActions().contains("Android.Pdf.DiscardAnnotations"));
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

        mPdfCoordinator.exitEditMode();

        FakePdfWriteHandle fakeHandle =
                new FakePdfWriteHandle() {
                    @Override
                    public Object writeTo(
                            ParcelFileDescriptor destination,
                            Continuation<? super Unit> continuation) {
                        super.writeTo(destination, continuation);
                        throw new RuntimeException("Test runtime exception during writeTo");
                    }
                };

        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Run posted tasks on UI thread (finishExitingEditMode is posted)
        ShadowLooper.idleMainLooper();

        // Even with RuntimeException, it should close handles and disable edit mode
        assertTrue(fakeHandle.mClosed);
        assertFalse(shadowFragment.getEditModeEnabled());

        // Also check if destination pfd is closed.
        boolean pfdClosed = false;
        try {
            if (fakeHandle.mDestination != null) {
                fakeHandle.mDestination.getFd();
            }
        } catch (IllegalStateException e) {
            pfdClosed = true;
        }
        assertTrue("ParcelFileDescriptor should be closed", pfdClosed);
        pfd.close();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testDownload_WithUnsavedChanges_TriggersApplyDraftEdits() {
        createPdfCoordinator();

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        mPdfCoordinator.download();

        assertTrue(shadowFragment.wasApplyDraftEditsCalled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_WithAnnotations_ShowsDownloadingToastImmediately() {
        createPdfCoordinator();
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);
        mPdfCoordinator.onEditsApplied();

        mPdfCoordinator.download();

        assertEquals(
                mActivity.getString(R.string.pdf_downloading), ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_NoAnnotations_CallsHostDownload() {
        createPdfCoordinator();
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mPdfCoordinator.download();

        verify(mNativePageHost).downloadUrl(PDF_DOWNLOAD_URL);
        assertNull(ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_NoAnnotations_LocalPdf_IsNoOp() {
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        mFilePath,
                        PDF_TITLE,
                        mTab,
                        TEST_CONTENT_URI,
                        mPdfFragmentViewTracker);
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mPdfCoordinator.download();

        verify(mNativePageHost, never()).downloadUrl(any());
        assertNull(ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_NoAnnotations_BlobPdf_CallsHostDownload() {
        String blobUrl = "blob:https://www.irs.gov/1234-5678";
        String encodedBlobPdfUrl = PdfUtils.encodePdfPageUrl(blobUrl);
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        mFilePath,
                        PDF_TITLE,
                        mTab,
                        encodedBlobPdfUrl,
                        mPdfFragmentViewTracker);
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mPdfCoordinator.download();

        verify(mNativePageHost).downloadUrl(blobUrl);
        assertNull(ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_NoAnnotations_DataPdf_CallsHostDownload() {
        String dataUrl = "data:application/pdf;base64,JVBERi0xLjc=";
        String encodedDataPdfUrl = PdfUtils.encodePdfPageUrl(dataUrl);
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        mFilePath,
                        PDF_TITLE,
                        mTab,
                        encodedDataPdfUrl,
                        mPdfFragmentViewTracker);
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mPdfCoordinator.download();

        verify(mNativePageHost).downloadUrl(dataUrl);
        assertNull(ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_NoAnnotations_DisallowedScheme_IsNoOp() {
        String disallowedUrl = "javascript:alert(1)";
        String encodedUrl = PdfUtils.encodePdfPageUrl(disallowedUrl);
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        mFilePath,
                        PDF_TITLE,
                        mTab,
                        encodedUrl,
                        mPdfFragmentViewTracker);
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mPdfCoordinator.download();

        verify(mNativePageHost, never()).downloadUrl(any());
        assertNull(ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_WithAnnotations_LocalPdf_CallsDownloadAnnotatedPdf() {
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        mFilePath,
                        PDF_TITLE,
                        mTab,
                        TEST_CONTENT_URI,
                        mPdfFragmentViewTracker);
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);
        mPdfCoordinator.onEditsApplied();

        mPdfCoordinator.download();

        verify(mNativePageHost, never()).downloadUrl(any());
        assertEquals(
                mActivity.getString(R.string.pdf_downloading), ShadowToast.getTextOfLatestToast());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDownload_FeatureDisabled_CallsHostDownload() {
        createPdfCoordinator();
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);
        mPdfCoordinator.onEditsApplied();

        mPdfCoordinator.download();

        verify(mNativePageHost).downloadUrl(PDF_DOWNLOAD_URL);
        assertNull(ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testOnPdfEditsSaved_TriggersDownloadIfDownloadAfterSave() throws Exception {
        createPdfCoordinator();
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        mPdfCoordinator.download();
        assertTrue(shadowFragment.wasApplyDraftEditsCalled());
        shadowFragment.setHasUnsavedChanges(false);

        File tempFile = File.createTempFile("test_saved_pdf", ".pdf");
        tempFile.deleteOnExit();
        java.util.concurrent.atomic.AtomicBoolean onDoneCalled =
                new java.util.concurrent.atomic.AtomicBoolean(false);

        mPdfCoordinator.onPdfEditsSaved(tempFile, /* pfd= */ null, () -> onDoneCalled.set(true));

        ShadowLooper.idleMainLooper();
        assertTrue(onDoneCalled.get());
        assertTrue(mPdfCoordinator.hasChanges());
        assertEquals(
                mActivity.getString(R.string.pdf_downloading), ShadowToast.getTextOfLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testOnPdfEditsSaveFailed_ResetsDownloadAfterSave() throws Exception {
        createPdfCoordinator();
        mPdfCoordinator.onDownloadComplete(mFilePath, PDF_TITLE);

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(mPdfCoordinator.mChromePdfViewerFragment, "test_pdf_tag")
                .commitNow();

        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(true);

        mPdfCoordinator.download();
        assertTrue(shadowFragment.wasApplyDraftEditsCalled());

        mPdfCoordinator.onPdfEditsSaveFailed();

        // Subsequent save without download flag should not trigger download toast
        File tempFile = File.createTempFile("test_saved_pdf2", ".pdf");
        tempFile.deleteOnExit();
        ShadowToast.reset();

        mPdfCoordinator.onPdfEditsSaved(tempFile, /* pfd= */ null, () -> {});
        ShadowLooper.idleMainLooper();
        assertNull(ShadowToast.getLatestToast());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testOnApplyEditsSuccess_CallsDelegateOnPdfEditsSaved() throws Exception {
        PdfActionsDelegate mockDelegate = Mockito.mock(PdfActionsDelegate.class);
        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mockDelegate);

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "test_custom_delegate_tag")
                .commitNow();

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        fakeHandle.mResult = Unit.INSTANCE;

        fragment.onApplyEditsSuccess(fakeHandle);

        ShadowLooper.idleMainLooper();
        verify(mockDelegate).onPdfEditsSaved(any(File.class), isNull(), any(Runnable.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(shadows = {ShadowEditablePdfViewerFragment.class})
    public void testOnApplyEditsSuccess_Incognito_UsesMemfdAndNoTempFileOnDisk() throws Exception {
        PdfActionsDelegate mockDelegate = Mockito.mock(PdfActionsDelegate.class);
        when(mockDelegate.isIncognito()).thenReturn(true);
        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mockDelegate);

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "test_incognito_custom_delegate_tag")
                .commitNow();

        File pdfsDir = new File(mActivity.getCacheDir(), "pdfs");
        int fileCountBefore =
                pdfsDir.exists() && pdfsDir.list() != null ? pdfsDir.list().length : 0;

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();
        fakeHandle.mResult = Unit.INSTANCE;

        fragment.onApplyEditsSuccess(fakeHandle);

        ShadowLooper.idleMainLooper();
        assertTrue(fakeHandle.mWriteToCalled);
        assertNotNull(fakeHandle.mDestination);
        verify(mockDelegate)
                .onPdfEditsSaved(isNull(), any(ParcelFileDescriptor.class), any(Runnable.class));

        // Ensure no temporary file was created on disk in Incognito.
        int fileCountAfter = pdfsDir.exists() && pdfsDir.list() != null ? pdfsDir.list().length : 0;
        assertEquals(fileCountBefore, fileCountAfter);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    @Config(
            shadows = {
                ShadowEditablePdfViewerFragment.class,
                CustomShadowParcelFileDescriptor.class
            })
    public void testOnPdfEditsSaved_Incognito_RegistersStream() throws Exception {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        createPdfCoordinator();

        File tempFile = File.createTempFile("test_saved_incognito", ".pdf");
        tempFile.deleteOnExit();
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_WRITE);

        java.util.concurrent.atomic.AtomicBoolean onDoneCalled =
                new java.util.concurrent.atomic.AtomicBoolean(false);

        mPdfCoordinator.onPdfEditsSaved(null, pfd, () -> onDoneCalled.set(true));

        ShadowLooper.idleMainLooper();
        assertTrue(onDoneCalled.get());
        assertTrue(mPdfCoordinator.hasChanges());
        assertNotNull(mPdfCoordinator.getUri());
        assertTrue(mPdfCoordinator.getUri().getAuthority().endsWith(".PdfContentProvider"));
        assertEquals(
                mPdfCoordinator.getUri(),
                mPdfCoordinator.mChromePdfViewerFragment.getDocumentUri());
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
        Shadows.shadowOf(mActivity.getPackageManager())
                .addResolveInfoForIntent(intent, resolveInfo);

        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mPdfCoordinator);
        fragment.setDocumentUri(mPdfCoordinator.getUri());
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
        Shadows.shadowOf(mActivity.getPackageManager())
                .addResolveInfoForIntent(intent, resolveInfo);

        TestChromePdfViewerFragment fragment = new TestChromePdfViewerFragment(mPdfCoordinator);
        fragment.setDocumentUri(mPdfCoordinator.getUri());
        mPdfCoordinator.mChromePdfViewerFragment = fragment;
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "test_pdf_tag_3")
                .commitNow();

        FrameLayout fragmentView = new FrameLayout(mActivity);
        FrameLayout toolBoxView = new FrameLayout(mActivity);
        toolBoxView.setId(R.id.toolBoxView);
        View childView = new View(mActivity);
        toolBoxView.addView(childView);
        fragmentView.addView(toolBoxView);
        fragment.onViewCreated(fragmentView, null);

        childView.performClick();
        assertTrue(mUserActionTester.getActions().contains("Android.Pdf.EditFab"));

        Intent startedIntent = Shadows.shadowOf(mActivity).getNextStartedActivity();
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
        mPdfCoordinator.exitEditMode();

        FakePdfWriteHandle fakeHandle = new FakePdfWriteHandle();

        // Trigger onApplyEditsSuccess
        mPdfCoordinator.mChromePdfViewerFragment.onApplyEditsSuccess(fakeHandle);

        // Resume continuation to simulate success
        assertNotNull(fakeHandle.mContinuation);
        fakeHandle.mContinuation.resumeWith(Unit.INSTANCE);

        // Wait for post tasks to run (finishExitingEditMode is posted)
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Now reload should show confirmation dialog.
        mPdfCoordinator.reload();

        // Verify dialog is shown.
        AlertDialog latestDialog = (AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("Dialog should be shown", latestDialog);
        assertTrue("Dialog should be showing", latestDialog.isShowing());
        pfd.close();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testReload_WithoutChanges_DoesNotShowConfirmation() throws Exception {
        createPdfCoordinator();
        ShadowEditablePdfViewerFragment shadowFragment =
                Shadow.extract(mPdfCoordinator.mChromePdfViewerFragment);
        shadowFragment.setHasUnsavedChanges(false);

        mPdfCoordinator.reload();

        AlertDialog latestDialog = (AlertDialog) ShadowDialog.getLatestDialog();
        if (latestDialog != null) {
            assertFalse("Dialog should not be showing", latestDialog.isShowing());
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testDestroy_DismissesAlertDialog() {
        createPdfCoordinator();
        mPdfCoordinator.showReloadConfirmationDialog(() -> {});

        AlertDialog latestDialog = (AlertDialog) ShadowDialog.getLatestDialog();
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

        AlertDialog firstDialog = (AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull("First dialog should be shown", firstDialog);
        assertTrue("First dialog should be showing", firstDialog.isShowing());
        assertSame(firstDialog, mPdfCoordinator.getAlertDialogForTesting());

        mPdfCoordinator.showReloadConfirmationDialog(() -> {});

        AlertDialog secondDialog = (AlertDialog) ShadowDialog.getLatestDialog();
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

        AlertDialog dialog = (AlertDialog) ShadowDialog.getLatestDialog();
        assertNotNull(dialog);
        assertSame(dialog, mPdfCoordinator.getAlertDialogForTesting());

        dialog.dismiss();
        ShadowLooper.idleMainLooper();
        assertNull(mPdfCoordinator.getAlertDialogForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testHasUnsavedChanges_FragmentNotAdded_ReturnsFalse() {
        createPdfCoordinator();
        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(false).when(spyFragment).isAdded();

        assertFalse(mPdfCoordinator.hasUnsavedChanges());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testHasUnsavedChanges_ThrowsIllegalStateException_ReturnsFalse() {
        createPdfCoordinator();
        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(true).when(spyFragment).isAdded();
        doThrow(new IllegalStateException("Fragment not attached"))
                .when(spyFragment)
                .hasUnsavedChanges();

        assertFalse(mPdfCoordinator.hasUnsavedChanges());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testHasUnsavedChanges_WithUnsavedChanges_ReturnsTrue() {
        createPdfCoordinator();
        PdfCoordinator.ChromePdfViewerFragment originalFragment =
                mPdfCoordinator.mChromePdfViewerFragment;
        PdfCoordinator.ChromePdfViewerFragment spyFragment = spy(originalFragment);
        mPdfCoordinator.mChromePdfViewerFragment = spyFragment;
        doReturn(true).when(spyFragment).isAdded();
        doReturn(true).when(spyFragment).hasUnsavedChanges();

        assertTrue(mPdfCoordinator.hasUnsavedChanges());
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
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            return new FrameLayout(inflater.getContext());
        }

        @Implementation
        public void onViewCreated(View view, Bundle savedInstanceState) {
            // Do nothing to avoid findViewById crashes on placeholder view
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
            Shadow.directlyOn(mRealFragment, Fragment.class, "onDestroyView");
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
            Shadow.directlyOn(mRealFragment, Fragment.class, "onStart");
        }

        @Implementation
        public void onResume() {
            Shadow.directlyOn(mRealFragment, Fragment.class, "onResume");
        }

        @Implementation
        public void onPause() {
            Shadow.directlyOn(mRealFragment, Fragment.class, "onPause");
        }

        @Implementation
        public void onStop() {
            Shadow.directlyOn(mRealFragment, Fragment.class, "onStop");
        }
    }

    public static class FakePdfWriteHandle implements PdfWriteHandle {
        public boolean mClosed;
        public boolean mWriteToCalled;
        public ParcelFileDescriptor mDestination;
        public Continuation<? super Unit> mContinuation;
        public Object mResult = IntrinsicsKt.getCOROUTINE_SUSPENDED();

        @Override
        public Object writeTo(
                ParcelFileDescriptor destination, Continuation<? super Unit> continuation) {
            mWriteToCalled = true;
            mDestination = destination;
            mContinuation = continuation;
            try {
                new FileOutputStream(destination.getFileDescriptor()).write(new byte[] {1, 2, 3});
            } catch (Exception ignored) {
            }
            return mResult;
        }

        @Override
        public void close() throws IOException {
            mClosed = true;
        }
    }

    @Implements(ParcelFileDescriptor.class)
    public static class CustomShadowParcelFileDescriptor extends ShadowParcelFileDescriptor {
        @Implementation
        protected static ParcelFileDescriptor fromFd(int fd) throws IOException {
            File tempFile = File.createTempFile("shadow_pfd", ".pdf");
            tempFile.deleteOnExit();
            return ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_ONLY);
        }

        @Implementation
        public int detachFd() {
            return 1023;
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
