// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link AccountMenuViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountMenuViewBinderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnClickListener mClickListener;

    private Activity mActivity;
    private TextView mItemView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mItemView =
                (TextView) LayoutInflater.from(mActivity).inflate(R.layout.account_menu_item, null);
        mModel = new PropertyModel(MenuItemProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mItemView, AccountMenuViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testBindTitle() {
        mModel.set(MenuItemProperties.TITLE_ID, R.string.menu_passwords_and_autofill);
        assertEquals(
                mActivity.getString(R.string.menu_passwords_and_autofill),
                mItemView.getText().toString());
    }

    @Test
    @SmallTest
    public void testBindStartIcon() {
        mModel.set(MenuItemProperties.START_ICON_ID, R.drawable.ic_password_manager_24dp);
        assertNotNull(mItemView.getCompoundDrawablesRelative()[0]);
    }

    @Test
    @SmallTest
    public void testBindClickListener() {
        mModel.set(MenuItemProperties.CLICK_LISTENER, mClickListener);
        mItemView.performClick();
        verify(mClickListener).onClick(mItemView);
    }
}
