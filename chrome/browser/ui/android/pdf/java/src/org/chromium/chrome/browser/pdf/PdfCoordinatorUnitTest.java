// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;

import androidx.fragment.app.FragmentActivity;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
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
    private static final String PDF_URL =
            "chrome-native://pdf/link?url=https%3A%2F%2Fwww.irs.gov%2Fpub%2Firs-pdf%2Ffw4.pdf";
    private static final String LINK_URL = "https://www.bar.com";
    private static final String FILE_PATH =
            "/data/user/10/com.google.android.apps.chrome/cache/pdfs/fw4.pdf";
    private static final int TAB_ID = 123;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        PdfCoordinator.skipLoadPdfForTesting(true);
    }

    private void createPdfCoordinator(boolean isIncognito) {
        when(mProfile.isOffTheRecord()).thenReturn(isIncognito);
        // For the purpose of testing, we are using the transient file path and url above when in
        // reality, the file path will not be available for a transient pdf when this constructor
        // is called.
        mPdfCoordinator =
                new PdfCoordinator(
                        mNativePageHost, mProfile, mActivity, FILE_PATH, TAB_ID, PDF_URL);
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
    public void testNavigateToPage() {
        createPdfCoordinator(false);

        // Mock the fragment to isolate the coordinator and avoid calling real viewport scrolling
        // logic which might reach final methods on PdfView.
        PdfCoordinator.ChromePdfViewerFragment mockFragment =
                org.mockito.Mockito.mock(PdfCoordinator.ChromePdfViewerFragment.class);
        mPdfCoordinator.mChromePdfViewerFragment = mockFragment;

        mPdfCoordinator.navigateToPage(2);

        // Verify delegation to the fragment.
        verify(mockFragment).scrollToPage(2);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testCalculateYOffsetPoints() {
        float viewHeightPx = 1000f;
        float zoom = 2.0f;

        // (1000 / 2) / 2.0 = 250f
        float expectedOffset = 250f;
        float actualOffset = PdfCoordinator.calculateYOffsetPoints(viewHeightPx, zoom);

        assertEquals("Y offset calculation should be correct", expectedOffset, actualOffset, 0.01f);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INLINE_PDF_V2)
    public void testNavigateToPage_PdfViewNull() {
        createPdfCoordinator(false);
        mPdfCoordinator.mChromePdfViewerFragment.setPdfViewForTesting(null);
        mPdfCoordinator.navigateToPage(2);
    }

    private void runOnLinkClickedTest(boolean isIncognito) {
        createPdfCoordinator(isIncognito);
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
}
