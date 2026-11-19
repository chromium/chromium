// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link OmniboxContextualSearchFulfillmentAction}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxContextualSearchFulfillmentActionUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private OmniboxActionDelegate mDelegate;

    private final GURL mUrl = JUnitTestGURLs.SEARCH_URL;
    private OmniboxContextualSearchFulfillmentAction mAction;

    @Before
    public void setUp() {
        mAction = new OmniboxContextualSearchFulfillmentAction(12345L, "hint", "a11y", mUrl);
    }

    @Test
    public void testConstructor() {
        assertEquals(OmniboxActionId.CONTEXTUAL_SEARCH_FULFILLMENT, mAction.actionId);
        assertEquals(ActionPresentationMode.CHIP, mAction.presentationMode);
        assertEquals("hint", mAction.hint);
        assertEquals("a11y", mAction.accessibilityHint);
        assertEquals(mUrl, mAction.fulfillmentUrl);
        assertEquals(Resources.ID_NULL, mAction.icon.chipIconRes);
        assertFalse(mAction.icon.tintWithTextColor);
    }

    @Test
    public void testExecute_issuesContextualSearchQuery() {
        assertTrue(mAction.execute(mDelegate));
        verify(mDelegate).issueContextualSearchQuery(mUrl);
    }
}
