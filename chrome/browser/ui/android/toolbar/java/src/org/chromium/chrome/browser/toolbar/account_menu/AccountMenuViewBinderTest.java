// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.IdentityCardProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.PromoCardProperties;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link AccountMenuViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountMenuViewBinderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnClickListener mClickListener;

    private Activity mActivity;
    private Drawable mAvatar;
    private TextView mItemView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mAvatar = new ColorDrawable(Color.BLUE);
        mItemView =
                (TextView) LayoutInflater.from(mActivity).inflate(R.layout.account_menu_item, null);
        mModel = new PropertyModel(MenuItemProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mItemView, AccountMenuViewBinder::bindMenuItem);
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

    @Test
    @SmallTest
    public void testBindPromoCard_onSigninClickListener() {
        View promoView =
                LayoutInflater.from(mActivity).inflate(R.layout.account_menu_promo_card, null);
        View signinButton = promoView.findViewById(R.id.account_menu_signin_button);

        PropertyModel model =
                new PropertyModel.Builder(PromoCardProperties.ALL_KEYS)
                        .with(PromoCardProperties.ON_SIGNIN_CLICK_LISTENER, mClickListener)
                        .build();
        PropertyModelChangeProcessor.create(model, promoView, AccountMenuViewBinder::bindPromoCard);

        signinButton.performClick();
        verify(mClickListener).onClick(signinButton);
    }

    @Test
    @SmallTest
    public void testBindIdentityCard() {
        View cardView =
                bindIdentityCard(
                        "John Doe", "test@gmail.com", /* hasDisplayableEmailAddress= */ true);
        ImageView avatarView = cardView.findViewById(R.id.account_menu_identity_avatar);
        TextView nameView = cardView.findViewById(R.id.account_menu_identity_name);
        TextView emailView = cardView.findViewById(R.id.account_menu_identity_email);

        assertEquals(mAvatar, avatarView.getDrawable());
        assertEquals("John Doe", nameView.getText().toString());
        assertEquals("test@gmail.com", emailView.getText().toString());
        assertEquals(View.VISIBLE, emailView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindIdentityCard_emptyFullName_fallsBackToEmail() {
        View cardView =
                bindIdentityCard(
                        /* fullName= */ null,
                        "test@gmail.com",
                        /* hasDisplayableEmailAddress= */ true);
        TextView nameView = cardView.findViewById(R.id.account_menu_identity_name);
        TextView emailView = cardView.findViewById(R.id.account_menu_identity_email);

        assertEquals("test@gmail.com", nameView.getText().toString());
        assertEquals(View.GONE, emailView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindIdentityCard_nonDisplayableEmail() {
        View cardView =
                bindIdentityCard(
                        "Child User", "test@gmail.com", /* hasDisplayableEmailAddress= */ false);
        TextView nameView = cardView.findViewById(R.id.account_menu_identity_name);
        TextView emailView = cardView.findViewById(R.id.account_menu_identity_email);

        assertEquals("Child User", nameView.getText().toString());
        assertEquals(View.GONE, emailView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindIdentityCard_nonDisplayableEmailAndEmptyFullName() {
        View cardView =
                bindIdentityCard(
                        /* fullName= */ null,
                        "test@gmail.com",
                        /* hasDisplayableEmailAddress= */ false);
        TextView nameView = cardView.findViewById(R.id.account_menu_identity_name);
        TextView emailView = cardView.findViewById(R.id.account_menu_identity_email);

        assertEquals(
                mActivity.getString(
                        org.chromium.chrome.browser.signin.services.R.string
                                .default_google_account_username),
                nameView.getText().toString());
        assertEquals(View.GONE, emailView.getVisibility());
    }

    private View bindIdentityCard(
            @Nullable String fullName, String email, boolean hasDisplayableEmailAddress) {
        DisplayableProfileData profileData =
                new DisplayableProfileData(
                        new CoreAccountId(new GaiaId("account_id")),
                        email,
                        mAvatar,
                        fullName,
                        /* givenName= */ null,
                        hasDisplayableEmailAddress,
                        /* hasAiTierRing= */ false);
        View cardView =
                LayoutInflater.from(mActivity).inflate(R.layout.account_menu_identity_card, null);
        PropertyModel model = IdentityCardProperties.createModel(profileData);
        PropertyModelChangeProcessor.create(
                model, cardView, AccountMenuViewBinder::bindIdentityCard);
        return cardView;
    }
}
