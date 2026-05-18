// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.ActionPresentationMode;

/** Tests for {@link OmniboxActionFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxActionFactoryUnitTest {

    @Test
    public void omniboxPedalsDowncasting() {
        // The underlying code will throw if instance is not valid.
        // Checking for null in case that changes.
        assertNotNull(
                OmniboxPedal.from(
                        OmniboxActionFactory.buildOmniboxPedal(0, "hint", "accessibility", 1)));
    }

    @Test
    public void omniboxActionInSuggestDowncasting() {
        // The underlying code will throw if instance is not valid.
        // Checking for null in case that changes.
        assertNotNull(
                OmniboxActionInSuggest.from(
                        OmniboxActionFactory.buildActionInSuggest(
                                0,
                                "hint",
                                "accessibility",
                                1,
                                "url",
                                /* tabId= */ 0,
                                ActionPresentationMode.CHIP)));
    }

    @Test
    public void actionInSuggest_callActionNotCreatedWhenDialerUnavailable() {
        OmniboxActionFactory.setDialerAvailable(false);
        assertNull(
                OmniboxActionFactory.buildActionInSuggest(
                        0,
                        "hint",
                        "accessibility",
                        SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE,
                        "url",
                        /* tabId= */ 0,
                        ActionPresentationMode.CHIP));
    }

    @Test
    public void actionInSuggest_callActionCreatedWhenDialerAvailable() {
        OmniboxActionFactory.setDialerAvailable(true);
        assertNotNull(
                OmniboxActionFactory.buildActionInSuggest(
                        0,
                        "hint",
                        "accessibility",
                        SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE,
                        "url",
                        /* tabId= */ 0,
                        ActionPresentationMode.CHIP));
    }

    @Test
    public void siteSearchActionDowncasting() {
        // The underlying code will throw if instance is not valid.
        // Checking for null in case that changes.
        assertNotNull(
                OmniboxActionFactory.buildSiteSearchAction(0, "hint", "accessibility", "keyword"));
    }
}
