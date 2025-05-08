// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests the class {@link AccountPickerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountPickerMediatorTest {
    /* Used to simulate a name change event for TestAccounts.ACCOUNT1. */
    public static final AccountInfo ACCOUNT1_DIFFERENT_NAME =
            new AccountInfo.Builder(
                            TestAccounts.ACCOUNT1.getEmail(), TestAccounts.ACCOUNT1.getGaiaId())
                    .fullName("Different Test1 Full")
                    .givenName("Different Test1 Given")
                    .build();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private AccountPickerCoordinator.Listener mListenerMock;

    private final MVCListAdapter.ModelList mModelList = new MVCListAdapter.ModelList();

    private AccountPickerMediator mMediator;

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    @Test
    public void testModelPopulation() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mMediator =
                new AccountPickerMediator(
                        RuntimeEnvironment.getApplication(), mModelList, mListenerMock);
        // ACCOUNT1, ACCOUNT2, ADD_ACCOUNT.
        Assert.assertEquals(3, mModelList.size());
        checkItemForExistingAccountRow(0, TestAccounts.ACCOUNT1);
        checkItemForExistingAccountRow(1, TestAccounts.ACCOUNT2);
        checkItemForAddAccountRow(2);
    }

    @Test
    public void testProfileDataUpdateWhenAccountPickerIsShownFromSettings() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mMediator =
                new AccountPickerMediator(
                        RuntimeEnvironment.getApplication(), mModelList, mListenerMock);
        mAccountManagerTestRule.addAccount(ACCOUNT1_DIFFERENT_NAME);
        // ACCOUNT_DIFFERENT_NAME, ADD_ACCOUNT
        Assert.assertEquals(2, mModelList.size());
        checkItemForExistingAccountRow(0, ACCOUNT1_DIFFERENT_NAME);
        checkItemForAddAccountRow(1);
    }

    private void checkItemForExistingAccountRow(int position, AccountInfo coreAccountInfo) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW, item.type);
        PropertyModel model = item.model;
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        Assert.assertEquals(coreAccountInfo.getEmail(), profileData.getAccountEmail());
        Assert.assertEquals(coreAccountInfo.getFullName(), profileData.getFullName());
        Assert.assertNotNull("Profile avatar should not be null!", profileData.getImage());

        model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER).run();
        verify(mListenerMock).onAccountSelected(coreAccountInfo);
    }

    private void checkItemForAddAccountRow(int position) {
        MVCListAdapter.ListItem item = mModelList.get(position);
        Assert.assertEquals(AccountPickerProperties.ItemType.ADD_ACCOUNT_ROW, item.type);
        item.model.get(AddAccountRowProperties.ON_CLICK_LISTENER).onClick(null);
        verify(mListenerMock).addAccount();
    }
}
