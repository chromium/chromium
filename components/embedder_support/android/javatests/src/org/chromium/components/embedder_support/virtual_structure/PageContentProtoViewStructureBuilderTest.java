// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.virtual_structure;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.autofill.TestViewStructure;

/** Tests for the {@link PageContentProtoViewStructureBuilder} class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
@EnableFeatures({"AnnotatedPageContentsVirtualStructure"})
public class PageContentProtoViewStructureBuilderTest {
    private static final String TEST_PATH =
            "/chrome/test/data/android/annotated_page_content/index.html";
    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Test
    @SmallTest
    public void builderIncludesProto() {
        var testUrl = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        var webPage = mActivityTestRule.startOnWebPage(testUrl);
        var viewStructure = getViewStructureForPage(webPage);

        assertEquals(1, viewStructure.getChildCount());
        var rootNode = viewStructure.getChild(0);
        assertTrue(rootNode.hasExtras());
        assertTrue(
                rootNode.getExtras()
                        .containsKey(PageContentProtoViewStructureBuilder.APC_PROTO_EXTRA_KEY));
    }

    @Test
    @SmallTest
    public void builderDoesNothingOnIncognitoTabs() {
        var testUrl = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        var incognitoPage =
                mActivityTestRule
                        .startOnBlankPage()
                        .openNewIncognitoTabOrWindowFast()
                        .loadWebPageProgrammatically(testUrl);
        var viewStructure = getViewStructureForPage(incognitoPage);

        assertEquals(0, viewStructure.getChildCount());
        if (viewStructure.hasExtras()) {
            assertFalse(
                    viewStructure
                            .getExtras()
                            .containsKey(PageContentProtoViewStructureBuilder.APC_PROTO_EXTRA_KEY));
        }

        if (incognitoPage.getActivity().isIncognitoWindow()) {
            ApplicationTestUtils.finishActivity(incognitoPage.getActivity());
        }
    }

    @Test
    @SmallTest
    public void builderPopulatesText() {
        var testUrl = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        var webPage = mActivityTestRule.startOnWebPage(testUrl);

        var viewStructure = getViewStructureForPage(webPage);

        assertEquals(1, viewStructure.getChildCount());
        var rootNode = viewStructure.getChild(0);
        assertEquals("HTML", rootNode.getHtmlInfo().getTag());

        assertStructureContainsViewWithSubstring("Annotated page contents", rootNode);
        assertStructureContainsViewWithSubstring("Testing H6 text.", rootNode);
        assertStructureContainsViewWithSubstring("Hello, World!", rootNode);
        assertStructureContainsViewWithSubstring("Testing image descriptions", rootNode);
        assertStructureContainsViewWithSubstring("Test link", rootNode);
        assertStructureContainsViewWithSubstring("Hello, Frame 1!", rootNode);
        assertStructureContainsViewWithSubstring("Hello, Frame 2!", rootNode);
        assertStructureContainsViewWithSubstring("Test link", rootNode);
        assertStructureContainsViewWithSubstring("List item 1", rootNode);
        assertStructureContainsViewWithSubstring("List item 2", rootNode);
        assertStructureContainsViewWithSubstring("List item 3", rootNode);
    }

    private TestViewStructure getViewStructureForPage(WebPageStation webPage) {
        var outViewStructure = new TestViewStructure();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webPage.getTab().getContentView().onProvideVirtualStructure(outViewStructure);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    if (outViewStructure.getChildCount() == 0
                            && outViewStructure.isChildCountSet()) {
                        return true;
                    }
                    if (outViewStructure.getChildCount() > 0
                            && outViewStructure.getChild(0).getHtmlInfo() != null) {
                        return true;
                    }
                    return false;
                },
                DEFAULT_MAX_TIME_TO_WAIT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        return outViewStructure;
    }

    @Nullable
    private TestViewStructure getViewWithSubstring(
            TestViewStructure viewStructure, String substring) {
        if (viewStructure.getText() != null
                && viewStructure.getText().toString().contains(substring)) {
            return viewStructure;
        }

        for (int i = 0; i < viewStructure.getChildCount(); i++) {
            var childWithSubstring = getViewWithSubstring(viewStructure.getChild(i), substring);
            if (childWithSubstring != null) {
                return childWithSubstring;
            }
        }

        return null;
    }

    private void assertStructureContainsViewWithSubstring(
            String expectedSubstring, TestViewStructure viewStructure) {
        assertNotNull(
                "View should contain a child with text containing: \"" + expectedSubstring + "\"",
                getViewWithSubstring(viewStructure, expectedSubstring));
    }
}
