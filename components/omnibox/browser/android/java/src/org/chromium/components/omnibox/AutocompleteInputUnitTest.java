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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AimToolsAndModelsProto.ChromeAimToolsAndModels;

import java.util.Set;

/** Tests for {@link AutocompleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteInputUnitTest {

    private final AutocompleteInput mInput = new AutocompleteInput();

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
    @DisableFeatures(OmniboxFeatureList.JUMP_START_OMNIBOX)
    public void isInCacheableContext_defaultContexts() {
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE));
    }

    @Test
    @EnableFeatures(
            OmniboxFeatureList.JUMP_START_OMNIBOX + ":jump_start_cover_recently_visited_page/false")
    public void isInCacheableContext_jumpStartDisabled() {
        OmniboxFeatures.setJumpStartOmniboxEnabled(false);
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE));
    }

    @Test
    @EnableFeatures(
            OmniboxFeatureList.JUMP_START_OMNIBOX + ":jump_start_cover_recently_visited_page/false")
    public void isInCacheableContext_jumpStartDefaultContext() {
        OmniboxFeatures.setJumpStartOmniboxEnabled(true);
        verifyCacheablePageClasses(
                Set.of(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE));
    }

    @Test
    @EnableFeatures(
            OmniboxFeatureList.JUMP_START_OMNIBOX + ":jump_start_cover_recently_visited_page/true")
    public void isInCacheableContext_jumpStartAdditionalContext() {
        OmniboxFeatures.setJumpStartOmniboxEnabled(true);
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
        mInput.reset();
        mInput.setUserText(null);
    }

    @Test
    public void allowExactKeywordMatch_initialState() {
        assertFalse(mInput.allowExactKeywordMatch());
    }

    @Test
    public void allowExactKeywordMatch_addingFirstSpace() {
        // Initially false
        assertFalse(mInput.allowExactKeywordMatch());

        // Adding text without space should keep it false
        mInput.setUserText("keyword");
        assertFalse(mInput.allowExactKeywordMatch());

        // Adding first space should enable keyword matching
        mInput.setUserText("keyword test");
        assertTrue(mInput.allowExactKeywordMatch());
    }

    @Test
    public void allowExactKeywordMatch_removingSpace() {
        // Set text with space to enable keyword matching
        mInput.setUserText("keyword test");
        assertTrue(mInput.allowExactKeywordMatch());

        // Removing space should disable keyword matching
        mInput.setUserText("keyword");
        assertFalse(mInput.allowExactKeywordMatch());
    }

    @Test
    public void allowExactKeywordMatch_complexScenarios() {
        // Start with no space
        mInput.setUserText("search");
        assertFalse(mInput.allowExactKeywordMatch());

        // Add space - should enable
        mInput.setUserText("search term");
        assertTrue(mInput.allowExactKeywordMatch());

        // Modify text but keep space - should remain enabled
        mInput.setUserText("search different");
        assertTrue(mInput.allowExactKeywordMatch());

        // Remove space - should disable
        mInput.setUserText("searchterm");
        assertFalse(mInput.allowExactKeywordMatch());

        // Add space again - should re-enable
        mInput.setUserText("search again");
        assertTrue(mInput.allowExactKeywordMatch());
    }

    @Test
    public void allowExactKeywordMatch_nullAndEmptyHandling() {
        // Test with null input
        mInput.setUserText(null);
        assertFalse(mInput.allowExactKeywordMatch());

        // Test transition from null to text with space
        mInput.setUserText("test space");
        assertTrue(mInput.allowExactKeywordMatch());

        // Test transition to empty string
        mInput.setUserText("");
        assertFalse(mInput.allowExactKeywordMatch());
    }

    @Test
    public void isInZeroPrefixContext() {
        // Empty string should be zero-prefix
        mInput.setUserText("");
        assertTrue(mInput.isInZeroPrefixContext());

        // Null should be zero-prefix
        mInput.setUserText(null);
        assertTrue(mInput.isInZeroPrefixContext());

        // Non-empty string should not be zero-prefix
        mInput.setUserText("test");
        assertFalse(mInput.isInZeroPrefixContext());

        // String with just spaces should not be zero-prefix
        mInput.setUserText(" ");
        assertFalse(mInput.isInZeroPrefixContext());
    }

    @Test
    public void getPageClassification() {
        // Test initial value
        assertEquals(PageClassification.BLANK_VALUE, mInput.getPageClassification());

        // Test setting and getting different values
        mInput.setPageClassification(PageClassification.ANDROID_SEARCH_WIDGET_VALUE);
        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE, mInput.getPageClassification());

        mInput.setPageClassification(PageClassification.OTHER_VALUE);
        assertEquals(PageClassification.OTHER_VALUE, mInput.getPageClassification());
    }

    @Test
    public void setUserText_edgeCases() {
        // Test setting text to exactly one space
        mInput.setUserText(" ");
        assertEquals(" ", mInput.getUserText());
        assertFalse(mInput.allowExactKeywordMatch());

        // Test setting text with leading space
        mInput.setUserText(" test");
        assertEquals(" test", mInput.getUserText());
        assertFalse(mInput.allowExactKeywordMatch());

        // Test setting text with trailing space
        mInput.setUserText("test ");
        assertEquals("test ", mInput.getUserText());
        assertTrue(mInput.allowExactKeywordMatch());

        // Test setting text with multiple spaces
        mInput.setUserText("test multiple spaces");
        assertEquals("test multiple spaces", mInput.getUserText());
        assertTrue(mInput.allowExactKeywordMatch());
    }

    @Test
    public void integrationTest_resetClearsAllState() {
        // Set up some state
        mInput.setPageClassification(PageClassification.OTHER_VALUE);
        mInput.setUserText("test with space");

        // Verify state is set
        assertEquals(PageClassification.OTHER_VALUE, mInput.getPageClassification());
        assertEquals("test with space", mInput.getUserText());
        assertTrue(mInput.allowExactKeywordMatch());
        assertFalse(mInput.isInZeroPrefixContext());

        // Reset should clear text and keyword match but not page classification
        mInput.reset();

        assertEquals(PageClassification.BLANK_VALUE, mInput.getPageClassification());
        assertEquals("", mInput.getUserText());
        assertFalse(mInput.allowExactKeywordMatch());
        assertTrue(mInput.isInZeroPrefixContext());
    }

    @Test
    public void integrationTest_cacheableContextAndKeywordMatch() {
        // Set up cacheable context
        mInput.setPageClassification(PageClassification.ANDROID_SEARCH_WIDGET_VALUE);
        mInput.setUserText("");

        // Should be cacheable and zero-prefix, but not allow keyword match
        assertTrue(mInput.isInCacheableContext());
        assertTrue(mInput.isInZeroPrefixContext());
        assertFalse(mInput.allowExactKeywordMatch());

        // Add text with space - should disable caching but enable keyword match
        mInput.setUserText("search term");
        assertFalse(mInput.isInCacheableContext());
        assertFalse(mInput.isInZeroPrefixContext());
        assertTrue(mInput.allowExactKeywordMatch());

        // Back to empty - should be cacheable again, keyword match disabled
        mInput.setUserText("");
        assertTrue(mInput.isInCacheableContext());
        assertTrue(mInput.isInZeroPrefixContext());
        assertFalse(mInput.allowExactKeywordMatch());
    }

    @Test
    public void toolMode() {
        assertEquals(ChromeAimToolsAndModels.TOOL_MODE_UNSPECIFIED_VALUE, mInput.getToolMode());
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        assertEquals(ChromeAimToolsAndModels.TOOL_MODE_IMAGE_GEN_VALUE, mInput.getToolMode());
        mInput.setHasAttachments(true);
        assertEquals(
                ChromeAimToolsAndModels.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE, mInput.getToolMode());
    }
}
