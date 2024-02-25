// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class PdfPageUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private NativePageHost mMockNativePageHost;
    @Mock private Profile mMockProfile;
    @Mock private DestroyableObservableSupplier<Rect> mMarginSupplier;
    private TestActivity mActivity;
    private AutoCloseable mCloseableMocks;

    private static final String CONTENT_URL = "content://media/external/downloads/1000000022";
    private static final String PDF_LINK = "https://www.foo.com/testfiles/pdf/sample.pdf";
    private static final String FILE_PATH = "/media/external/downloads/sample.pdf";
    private static final String FILE_NAME = "sample.pdf";

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            doReturn(activity).when(mMockNativePageHost).getContext();
                        });
        doReturn(mMarginSupplier).when(mMockNativePageHost).createDefaultMarginSupplier();
    }

    @After
    public void tearDown() throws Exception {
        mCloseableMocks.close();
    }

    @Test
    public void testCreatePdfPage_WithContentUri() {
        PdfPage pdfPage =
                new PdfPage(
                        mMockNativePageHost,
                        mMockProfile,
                        mActivity,
                        CONTENT_URL,
                        FILE_NAME,
                        FILE_PATH);
        Assert.assertNotNull(pdfPage);
        Assert.assertEquals("Pdf page title should match.", FILE_NAME, pdfPage.getTitle());
        Assert.assertEquals(
                "Pdf page host should match.", UrlConstants.PDF_HOST, pdfPage.getHost());
        Assert.assertEquals("Pdf page url should match.", CONTENT_URL, pdfPage.getUrl());
    }

    @Test
    public void testCreatePdfPage_WithPdfLink() {
        PdfPage pdfPage =
                new PdfPage(mMockNativePageHost, mMockProfile, mActivity, PDF_LINK, "", "");
        Assert.assertNotNull(pdfPage);
        pdfPage.onDownloadComplete(FILE_NAME, FILE_PATH);
        Assert.assertEquals("Pdf page title should match.", FILE_NAME, pdfPage.getTitle());
        Assert.assertEquals(
                "Pdf page host should match.", UrlConstants.PDF_HOST, pdfPage.getHost());
        Assert.assertEquals("Pdf page url should match.", PDF_LINK, pdfPage.getUrl());
    }
}
