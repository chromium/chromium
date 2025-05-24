// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.webkit.URLUtil;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.url.GURL;

/** Unit tests for {@link ContextMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuUtilsUnitTest {
    Activity mActivity;
    private static final String sTitleText = "titleText";
    private static final String sLinkText = "linkText";
    private static final String sSrcUrl = "https://www.google.com/";

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    @SmallTest
    public void getTitle_hasTitleText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals(sTitleText, ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noTitleTextHasLinkText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals(sLinkText, ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noTitleTextOrLinkText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals(URLUtil.guessFileName(sSrcUrl, null, null), ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noShareParams() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestTarget= */ false,
                        /* interestTargetNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals("", ContextMenuUtils.getTitle(params));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void usePopupAllScreen_Small() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void usePopupAllScreen_Large() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void doNotUsePopupForSmallScreen() {
        assertFalse(
                "Popup should not be used for small screen.",
                ContextMenuUtils.isPopupSupported(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void usePopupForLargeScreen() {
        assertTrue(
                "Popup should be used for large screen.",
                ContextMenuUtils.isPopupSupported(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void nullInputs() {
        assertFalse("Always return false for null input.", ContextMenuUtils.isPopupSupported(null));
    }

    private void doTestUsePopupWhenEnabledByFlag() {
        assertTrue(
                "Popup should be used when switch FORCE_CONTEXT_MENU_POPUP is enabled.",
                ContextMenuUtils.isPopupSupported(mActivity));
    }
}
