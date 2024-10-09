// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the Account Selection Passive Mode component ensure that model changes are
 * reflected in the sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionWidgetModeViewTest extends AccountSelectionJUnitTestBase {
    private final RpContextEntry[] mRpContexts =
            new RpContextEntry[] {
                new RpContextEntry(
                        RpContext.SIGN_IN, R.string.account_selection_sheet_title_explicit_signin),
                new RpContextEntry(
                        RpContext.SIGN_UP, R.string.account_selection_sheet_title_explicit_signup),
                new RpContextEntry(
                        RpContext.USE, R.string.account_selection_sheet_title_explicit_use),
                new RpContextEntry(
                        RpContext.CONTINUE,
                        R.string.account_selection_sheet_title_explicit_continue),
                // Test an invalid value.
                new RpContextEntry(0xCAFE, R.string.account_selection_sheet_title_explicit_signin)
            };

    @Before
    @Override
    public void setUp() {
        mRpMode = RpMode.PASSIVE;
        super.setUp();
    }

    @Test
    public void testRpContextTitleDisplayed() {
        for (RpContextEntry rpContext : mRpContexts) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, rpContext.mValue)
                            .with(HeaderProperties.RP_MODE, RpMode.PASSIVE)
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            TextView title = mContentView.findViewById(R.id.header_title);

            assertEquals(
                    "Incorrect title",
                    mResources.getString(rpContext.mTitleId, "example.org", "idp.org"),
                    title.getText().toString());
        }
    }

    @Test
    public void testVerifyingTitleDisplayedExplicitSignin() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.VERIFY)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.PASSIVE)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        assertEquals(
                "Incorrect title",
                mResources.getString(R.string.verify_sheet_title),
                title.getText().toString());
        assertEquals("Incorrect subtitle", "", subtitle.getText());
    }

    @Test
    public void testVerifyingTitleDisplayedAutoReauthn() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.VERIFY_AUTO_REAUTHN)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.PASSIVE)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

        assertEquals(
                "Incorrect title",
                mResources.getString(R.string.verify_sheet_title_auto_reauthn),
                title.getText().toString());
        assertEquals("Incorrect subtitle", "", subtitle.getText());
    }

    @Test
    public void testScrimHidden() {
        // Default scrim is NOT displayed if this returns true.
        assertTrue(mBottomSheetContent.hasCustomScrimLifecycle());
    }
}
