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
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.fragment.app.FragmentActivity;
import androidx.pdf.PdfPoint;
import androidx.pdf.view.PdfView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
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

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class PdfCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mNativePageHost;
    @Mock private Profile mProfile;

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
    private final List<View> mPdfFragmentViews = new ArrayList<>();

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
                        mPdfFragmentViews);
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
                        mPdfFragmentViews);

        Uri uri =
                mPdfCoordinator.getFileUri(
                        /* isWorkProfile= */ false, "com.google.android.googlequicksearchbox");
        assertEquals(null, uri);
    }

    @Test
    public void testRelocateFragmentViews_removeWrongPdfFragmentViews() {
        createPdfCoordinator();
        String tabId1 = String.valueOf(TAB_ID);
        String tabId2 = String.valueOf(TAB_ID + 1);
        String tabId3 = String.valueOf(TAB_ID + 2);
        var fragment = new PdfCoordinator.ChromePdfViewerFragment();
        var fragmentTagKey = R.id.fragment_container_view_tag;
        var lp =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        View pdfViewerFragmentView1 = Mockito.mock(View.class);
        View pdfViewerFragmentView2 = Mockito.mock(View.class);
        View pdfViewerFragmentView3 = Mockito.mock(View.class);
        when(pdfViewerFragmentView1.getTag()).thenReturn(tabId1);
        when(pdfViewerFragmentView2.getTag()).thenReturn(tabId2);
        when(pdfViewerFragmentView3.getTag()).thenReturn(tabId3);
        when(pdfViewerFragmentView1.getTag(eq(fragmentTagKey))).thenReturn(fragment);
        when(pdfViewerFragmentView2.getTag(eq(fragmentTagKey))).thenReturn(fragment);
        when(pdfViewerFragmentView3.getTag(eq(fragmentTagKey))).thenReturn(fragment);
        when(pdfViewerFragmentView1.getLayoutParams()).thenReturn(lp);
        when(pdfViewerFragmentView2.getLayoutParams()).thenReturn(lp);
        when(pdfViewerFragmentView3.getLayoutParams()).thenReturn(lp);

        ViewGroup pfc = mPdfCoordinator.getView().findViewById(R.id.pdf_fragment_container);

        pfc.addView(pdfViewerFragmentView1);
        pfc.addView(pdfViewerFragmentView2);
        pfc.addView(pdfViewerFragmentView3);
        assertEquals(3, pfc.getChildCount());
        assertEquals(0, mPdfFragmentViews.size());

        mPdfCoordinator.relocatePdfPageViews();
        assertEquals(1, pfc.getChildCount());
        assertEquals(String.valueOf(TAB_ID), pfc.getChildAt(0).getTag());
        assertEquals(2, mPdfFragmentViews.size());
    }

    @Implements(PdfView.class)
    public static class ShadowPdfView extends ShadowView {
        public PdfPoint mPdfPoint;
        public float mZoom = 1.0f;

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
        public void testGetZoom() {}
    }
}
