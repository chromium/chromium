// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AccountMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountMenuMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private Runnable mDismissCallback;

    private Context mContext;
    private PropertyModel mModel;
    private AccountMenuMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

        mModel = new PropertyModel(AccountMenuProperties.ALL_KEYS);
        mMediator = new AccountMenuMediator(mContext, mModel, mDismissCallback);
    }

    @After
    public void tearDown() {
        SettingsNavigationFactory.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    public void testAutofillItemClick_dismissesAndOpensAutofillSettings() {
        OnClickListener clickListener = mModel.get(AccountMenuProperties.AUTOFILL_CLICK_LISTENER);
        assertNotNull(clickListener);

        clickListener.onClick(null);

        verify(mDismissCallback).run();
        verify(mSettingsNavigation)
                .startSettings(mContext, SettingsFragment.AUTOFILL_AND_PASSWORDS);
    }
}
