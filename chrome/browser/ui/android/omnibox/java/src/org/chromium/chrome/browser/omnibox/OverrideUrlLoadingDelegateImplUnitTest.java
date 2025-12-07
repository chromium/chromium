// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Unit tests for the URL bar UI component. */
@RunWith(BaseRobolectricTestRunner.class)
public class OverrideUrlLoadingDelegateImplUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock Runnable mOpenGridTabSwitcher;
    private OverrideUrlLoadingDelegateImpl mDelegate;

    @Before
    public void setUp() {
        mDelegate = new OverrideUrlLoadingDelegateImpl();
    }

    private boolean willHandleLoadUrlWithPostData(String url, boolean isIncognito) {
        return mDelegate.willHandleLoadUrlWithPostData(
                new OmniboxLoadUrlParams.Builder(url, 0).build(), isIncognito);
    }

    @Test
    public void handleLoadUrl_unsupportedUrl() {
        assertFalse(willHandleLoadUrlWithPostData(UrlConstants.NTP_URL, false));
        assertFalse(willHandleLoadUrlWithPostData(UrlConstants.NTP_URL, true));
    }

    @Test
    public void handleLoadUrl_gridTabSwitcher_missingCallback() {
        assertFalse(willHandleLoadUrlWithPostData(UrlConstants.GRID_TAB_SWITCHER_URL, false));
        assertFalse(willHandleLoadUrlWithPostData(UrlConstants.GRID_TAB_SWITCHER_URL, true));
    }

    @Test
    public void handleLoadUrl_gridTabSwitcher_withCallback() {
        mDelegate.setOpenGridTabSwitcherCallback(mOpenGridTabSwitcher);
        assertTrue(willHandleLoadUrlWithPostData(UrlConstants.GRID_TAB_SWITCHER_URL, false));
        verify(mOpenGridTabSwitcher).run();
        clearInvocations(mOpenGridTabSwitcher);

        assertTrue(willHandleLoadUrlWithPostData(UrlConstants.GRID_TAB_SWITCHER_URL, true));
        verify(mOpenGridTabSwitcher).run();
    }

    @Test
    public void handleLoadUrl_gridTabSwitcher_resetCallback() {
        mDelegate.setOpenGridTabSwitcherCallback(mOpenGridTabSwitcher);
        mDelegate.setOpenGridTabSwitcherCallback(null);

        assertFalse(willHandleLoadUrlWithPostData(UrlConstants.GRID_TAB_SWITCHER_URL, false));
        assertFalse(willHandleLoadUrlWithPostData(UrlConstants.GRID_TAB_SWITCHER_URL, true));
        verifyNoMoreInteractions(mOpenGridTabSwitcher);
    }
}
