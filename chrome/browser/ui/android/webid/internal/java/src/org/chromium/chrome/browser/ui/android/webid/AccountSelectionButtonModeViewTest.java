// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static java.util.Arrays.asList;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AddAccountButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerItemDecoration;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;

/**
 * View tests for the Account Selection Active Mode component ensure that model changes are
 * reflected in the sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionButtonModeViewTest extends AccountSelectionJUnitTestBase {
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
    public void setUp() {
        mRpMode = RpMode.ACTIVE;
        super.setUp();
    }

    @Test
    public void testDragHandlebarShown() {
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        View handlebar = mContentView.findViewById(R.id.drag_handlebar);
        assertTrue(handlebar.isShown());
    }

    @Test
    public void testAccountChooserRpContextDisplayed() {
        for (RpContextEntry rpContext : mRpContexts) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, rpContext.mValue)
                            .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            TextView title = mContentView.findViewById(R.id.header_title);
            TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

            assertEquals(
                    "Incorrect title",
                    mResources.getString(rpContext.mTitleId, "idp.org"),
                    title.getText().toString());
            assertEquals("Incorrect subtitle", "example.org", subtitle.getText().toString());
        }
    }

    @Test
    public void testRequestPermissionRpContextDisplayed() {
        for (RpContextEntry rpContext : mRpContexts) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, rpContext.mValue)
                            .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            TextView title = mContentView.findViewById(R.id.header_title);
            TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

            assertEquals(
                    "Incorrect title",
                    mResources.getString(rpContext.mTitleId, "idp.org"),
                    title.getText().toString());
            assertEquals("Incorrect subtitle", "example.org", subtitle.getText().toString());
        }
    }

    @Test
    public void testMultipleAccountsSubtitleDisplayed() {
        for (RpContextEntry rpContext : mRpContexts) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, rpContext.mValue)
                            .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                            .with(HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER, true)
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            TextView subtitle = mContentView.findViewById(R.id.header_subtitle);

            assertEquals(
                    "Incorrect subtitle",
                    "Choose an account to continue on example.org",
                    subtitle.getText().toString());
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

    /** Tests that the brand background color is the add account secondary button's text color. */
    @Test
    public void testAddAccountButtonBranding() {
        mModel.set(ItemProperties.ADD_ACCOUNT_BUTTON, buildAddAccountButton());

        assertEquals(View.VISIBLE, mContentView.getVisibility());

        TextView addAccountButton =
                mContentView.findViewById(R.id.account_selection_add_account_btn);

        final int expectedTextColor = mIdpMetadata.getBrandBackgroundColor();
        assertEquals(expectedTextColor, addAccountButton.getTextColors().getDefaultColor());
    }

    @Test
    public void testScrimShown() {
        // Default scrim is displayed if this returns false.
        assertFalse(mBottomSheetContent.hasCustomScrimLifecycle());
    }

    @Test
    public void testLoadingSpinnerShown() {
        mModel.set(ItemProperties.SPINNER_ENABLED, true);

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        View spinner = mContentView.findViewById(R.id.spinner);
        assertTrue(spinner.isShown());
    }

    @Test
    public void testLoadingSpinnerHidden() {
        mModel.set(ItemProperties.SPINNER_ENABLED, false);

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        View spinner = mContentView.findViewById(R.id.spinner);
        assertFalse(spinner.isShown());
    }

    @Test
    public void testAccountChipDisplayed() {
        mModel.set(
                ItemProperties.ACCOUNT_CHIP,
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, mAnaAccount)
                        .with(AccountProperties.ON_CLICK_LISTENER, null)
                        .build());

        assertEquals(View.VISIBLE, mContentView.getVisibility());
        View accountChip = mContentView.findViewById(R.id.account_chip);
        assertTrue(accountChip.isShown());
        TextView email = accountChip.findViewById(R.id.description);
        assertEquals(mAnaAccount.getEmail(), email.getText());
    }

    @Test
    public void testRequestPermissionDialogRpIconDisplayed() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(
                                HeaderProperties.IDP_BRAND_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .with(
                                HeaderProperties.RP_BRAND_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_idp_icon);
        ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
        ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

        assertTrue(idpBrandIcon.isShown());
        assertTrue(rpBrandIcon.isShown());
        assertNull(rpBrandIcon.getImageTintList());
        assertTrue(arrowRangeIcon.isShown());
    }

    @Test
    public void testHeaderTypesWithRpIconHidden() {
        List<HeaderType> headerTypesWithoutRpIcon =
                Arrays.asList(
                        HeaderType.SIGN_IN,
                        HeaderType.SIGN_IN_TO_IDP_STATIC,
                        HeaderType.SIGN_IN_ERROR,
                        HeaderType.LOADING);
        for (HeaderType headerType : headerTypesWithoutRpIcon) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, headerType)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                            .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                            .with(
                                    HeaderProperties.IDP_BRAND_ICON,
                                    Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                            .with(
                                    HeaderProperties.RP_BRAND_ICON,
                                    Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            ImageView idpBrandIcon = mContentView.findViewById(R.id.header_idp_icon);
            ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
            ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

            assertTrue(idpBrandIcon.isShown());
            assertFalse(rpBrandIcon.isShown());
            assertFalse(arrowRangeIcon.isShown());
        }
    }

    @Test
    public void testRpIconUnavailableRpIconHidden() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(
                                HeaderProperties.IDP_BRAND_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .with(HeaderProperties.RP_BRAND_ICON, null)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_idp_icon);
        ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
        ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

        assertTrue(idpBrandIcon.isShown());
        assertFalse(rpBrandIcon.isShown());
        assertFalse(arrowRangeIcon.isShown());
    }

    @Test
    public void testIdpIconUnavailableBothIconsHidden() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(HeaderProperties.IDP_BRAND_ICON, null)
                        .with(
                                HeaderProperties.RP_BRAND_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_idp_icon);
        ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
        ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

        assertFalse(idpBrandIcon.isShown());
        assertFalse(rpBrandIcon.isShown());
        assertFalse(arrowRangeIcon.isShown());
    }

    @Test
    public void testIdpAndRpIconsUnavailableBothIconsHidden() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(HeaderProperties.IDP_BRAND_ICON, null)
                        .with(HeaderProperties.RP_BRAND_ICON, null)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_idp_icon);
        ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
        ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

        assertFalse(idpBrandIcon.isShown());
        assertFalse(rpBrandIcon.isShown());
        assertFalse(arrowRangeIcon.isShown());
    }

    private PropertyModel buildAddAccountButton() {
        AddAccountButtonProperties.Properties properties =
                new AddAccountButtonProperties.Properties();
        properties.mIdpMetadata = mIdpMetadata;
        return new PropertyModel.Builder(AddAccountButtonProperties.ALL_KEYS)
                .with(AddAccountButtonProperties.PROPERTIES, properties)
                .build();
    }
}
