// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.res.Resources;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.RpContext;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Common test fixtures for AccountSelectionView JUnit tests. */
public class AccountSelectionViewTestBase {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    class RpContextEntry {
        public int mValue;
        public int mTitleId;

        RpContextEntry(@RpContext.EnumType int value, int titleId) {
            mValue = value;
            mTitleId = titleId;
        }
    }

    @Mock Callback<Account> mAccountCallback;

    // Constants but this test base is used by parameterized tests. These can only be initialized
    // after parameterized test runner setup.
    GURL mTestProfilePicUrl;
    Account mAnaAccount;
    Account mNoOneAccount;
    Account mBobAccount;

    Resources mResources;
    PropertyModel mModel;
    ModelList mSheetAccountItems;
    View mContentView;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mTestProfilePicUrl = new GURL("https://profile-picture.com");

        mAnaAccount =
                new Account(
                        "Ana",
                        "ana@email.example",
                        "Ana Doe",
                        "Ana",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true);
        mNoOneAccount =
                new Account(
                        "",
                        "",
                        "No Subject",
                        "",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true);
        mBobAccount =
                new Account(
                        "Bob",
                        "",
                        "Bob",
                        "",
                        mTestProfilePicUrl,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true);
    }

    MVCListAdapter.ListItem buildAccountItem(Account account) {
        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, account)
                        .with(AccountProperties.ON_CLICK_LISTENER, mAccountCallback)
                        .build());
    }
}
