// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;

/** Tests for {@link CrossDeviceTabAction}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrossDeviceTabActionUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OmniboxActionDelegate mGenericDelegate;

    private CrossDeviceTabAction mAction;

    @Before
    public void setUp() {
        mAction = new CrossDeviceTabAction(12345L, "Open tab from device", "Open tab from device");
    }

    @Test
    public void testConstructor() {
        assertEquals(OmniboxActionId.CROSS_DEVICE_TAB, mAction.actionId);
        assertEquals(ActionPresentationMode.CHIP, mAction.presentationMode);
        assertEquals("Open tab from device", mAction.hint);
        assertEquals("Open tab from device", mAction.accessibilityHint);
    }

    @Test
    public void testExecute_success() {
        assertTrue(mAction.execute(mGenericDelegate));
        verify(mGenericDelegate).loadPageInCurrentTab(UrlConstants.RECENT_TABS_URL);
    }
}
