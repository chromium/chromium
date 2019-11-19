// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.ChromeMenu;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.controllers.urlpage.UrlPage;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;

/**
 * Tests for the NewTabPageController.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class NewTabPageControllerTest {
    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();

    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    private NewTabPageController mController;

    @Before
    public void setUp() {
        mController = mChromeUiRule.launchIntoNewTabPageOnFirstRun();
    }

    @Test
    public void testIsCurrentPageThis() {
        Assert.assertTrue(mController.isCurrentPageThis());
    }

    @Test
    public void testHideArticles() {
        boolean isHidden = mController.areArticlesHidden();
        mController.toggleHideArticles();
        assertNotEquals(isHidden, mController.areArticlesHidden());
    }

    @LargeTest
    @Test
    public void testScrollPage() {
        mController.scrollToTop();
        assertTrue(mController.hasScrolledToTop());
        mController.scrollToBottom();
        assertTrue(mController.hasScrolledToBottom());
    }

    @Test
    public void testOpenChromeMenu() {
        ChromeMenu menu = mController.openChromeMenu();
        Assert.assertTrue(menu.isCurrentPageThis());
    }

    @Test
    public void testOmniboxSearch() {
        UrlPage urlPage = mController.omniboxSearch("www.google.com");
        Assert.assertTrue(urlPage.isCurrentPageThis());
    }
}
