// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static java.util.Arrays.asList;

import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerItemDecoration;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the Account Selection Button Mode component ensure that model changes are
 * reflected in the sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionButtonModeViewTest extends AccountSelectionViewTestBase {
    private final RpContextEntry[] mRpContexts =
            new RpContextEntry[] {
                new RpContextEntry(
                        RpContext.SIGN_IN,
                        R.string.account_selection_button_mode_sheet_title_explicit_signin),
                new RpContextEntry(
                        RpContext.SIGN_UP,
                        R.string.account_selection_button_mode_sheet_title_explicit_signup),
                new RpContextEntry(
                        RpContext.USE,
                        R.string.account_selection_button_mode_sheet_title_explicit_use),
                new RpContextEntry(
                        RpContext.CONTINUE,
                        R.string.account_selection_button_mode_sheet_title_explicit_continue),
                // Test a random invalid value.
                new RpContextEntry(
                        0xCAFE, R.string.account_selection_button_mode_sheet_title_explicit_signin)
            };

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mModel =
                                    new PropertyModel.Builder(
                                                    AccountSelectionProperties.ItemProperties
                                                            .ALL_KEYS)
                                            .build();
                            mSheetAccountItems = new ModelList();
                            mContentView =
                                    AccountSelectionCoordinator.setupContentView(
                                            activity,
                                            mModel,
                                            mSheetAccountItems,
                                            /* rpMode= */ RpMode.BUTTON);
                            activity.setContentView(mContentView);
                            mResources = activity.getResources();
                        });
    }

    @Test
    public void testDragHandlebarShown() {
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        View handlebar = mContentView.findViewById(R.id.drag_handlebar);
        assertTrue(handlebar.isShown());
    }

    @Test
    public void testRpContextTitleDisplayed() {
        for (RpContextEntry rpContext : mRpContexts) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                            .with(HeaderProperties.TOP_FRAME_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IFRAME_FOR_DISPLAY, "iframe-example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, rpContext.mValue)
                            .with(HeaderProperties.RP_MODE, RpMode.BUTTON)
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            TextView title = mContentView.findViewById(R.id.header_title);
            TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

            assertEquals(
                    "Incorrect title",
                    mResources.getString(rpContext.mTitleId, "idp.org"),
                    title.getText().toString());
            assertEquals("Incorrect subtitle", "example.org", subtitle.getText());
        }
    }

    @Test
    public void testAccountsListHasAccountPickerItemDecoration() {
        mSheetAccountItems.addAll(
                asList(
                        buildAccountItem(mAnaAccount),
                        buildAccountItem(mNoOneAccount),
                        buildAccountItem(mBobAccount)));
        ShadowLooper.shadowMainLooper().idle();

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        RecyclerView accountsList = mContentView.findViewById(R.id.sheet_item_list);
        assertTrue(accountsList.isShown());

        assertEquals(1, accountsList.getItemDecorationCount());
        assertEquals(
                accountsList.getItemDecorationAt(0).getClass(), AccountPickerItemDecoration.class);
    }
}
