// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
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
                        PDF_URL);
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
    @Config(shadows = {ShadowPdfView.class})
    public void testZoomButtons() {
        createPdfCoordinator();

        View zoomInButton = mPdfCoordinator.getView().findViewById(R.id.zoom_increase_button);
        View zoomOutButton = mPdfCoordinator.getView().findViewById(R.id.zoom_decrease_button);

        // Initial state at normal zoom
        mPdfCoordinator.onViewportChanged(0, 1.0f);
        assertTrue("Zoom in button should be enabled at 1.0f zoom", zoomInButton.isEnabled());
        assertTrue("Zoom out button should be enabled at 1.0f zoom", zoomOutButton.isEnabled());

        // Click zoom in
        zoomInButton.performClick();

        // Assert zoom level increased
        ShadowPdfView shadowPdfView = Shadow.extract(mPdfView);
        assertEquals(1.1f, shadowPdfView.mZoom, 0.001f);

        // Simulate minimum zoom level (0.25f)
        mPdfCoordinator.onViewportChanged(0, 0.25f);
        assertTrue("Zoom in button should be enabled at min zoom", zoomInButton.isEnabled());
        assertFalse("Zoom out button should be disabled at min zoom", zoomOutButton.isEnabled());

        // Simulate maximum zoom level (5.0f)
        mPdfCoordinator.onViewportChanged(0, 5.0f);
        assertFalse("Zoom in button should be disabled at max zoom", zoomInButton.isEnabled());
        assertTrue("Zoom out button should be enabled at max zoom", zoomOutButton.isEnabled());
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
        assertTrue("onLinkClicked should return true.", result);
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
    public void testFragmentCanBeInstantiated() {
        // This test verifies that the fragment can be instantiated by the FragmentManager.
        // The FragmentManager requires a public no-argument constructor.
        try {
            PdfCoordinator.ChromePdfViewerFragment fragment =
                    new PdfCoordinator.ChromePdfViewerFragment();
            assertNotNull("Fragment should be created successfully.", fragment);
        } catch (Exception e) {
            fail("Fragment instantiation should not throw an exception: " + e.getMessage());
        }
    }

    @Test
    public void testGetFileUri() {
        createPdfCoordinator();

        Uri uri =
                mPdfCoordinator.getFileUri(
                        /* isWorkProfile= */ false, "com.google.android.googlequicksearchbox");
        assertNotNull(uri);
        assertEquals(mPdfCoordinator.getUri(), uri);
    }

    @Test
    public void testGetFileUri_NullUri() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost,
                        mProfile,
                        mActivity,
                        null,
                        PDF_TITLE,
                        TAB_ID,
                        PDF_URL);

        Uri uri =
                mPdfCoordinator.getFileUri(
                        /* isWorkProfile= */ false, "com.google.android.googlequicksearchbox");
        assertEquals(null, uri);
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
        public float getZoom() {
            return mZoom;
        }
    }
}
