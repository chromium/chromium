// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

@RunWith(BaseRobolectricTestRunner.class)
public class PdfTabHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    private UserDataHost mUserDataHost;
    private static final int TAB_ID = 123;
    private static final String FILE_URL = "file:///sdcard/Download/document.pdf";
    private static final String FILE_URL_PREFIX_MATCH =
            "file:///sdcard/Download/document.pdf-other.pdf";
    private static final String ENCODED_FILE_URL =
            "chrome-native://pdf/link?url=file%3A%2F%2F%2Fsdcard%2FDownload%2Fdocument.pdf";
    private static final String ENCODED_FILE_URL_PREFIX_MATCH =
            "chrome-native://pdf/link?url=file%3A%2F%2F%2Fsdcard%2FDownload%2Fdocument.pdf-other.pdf";
    private static final String DIFFERENT_URL = "https://www.example.com/";

    @Before
    public void setUp() {
        mUserDataHost = new UserDataHost();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(TAB_ID).when(mTab).getId();
    }

    @Test
    public void testIsSamePdf_ExactMatch() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(FILE_URL);

        assertTrue("Exact URL string should match", helper.isSamePdf(FILE_URL));
    }

    @Test
    public void testIsSamePdf_EncodedAndDecodedMatch() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(ENCODED_FILE_URL);

        assertTrue("Decoded URL should match encoded PDF page URL", helper.isSamePdf(FILE_URL));

        helper.setPdfUrl(FILE_URL);
        assertTrue(
                "Encoded PDF page URL should match decoded URL",
                helper.isSamePdf(ENCODED_FILE_URL));
    }

    @Test
    public void testIsSamePdf_PrefixMismatch_ReturnsFalse() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(FILE_URL);

        assertFalse(
                "URL with same prefix but different filename should not match",
                helper.isSamePdf(FILE_URL_PREFIX_MATCH));
    }

    @Test
    public void testIsSamePdf_EncodedPrefixMismatch_ReturnsFalse() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(ENCODED_FILE_URL);

        assertFalse(
                "Encoded URL with same prefix but different filename should not match",
                helper.isSamePdf(ENCODED_FILE_URL_PREFIX_MATCH));
    }

    @Test
    public void testIsSamePdf_NullPdfUrl() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        assertFalse("Should return false when mPdfUrl is null", helper.isSamePdf(FILE_URL));
    }

    @Test
    public void testOnPageLoadStarted_SamePdf_DoesNotCleanUp() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(FILE_URL);

        helper.onPageLoadStarted(mTab, new GURL(FILE_URL));

        assertNotNull(
                "UserData should not be removed on same PDF page load",
                mUserDataHost.getUserData(PdfTabHelper.class));
    }

    @Test
    public void testOnPageLoadStarted_DifferentPage_CleansUp() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(FILE_URL);

        helper.onPageLoadStarted(mTab, new GURL(DIFFERENT_URL));

        assertNull(
                "UserData should be removed on navigation to a different page",
                mUserDataHost.getUserData(PdfTabHelper.class));
    }

    @Test
    public void testOnDestroyed_CleansUp() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(FILE_URL);

        helper.onDestroyed(mTab);

        assertNull(
                "UserData should be removed when tab is destroyed",
                mUserDataHost.getUserData(PdfTabHelper.class));
    }

    @Test
    public void testOnActivityAttachmentChanged_DoesNotCleanUpOrUnregisterObserver() {
        PdfTabHelper helper = PdfTabHelper.from(mTab);
        helper.setPdfUrl(FILE_URL);

        helper.onActivityAttachmentChanged(mTab, null);

        assertNotNull(
                "UserData should not be removed on activity detachment",
                mUserDataHost.getUserData(PdfTabHelper.class));
        verify(mTab, never()).removeObserver(helper);
    }
}
