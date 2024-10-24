// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

import java.util.Set;

/** Tests for {@link AutocompleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteInputUnitTest {
    private AutocompleteInput mInput = new AutocompleteInput();

    private void verifyCacheablePageClasses(Set<Integer> allowedPageClasses) {
        for (var pageClass : PageClassification.values()) {
            mInput.setPageClassification(pageClass.getNumber());

            // Typed contexts are never cacheable.
            mInput.setUserText("text");
            assertFalse(mInput.isInCacheableContext());

            // Only ZPS contexts are cacheable.
            mInput.setUserText("");
            assertEquals(
                    mInput.isInCacheableContext(),
                    allowedPageClasses.contains(pageClass.getNumber()));
        }
    }

    @Test
    public void isInCacheableContext_defaultContexts() {
        OmniboxFeatures.sJumpStartOmnibox.setForTesting(false);
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE));
    }

    @Test
    public void isInCacheableContext_jumpStartDisabled() {
        OmniboxFeatures.sJumpStartOmnibox.setForTesting(true);
        OmniboxFeatures.setJumpStartOmniboxEnabled(false);
        OmniboxFeatures.sJumpStartOmniboxCoverRecentlyVisitedPage.setForTesting(false);
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE));
    }

    @Test
    public void isInCacheableContext_jumpStartDefaultContext() {
        OmniboxFeatures.sJumpStartOmnibox.setForTesting(true);
        OmniboxFeatures.setJumpStartOmniboxEnabled(true);
        OmniboxFeatures.sJumpStartOmniboxCoverRecentlyVisitedPage.setForTesting(false);
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE));
    }

    @Test
    public void isInCacheableContext_jumpStartAdditionalContext() {
        OmniboxFeatures.sJumpStartOmnibox.setForTesting(true);
        OmniboxFeatures.setJumpStartOmniboxEnabled(true);
        OmniboxFeatures.sJumpStartOmniboxCoverRecentlyVisitedPage.setForTesting(true);
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE,
                        PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE,
                        PageClassification.OTHER_VALUE));
    }

    @Test
    public void setUserText() {
        mInput.setUserText("test");
        assertEquals("test", mInput.getUserText());

        mInput.setUserText("");
        assertEquals("", mInput.getUserText());

        mInput.setUserText(null);
        assertEquals(null, mInput.getUserText());
    }

    @Test
    public void reset() {
        mInput.setUserText("test");
        mInput.setPageClassification(123);

        mInput.reset();

        mInput.setUserText(null);
        assertTrue(mInput.getPageClassification().isEmpty());
    }
}
