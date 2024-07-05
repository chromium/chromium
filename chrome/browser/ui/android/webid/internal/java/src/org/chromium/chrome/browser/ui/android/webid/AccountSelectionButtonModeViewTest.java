// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.res.Resources;
import android.view.View;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View tests for the Account Selection Button Mode component ensure that model changes are
 * reflected in the sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionButtonModeViewTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private class RpContextEntry {
        public int mValue;
        public int mTitleId;

        RpContextEntry(@RpContext.EnumType int value, int titleId) {
            mValue = value;
            mTitleId = titleId;
        }
    }

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

    private Resources mResources;
    private PropertyModel mModel;
    private ModelList mSheetAccountItems;
    private View mContentView;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

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
}
