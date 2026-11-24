// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import static java.util.Arrays.asList;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.LoginButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ShowIdpPickerProperties;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerItemDecoration;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

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
                            .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
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
                            .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
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
                            .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, rpContext.mValue)
                            .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                            .with(HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER, true)
                            .with(HeaderProperties.IS_SHOWING_MULTIPLE_IDPS, false)
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
                        buildAccountItem(mAnaAccount, /* showIdp= */ false),
                        buildAccountItem(mNoOneAccount, /* showIdp= */ false),
                        buildAccountItem(mBobAccount, /* showIdp= */ false)));
        RobolectricUtil.runAllBackgroundAndUi();

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
        TextView displayIdentifier = accountChip.findViewById(R.id.description);
        assertEquals(mAnaAccount.getDisplayIdentifier(), displayIdentifier.getText());
    }

    @Test
    public void testRequestPermissionDialogRpIconDisplayed() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(
                                HeaderProperties.HEADER_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .with(
                                HeaderProperties.RP_BRAND_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_icon);
        ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
        ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

        assertTrue(idpBrandIcon.isShown());
        assertTrue(rpBrandIcon.isShown());
        assertNull(rpBrandIcon.getImageTintList());
        assertTrue(arrowRangeIcon.isShown());
    }

    @Test
    public void testHeaderTypesWithRpIconHidden() {
        List<@HeaderType Integer> headerTypesWithoutRpIcon =
                Arrays.asList(
                        HeaderType.SIGN_IN,
                        HeaderType.SIGN_IN_TO_IDP_STATIC,
                        HeaderType.SIGN_IN_ERROR,
                        HeaderType.LOADING,
                        HeaderType.IDP_PICKER);
        for (@HeaderType int headerType : headerTypesWithoutRpIcon) {
            mModel.set(
                    ItemProperties.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, headerType)
                            .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                            .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                            .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                            .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                            .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                            .with(
                                    HeaderProperties.HEADER_ICON,
                                    Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                            .with(
                                    HeaderProperties.RP_BRAND_ICON,
                                    Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                            .build());
            assertEquals(View.VISIBLE, mContentView.getVisibility());
            ImageView idpBrandIcon = mContentView.findViewById(R.id.header_icon);
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
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(
                                HeaderProperties.HEADER_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .with(HeaderProperties.RP_BRAND_ICON, null)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_icon);
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
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(HeaderProperties.HEADER_ICON, null)
                        .with(
                                HeaderProperties.RP_BRAND_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_icon);
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
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(HeaderProperties.HEADER_ICON, null)
                        .with(HeaderProperties.RP_BRAND_ICON, null)
                        .build());
        assertEquals(View.VISIBLE, mContentView.getVisibility());
        ImageView idpBrandIcon = mContentView.findViewById(R.id.header_icon);
        ImageView rpBrandIcon = mContentView.findViewById(R.id.header_rp_icon);
        ImageView arrowRangeIcon = mContentView.findViewById(R.id.arrow_range_icon);

        assertFalse(idpBrandIcon.isShown());
        assertFalse(rpBrandIcon.isShown());
        assertFalse(arrowRangeIcon.isShown());
    }

    @Test
    public void testMultiIdpAccountChooserHeader() {
        mModel.set(ItemProperties.HEADER, buildMultiIdpHeader().build());

        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);
        assertEquals("Sign in to example.org", title.getText().toString());
        assertEquals("Choose an account to continue", subtitle.getText().toString());
        assertTrue(subtitle.isShown());
    }

    @Test
    public void testMultiIdpHeaderShowsFallbackIcon() {
        mModel.set(
                ItemProperties.HEADER,
                buildMultiIdpHeader().with(HeaderProperties.HEADER_ICON, null).build());

        ImageView headerIcon = mContentView.findViewById(R.id.header_icon);
        assertTrue(headerIcon.isShown());
        assertNotNull(headerIcon.getDrawable());
    }

    @Test
    public void testMultiIdpAccountChooserHeaderWithIframe() {
        mModel.set(
                ItemProperties.HEADER,
                buildMultiIdpHeader()
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "iframe.org")
                        .build());

        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);
        assertEquals("Sign in to iframe.org", title.getText().toString());
        assertEquals("example.org embeds content from iframe.org", subtitle.getText().toString());
    }

    @Test
    public void testMultiIdpLoadingHeader() {
        mModel.set(
                ItemProperties.HEADER,
                buildMultiIdpHeader()
                        .with(HeaderProperties.TYPE, HeaderType.LOADING)
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "")
                        .with(HeaderProperties.HEADER_ICON, null)
                        .build());

        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);
        assertEquals("Sign in to example.org", title.getText().toString());
        // There is nothing to choose yet.
        assertFalse(subtitle.isShown());
        assertFalse(mContentView.findViewById(R.id.header_icon).isShown());
    }

    @Test
    public void testMultiIdpFallbackIconHiddenOnNextSheetWithoutIcon() {
        mModel.set(
                ItemProperties.HEADER,
                buildMultiIdpHeader().with(HeaderProperties.HEADER_ICON, null).build());
        ImageView headerIcon = mContentView.findViewById(R.id.header_icon);
        assertTrue(headerIcon.isShown());

        // The sheet for the picked account is about one IDP, which has no icon here.
        mModel.set(
                ItemProperties.HEADER,
                buildMultiIdpHeader()
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.IS_SHOWING_MULTIPLE_IDPS, false)
                        .with(HeaderProperties.HEADER_ICON, null)
                        .build());
        assertFalse(headerIcon.isShown());
    }

    @Test
    public void testSingleIdpHeaderWithoutIconLeavesPreviousIconUntouched() {
        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(HeaderProperties.IS_SHOWING_MULTIPLE_IDPS, false)
                        .with(HeaderProperties.IS_MULTI_IDP_REQUEST, false)
                        .with(
                                HeaderProperties.HEADER_ICON,
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888))
                        .build());
        ImageView headerIcon = mContentView.findViewById(R.id.header_icon);
        assertTrue(headerIcon.isShown());
        Drawable previousDrawable = headerIcon.getDrawable();
        assertNotNull(previousDrawable);

        mModel.set(
                ItemProperties.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.TYPE, HeaderType.REQUEST_PERMISSION_MODAL)
                        .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                        .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                        .with(HeaderProperties.IDP_FOR_DISPLAY, "idp.org")
                        .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                        .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                        .with(HeaderProperties.IS_SHOWING_MULTIPLE_IDPS, false)
                        .with(HeaderProperties.IS_MULTI_IDP_REQUEST, false)
                        .with(HeaderProperties.HEADER_ICON, null)
                        .build());
        assertTrue(headerIcon.isShown());
        assertSame(previousDrawable, headerIcon.getDrawable());
    }

    @Test
    public void testIdpPickerHeader() {
        mModel.set(
                ItemProperties.HEADER,
                buildMultiIdpHeader()
                        .with(HeaderProperties.TYPE, HeaderType.IDP_PICKER)
                        .with(HeaderProperties.HEADER_ICON, null)
                        .build());

        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);
        assertEquals("Sign in to example.org", title.getText().toString());
        assertEquals("Choose a provider to continue", subtitle.getText().toString());
        assertTrue(subtitle.isShown());
        ImageView headerIcon = mContentView.findViewById(R.id.header_icon);
        assertTrue(headerIcon.isShown());
        assertNotNull(headerIcon.getDrawable());
    }

    @Test
    public void testIdpPickerItemsDisplayedAndClickable() {
        ButtonData[] clicked = new ButtonData[1];
        mSheetAccountItems.addAll(
                asList(
                        buildIdpItem(mIdpData, data -> clicked[0] = data),
                        buildIdpItem(mIdpDataWithoutIcons, mIdpLoginCallback)));
        RobolectricUtil.runAllBackgroundAndUi();

        RecyclerView itemsList = mContentView.findViewById(R.id.sheet_item_list);
        assertEquals(2, itemsList.getAdapter().getItemCount());
        assertEquals(
                AccountSelectionProperties.ITEM_TYPE_IDP,
                itemsList.getAdapter().getItemViewType(0));
        assertEquals(
                AccountSelectionProperties.ITEM_TYPE_IDP,
                itemsList.getAdapter().getItemViewType(1));

        View firstRow = itemsList.getChildAt(0);
        ImageView firstStartIcon = firstRow.findViewById(R.id.start_icon);
        TextView firstTitle = firstRow.findViewById(R.id.title);
        ImageView firstEndIcon = firstRow.findViewById(R.id.end_icon);
        assertTrue(firstStartIcon.isShown());
        assertTrue(firstStartIcon.getDrawable() instanceof BitmapDrawable);
        assertEquals(mIdpData.getIdpForDisplay(), firstTitle.getText().toString());
        assertEquals(
                mIdpData.getIdpForDisplay() + ", opens in a new tab",
                firstTitle.getContentDescription().toString());
        assertTrue(firstEndIcon.isShown());

        View secondRow = itemsList.getChildAt(1);
        ImageView secondStartIcon = secondRow.findViewById(R.id.start_icon);
        TextView secondTitle = secondRow.findViewById(R.id.title);
        assertTrue(secondStartIcon.isShown());
        // Globe fallback drawable is set when no brand icon is present.
        assertNotNull(secondStartIcon.getDrawable());
        assertFalse(secondStartIcon.getDrawable() instanceof BitmapDrawable);
        assertEquals(mIdpDataWithoutIcons.getIdpForDisplay(), secondTitle.getText().toString());

        firstRow.performClick();
        assertNotNull(clicked[0]);
        assertNull(clicked[0].mAccount);
        assertSame(mIdpData.getIdpMetadata(), clicked[0].mIdpMetadata);
    }

    @Test
    public void testIdpPickerFallsBackToGlobeForInvalidBrandIcons() {
        IdentityProviderData undersizedIconIdp =
                createIdpDataWithBrandIcon(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888));
        IdentityProviderData nonSquareIconIdp =
                createIdpDataWithBrandIcon(Bitmap.createBitmap(100, 50, Bitmap.Config.ARGB_8888));
        mSheetAccountItems.addAll(
                asList(
                        buildIdpItem(undersizedIconIdp, mIdpLoginCallback),
                        buildIdpItem(nonSquareIconIdp, mIdpLoginCallback)));
        RobolectricUtil.runAllBackgroundAndUi();

        RecyclerView itemsList = mContentView.findViewById(R.id.sheet_item_list);
        for (int i = 0; i < 2; i++) {
            ImageView startIcon = itemsList.getChildAt(i).findViewById(R.id.start_icon);
            assertNotNull(startIcon.getDrawable());
            assertFalse(startIcon.getDrawable() instanceof BitmapDrawable);
        }
    }

    @Test
    public void testShowIdpPickerRowDisplayedAndClickable() {
        boolean[] clicked = new boolean[1];
        mSheetAccountItems.add(
                new MVCListAdapter.ListItem(
                        AccountSelectionProperties.ITEM_TYPE_SHOW_IDP_PICKER,
                        new PropertyModel.Builder(ShowIdpPickerProperties.ALL_KEYS)
                                .with(
                                        ShowIdpPickerProperties.ON_CLICK_LISTENER,
                                        () -> clicked[0] = true)
                                .build()));
        RobolectricUtil.runAllBackgroundAndUi();

        RecyclerView itemsList = mContentView.findViewById(R.id.sheet_item_list);
        View pickerRow = itemsList.getChildAt(0);
        ImageView startIcon = pickerRow.findViewById(R.id.start_icon);
        assertTrue(startIcon.isShown());
        assertNotNull(startIcon.getDrawable());
        TextView title = pickerRow.findViewById(R.id.title);
        assertEquals("Use a different account", title.getText().toString());
        assertEquals("Use a different account", title.getContentDescription().toString());
        pickerRow.performClick();
        assertTrue(clicked[0]);
    }

    private IdentityProviderData createIdpDataWithBrandIcon(Bitmap brandIcon) {
        IdentityProviderMetadata metadata =
                new IdentityProviderMetadata(
                        Color.BLUE,
                        Color.GREEN,
                        brandIcon,
                        mTestConfigUrl,
                        mTestLoginUrl,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        /* showUseDifferentAccountButton= */ false);
        return new IdentityProviderData(
                mTestEtldPlusOne2,
                metadata,
                mIdpData.getClientMetadata(),
                RpContext.SIGN_IN,
                DEFAULT_DISCLOSURE_FIELDS,
                /* hasLoginStatusMismatch= */ false);
    }

    private MVCListAdapter.ListItem buildIdpItem(
            IdentityProviderData idpData, Callback<ButtonData> onClickListener) {
        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ITEM_TYPE_IDP,
                new PropertyModel.Builder(IdpProperties.ALL_KEYS)
                        .with(
                                IdpProperties.PROPERTIES,
                                new IdpProperties.Properties(idpData, onClickListener))
                        .build());
    }

    private PropertyModel.Builder buildMultiIdpHeader() {
        return new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                .with(HeaderProperties.RP_FOR_DISPLAY, "example.org")
                .with(HeaderProperties.IFRAME_FOR_DISPLAY, "")
                .with(HeaderProperties.IDP_FOR_DISPLAY, null)
                .with(HeaderProperties.RP_CONTEXT, RpContext.SIGN_IN)
                .with(HeaderProperties.RP_MODE, RpMode.ACTIVE)
                .with(HeaderProperties.IS_SHOWING_MULTIPLE_IDPS, true)
                .with(HeaderProperties.IS_MULTI_IDP_REQUEST, true);
    }

    private PropertyModel buildAddAccountButton() {
        LoginButtonProperties.Properties properties =
                new LoginButtonProperties.Properties(
                        mIdpData, mIdpLoginCallback, RpMode.ACTIVE, /* showIdp= */ false);
        return new PropertyModel.Builder(LoginButtonProperties.ALL_KEYS)
                .with(LoginButtonProperties.PROPERTIES, properties)
                .build();
    }
}
