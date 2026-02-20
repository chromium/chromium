// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;

import androidx.fragment.app.FragmentActivity;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

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
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mNativePageHost;
    @Mock private Profile mProfile;

    private FragmentActivity mActivity;
    private PdfCoordinator mPdfCoordinator;
    private AutoCloseable mCloseableMocks;
    private static final String PDF_URL =
            "chrome-native://pdf/link?url=https%3A%2F%2Fwww.irs.gov%2Fpub%2Firs-pdf%2Ffw4.pdf";
    private static final String LINK_URL = "https://www.bar.com";
    private static final String FILE_PATH =
            "/data/user/10/com.google.android.apps.chrome/cache/pdfs/fw4.pdf";
    private static final int TAB_ID = 123;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        PdfCoordinator.skipLoadPdfForTesting(true);
    }

    @After
    public void tearDown() throws Exception {
        mCloseableMocks.close();
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

    public void runOnLinkClickedTest(boolean isIncognito) {
        createPdfCoordinator(isIncognito);
        Uri linkUri = Uri.parse(LINK_URL);
        boolean result = mPdfCoordinator.onLinkClicked(linkUri);
        Assert.assertTrue("onLinkClicked should return true.", result);
        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mNativePageHost).loadUrl(captor.capture(), eq(isIncognito));
        LoadUrlParams params = captor.getValue();
        Assert.assertEquals("URL should match.", LINK_URL, params.getUrl());
        Assert.assertEquals(
                "Transition type should be LINK.", PageTransition.LINK, params.getTransitionType());
        Assert.assertTrue("isRendererInitiated should be true.", params.getIsRendererInitiated());
        Assert.assertEquals(
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
            Assert.assertNotNull("Fragment should be created successfully.", fragment);
        } catch (Exception e) {
            Assert.fail("Fragment instantiation should not throw an exception: " + e.getMessage());
        }
    }
}
