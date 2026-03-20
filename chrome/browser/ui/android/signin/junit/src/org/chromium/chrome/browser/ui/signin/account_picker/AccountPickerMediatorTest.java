// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collection;

/**
 * Tests the class {@link AccountPickerMediator}.
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class AccountPickerMediatorTest {
    @Rule(order = Rule.DEFAULT_ORDER - 1)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameters(name = "{index}_isIdentityMgr={0}")
    public static Collection parameters() {
        return Arrays.asList(false, true);
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private AccountPickerCoordinator.Listener mListenerMock;

    private final MVCListAdapter.ModelList mModelList = new MVCListAdapter.ModelList();
    private final boolean mIsIdentityManagerSourceOfAccounts;

    private AccountPickerMediator mMediator;

    public AccountPickerMediatorTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
    }

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
                        RuntimeEnvironment.getApplication(),
                        mModelList,
                        mListenerMock,
                        mAccountManagerTestRule.getIdentityManager());
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
                        RuntimeEnvironment.getApplication(),
                        mModelList,
                        mListenerMock,
                        mAccountManagerTestRule.getIdentityManager());
        var accountWithDifferentName =
                new AccountInfo.Builder(TestAccounts.ACCOUNT1)
                        .fullName("Different Test1 Full")
                        .givenName("Different Test1 Given")
                        .build();
        mAccountManagerTestRule.addAccount(accountWithDifferentName);
        // accountWithDifferentName, ADD_ACCOUNT
        Assert.assertEquals(2, mModelList.size());
        checkItemForExistingAccountRow(0, accountWithDifferentName);
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
