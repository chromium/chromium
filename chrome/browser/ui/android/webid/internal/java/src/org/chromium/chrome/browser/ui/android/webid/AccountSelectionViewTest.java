// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.SINGLE_ACCOUNT;

import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * View tests for the Account Selection component ensure that model changes are reflected in the
 * sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccountSelectionViewTest {
    private DummyUiActivity mActivity;
    private ModelList mSheetItems;
    private View mContentView;
    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems = new ModelList();
            mContentView = AccountSelectionCoordinator.setupContentView(mActivity, mSheetItems);
            mActivity.setContentView(mContentView);
        });
    }

    @Test
    @MediumTest
    public void testSingleAccountTitleDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.add(new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(SINGLE_ACCOUNT, true)
                            .with(FORMATTED_URL, "www.example.org")
                            .build()));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.account_selection_sheet_title);

        assertThat("Incorrect title", title.getText(),
                is(mActivity.getString(
                        R.string.account_selection_sheet_title_single, "www.example.org")));
    }

    @Test
    @MediumTest
    public void testMultiAccountTitleDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.add(new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(SINGLE_ACCOUNT, false)
                            .with(FORMATTED_URL, "www.example.org")
                            .build()));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.account_selection_sheet_title);

        assertThat("Incorrect title", title.getText(),
                is(mActivity.getString(R.string.account_selection_sheet_title, "www.example.org")));
    }
}
