// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AimModelsProto.ModelMode;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Map;
import java.util.Set;

/** Tests for {@link AutocompleteInput}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteInputUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mToolModeCallback;

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
    public void testReset_clearsKeyword() {
        AutocompleteInput input = new AutocompleteInput();
        input.setSiteSearchData(new SiteSearchData("history", "Search history"));
        assertEquals("history", input.getSiteSearchData().keyword);

        input.reset();
        assertEquals(null, input.getSiteSearchData());
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
        assertEquals("", mInput.getUserText());
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
    public void getPageClassification_forFuseboxRequests() {
        Map<Integer, Integer> testCases =
                Map.of(
                        // NTP
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE,
                        PageClassification.NTP_OMNIBOX_COMPOSEBOX_VALUE,
                        // SRP
                        PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE,
                        PageClassification.SRP_OMNIBOX_COMPOSEBOX_VALUE,
                        // Web
                        PageClassification.OTHER_VALUE, //
                        PageClassification.OTHER_OMNIBOX_COMPOSEBOX_VALUE);

        for (var requestType :
                List.of(
                        AutocompleteRequestType.AI_MODE,
                        AutocompleteRequestType.IMAGE_GENERATION)) {
            mInput.setRequestType(requestType);
            for (var givePageClass : PageClassification.values()) {
                Integer wantPageClass = testCases.getOrDefault(givePageClass.getNumber(), null);
                String message =
                        String.format(
                                "Unexpected results in mode %d for page class %s",
                                requestType, givePageClass.name());

                if (wantPageClass != null) {
                    // Page classes known to Fusebox.
                    mInput.setPageClassification(givePageClass.getNumber());
                    assertEquals(message, (int) wantPageClass, mInput.getPageClassification());
                } else {
                    // These page classes not recognized by Fusebox.
                    mInput.setPageClassification(givePageClass.getNumber());
                    assertThrows(message, AssertionError.class, mInput::getPageClassification);
                }
            }
        }
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
    public void testGetToolMode() {
        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED_VALUE,
                mInput.getToolModeSupplier().get().intValue());
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN_VALUE, mInput.getToolModeSupplier().get().intValue());
        mInput.setHasAttachments(true);
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE,
                mInput.getToolModeSupplier().get().intValue());
    }

    @Test
    public void testToolModeObservations() {
        mInput.getToolModeSupplier().addSyncObserverAndCallIfNonNull(mToolModeCallback);
        verify(mToolModeCallback).onResult(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);

        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
        verify(mToolModeCallback).onResult(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

        mInput.setHasAttachments(true);
        verify(mToolModeCallback).onResult(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE);

        mInput.reset();
        verify(mToolModeCallback, atLeastOnce()).onResult(ToolMode.TOOL_MODE_UNSPECIFIED_VALUE);
    }

    @Test
    public void getSetInitialUserText() {
        mInput.setInitialUserText("initial");
        assertEquals("initial", mInput.getInitialUserText());

        mInput.setInitialUserText("");
        assertEquals("", mInput.getInitialUserText());

        mInput.setInitialUserText(null);
        assertEquals(null, mInput.getInitialUserText());
    }

    @Test
    public void shouldSuppressAutomaticSuggestionsUntilUserStartsTyping_updatesOwnState() {
        mInput.setInitialUserText("initial");
        mInput.setUserText("initial");
        mInput.setSuppressAutomaticSuggestionsUntilUserStartsTyping(true);

        // Still matches initial text.
        assertTrue(mInput.shouldSuppressAutomaticSuggestionsUntilUserStartsTyping());

        // Diverges from initial text.
        mInput.setUserText("initial typing");
        assertFalse(mInput.shouldSuppressAutomaticSuggestionsUntilUserStartsTyping());

        // Reverts to initial text - should still be false.
        mInput.setUserText("initial");
        assertFalse(mInput.shouldSuppressAutomaticSuggestionsUntilUserStartsTyping());
    }

    @Test
    public void testCopyFrom() {
        long urlFocusTime = 12345L;
        GURL pageUrl = GURL.emptyGURL();
        int pageClassification = PageClassification.OTHER_VALUE;
        String pageTitle = "pageTitle";
        String userText = "initialUserText";
        String initialUserText = "initialUserText";
        boolean hasAttachments = true;
        boolean suppressAutomaticSuggestionsUntilUserStartsTyping = true;
        int selectionStart = 1;
        int selectionEnd = 2;
        int refineActionUsage = AutocompleteInput.RefineActionUsage.SEARCH_WITH_PREFIX;
        int focusReason = OmniboxFocusReason.OMNIBOX_TAP;
        int modelMode = ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE;
        int requestType = AutocompleteRequestType.IMAGE_GENERATION;
        SiteSearchData siteSearchData = new SiteSearchData("keyword", "name");

        AutocompleteInput input1 = new AutocompleteInput();
        input1.setUrlFocusTime(urlFocusTime);
        input1.setPageUrl(pageUrl);
        input1.setPageClassification(pageClassification);
        input1.setPageTitle(pageTitle);
        input1.setUserText(userText);
        input1.setInitialUserText(initialUserText);
        input1.setHasAttachments(hasAttachments);
        input1.setSuppressAutomaticSuggestionsUntilUserStartsTyping(
                suppressAutomaticSuggestionsUntilUserStartsTyping);
        input1.setSelection(selectionStart, selectionEnd);
        input1.setRefineActionUsage(refineActionUsage);
        input1.setSuggestionsListScrolled();
        input1.setFocusReason(focusReason);
        input1.setModelMode(modelMode);
        input1.setRequestType(requestType);
        input1.setSiteSearchData(siteSearchData);

        AutocompleteInput input2 = new AutocompleteInput();
        input2.copyFrom(input1);

        assertEquals(urlFocusTime, input2.getUrlFocusTime());
        assertEquals(pageUrl, input2.getPageUrl());
        assertEquals(pageClassification, input2.getRawPageClassification());
        assertEquals(pageTitle, input2.getPageTitle());
        assertEquals(userText, input2.getUserText());
        assertEquals(initialUserText, input2.getInitialUserText());
        assertEquals(input1.allowExactKeywordMatch(), input2.allowExactKeywordMatch());
        assertEquals(
                suppressAutomaticSuggestionsUntilUserStartsTyping,
                input2.shouldSuppressAutomaticSuggestionsUntilUserStartsTyping());
        assertEquals(selectionStart, (int) input2.getSelection().getLower());
        assertEquals(selectionEnd, (int) input2.getSelection().getUpper());
        assertEquals(refineActionUsage, input2.getRefineActionUsage());
        assertTrue(input2.isSuggestionsListScrolled());
        assertEquals(focusReason, input2.getFocusReason());
        assertEquals(modelMode, input2.getModelMode());
        assertEquals(requestType, input2.getRequestType());
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE,
                input2.getToolModeSupplier().get().intValue());
        assertEquals(siteSearchData, input2.getSiteSearchData());
    }

    @Test
    public void getTextForAutocomplete() {
        mInput.setUserText("user query");

        // Without Site Search data, should return the exact user text.
        assertEquals("user query", mInput.getTextForAutocomplete());

        // With Site Search data, should prepend the keyword and a space.
        mInput.setSiteSearchData(new AutocompleteInput.SiteSearchData("example.com", "Example"));
        assertEquals("example.com user query", mInput.getTextForAutocomplete());
    }

    @Test
    public void getCursorPositionForAutocomplete() {
        mInput.setUserText("user query");

        // Without Site Search data, should return the given cursor position unmodified.
        assertEquals(0, mInput.getCursorPositionForAutocomplete(0));
        assertEquals(5, mInput.getCursorPositionForAutocomplete(5));
        assertEquals(10, mInput.getCursorPositionForAutocomplete(10));
        assertEquals(15, mInput.getCursorPositionForAutocomplete(15));
        assertEquals(-1, mInput.getCursorPositionForAutocomplete(-1));

        // With Site Search data, should offset by keyword length + 1 (for space).
        // Keyword "example.com" length is 11. Offset is 12.
        mInput.setSiteSearchData(new AutocompleteInput.SiteSearchData("example.com", "Example"));

        assertEquals(12, mInput.getCursorPositionForAutocomplete(0)); // 0 + 12
        assertEquals(17, mInput.getCursorPositionForAutocomplete(5)); // 5 + 12

        // Should cap cursor position to user text length (10) + offset (12) = 22.
        assertEquals(22, mInput.getCursorPositionForAutocomplete(10));
        assertEquals(22, mInput.getCursorPositionForAutocomplete(15));

        // Should return original value if cursor position < 0.
        assertEquals(-1, mInput.getCursorPositionForAutocomplete(-1));
    }
}
