// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.app.Activity;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Collections;

/** Unit tests that rely on UI rendering for UrlBar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class UrlBarUiUnitTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static FrameLayout sContentView;

    private UrlBar mUrlBar;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                    sContentView = new FrameLayout(sActivity);
                    sContentView.setLayoutParams(
                            new ViewGroup.MarginLayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    sActivity
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.control_container_height)));
                    sActivity.setContentView(sContentView);
                });
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                    sActivity.getLayoutInflater().inflate(R.layout.url_bar, sContentView);
                    mUrlBar = (UrlBar) sContentView.getChildAt(0);
                });
    }

    private static void assertTextEquals(CharSequence a, CharSequence b) {
        Assert.assertTrue(a + " should match: " + b, TextUtils.equals(a, b));
    }

    private void waitForUrlBarLayout() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mUrlBar.isLayoutRequested(), Matchers.is(false));
                    Criteria.checkThat(mUrlBar.isInLayout(), Matchers.is(false));
                });
    }

    private void updateUrlBarText(
            CharSequence text, @UrlBar.ScrollType int scrollType, int scrollIndex) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setText(text);
                    mUrlBar.setScrollState(scrollType, scrollIndex);
                });
        waitForUrlBarLayout();
    }

    private CharSequence getUrlText() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getText());
    }

    private CharSequence getVisibleTextPrefixHint() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_ShortUrl() throws Exception {
        String url = "www.test.com";
        updateUrlBarText(url, UrlBar.ScrollType.SCROLL_TO_TLD, url.length());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    float scrollXPosForEndOfUrlText =
                            mUrlBar.getLayout().getPrimaryHorizontal(mUrlBar.getText().length());
                    assertThat(
                            scrollXPosForEndOfUrlText,
                            Matchers.lessThan((float) mUrlBar.getMeasuredWidth()));
                });

        Assert.assertNull(getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_ShortTld_LongPath() throws Exception {
        final String domain = "www.test.com";
        final String path = "/" + TextUtils.join("", Collections.nCopies(500, "a"));
        updateUrlBarText(domain + path, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    float scrollXPosForEndOfUrlText =
                            mUrlBar.getLayout().getPrimaryHorizontal(mUrlBar.getText().length());
                    assertThat(
                            scrollXPosForEndOfUrlText,
                            Matchers.greaterThan((float) mUrlBar.getMeasuredWidth()));
                });

        CharSequence urlText = getUrlText();
        Assert.assertNull(getVisibleTextPrefixHint());

        // Append a string to the already long initial text and validate the prefix doesn't change.
        updateUrlBarText(
                getUrlText() + "bbbbbbbbbbbbbbbbbbbbbbb",
                UrlBar.ScrollType.SCROLL_TO_TLD,
                domain.length());
        final CharSequence prefixHint = getVisibleTextPrefixHint();
        Assert.assertNotNull(prefixHint);
        Assert.assertTrue(
                "Expected url text: '" + urlText + "' starts with " + prefixHint,
                TextUtils.indexOf(urlText, prefixHint) == 0);
        assertThat(prefixHint.length(), Matchers.lessThan(urlText.length()));

        // Append a character to just the hint prefix text and validate the prefix doesn't change.
        updateUrlBarText(prefixHint + "a", UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());
        assertTextEquals(prefixHint, getVisibleTextPrefixHint());

        // Set the text to just the prefix text and ensure the hint remains unchanged.
        updateUrlBarText(prefixHint, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());
        assertTextEquals(prefixHint, getVisibleTextPrefixHint());

        // Set the text to be slightly shorter than the prefix, which will result in the text
        // being shorter than the visual viewport, and thus generate a null hint text.
        //
        // We subtract by 2 because an additional trailing char is added to the visible text to
        // account for rounding issues with text positioning.
        updateUrlBarText(
                TextUtils.substring(prefixHint, 0, prefixHint.length() - 2),
                UrlBar.ScrollType.SCROLL_TO_TLD,
                domain.length());
        Assert.assertNull(getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_ShortTld_LongPath_WithRtl() throws Exception {
        final String domain = "www.test.com";
        // Add a RTL character shortly after the TLD, so that it is visible.
        final String path = "/aت" + TextUtils.join("", Collections.nCopies(500, "a"));
        updateUrlBarText(domain + path, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    float scrollXPosForEndOfUrlText =
                            mUrlBar.getLayout().getPrimaryHorizontal(mUrlBar.getText().length());
                    assertThat(
                            scrollXPosForEndOfUrlText,
                            Matchers.greaterThan((float) mUrlBar.getMeasuredWidth()));
                });

        // Assert null visible hint when there is RTl text anywhere in the visible url
        final CharSequence prefixHint = getVisibleTextPrefixHint();
        Assert.assertNull(prefixHint);

        // Append a string to the already long initial text and validate the prefix doesn't change.
        updateUrlBarText(
                getUrlText() + "bbbbbbbbbbbbbbbbbbbbbbb",
                UrlBar.ScrollType.SCROLL_TO_TLD,
                domain.length());
        Assert.assertNull(prefixHint);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_LongTld() throws Exception {
        final String domain = "www." + TextUtils.join("", Collections.nCopies(500, "a")) + ".com";
        updateUrlBarText(domain, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());

        final CharSequence urlText = getUrlText();
        CharSequence prefixHint = getVisibleTextPrefixHint();
        Assert.assertNotNull(prefixHint);
        assertTextEquals(urlText, prefixHint);

        updateUrlBarText(
                getUrlText() + "/foooooo", UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());
        assertTextEquals(urlText, getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_NonUrlText() {
        updateUrlBarText("a", UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        Assert.assertNull(getVisibleTextPrefixHint());

        updateUrlBarText(
                TextUtils.join("", Collections.nCopies(500, "a")),
                UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                0);
        Assert.assertNull(getVisibleTextPrefixHint());
    }
}
