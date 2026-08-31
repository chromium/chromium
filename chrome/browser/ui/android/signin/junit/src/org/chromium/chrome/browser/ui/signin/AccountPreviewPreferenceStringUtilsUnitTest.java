// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.signin.services.AccountPreviewPreference;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.protocol.SyncEnums.DeviceFormFactor;
import org.chromium.google_apis.gaia.GaiaId;

/** Unit tests for {@link AccountPreviewPreferenceStringUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountPreviewPreferenceStringUtilsUnitTest {
    private static final GaiaId GAIA_ID = new GaiaId("test_gaia_id");
    private final Context mContext = ApplicationProvider.getApplicationContext();
    private final String mPhoneStr = mContext.getString(R.string.signin_device_type_phone);
    private final String mTabletStr = mContext.getString(R.string.signin_device_type_tablet);
    private final String mLaptopStr = mContext.getString(R.string.signin_device_type_laptop);

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_emptyPreferredDataTypes_returnsNull() {
        assertNull(
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext, createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_unsupportedDataType_returnsNull() {
        assertNull(
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.READING_LIST)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_unsupportedFirstDataType_fallsThroughToSupported() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_bookmarks,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE,
                                DataType.READING_LIST,
                                DataType.BOOKMARKS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_bookmarks_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_bookmarks,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.BOOKMARKS)));
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_bookmarks,
                        mTabletStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_TABLET, DataType.BOOKMARKS)));
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_bookmarks,
                        mLaptopStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_DESKTOP, DataType.BOOKMARKS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_bookmarks_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_on_all_devices_bookmarks),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.BOOKMARKS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_passwords_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_passwords,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.PASSWORDS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_passwords_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_on_all_devices_passwords),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.PASSWORDS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_autofill_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_saved_info,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.AUTOFILL)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_autofill_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_on_all_devices_saved_info),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.AUTOFILL)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_walletMetadata_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_device_type_saved_info,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE,
                                DataType.AUTOFILL_WALLET_METADATA)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForDefaultFlow_walletMetadata_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_on_all_devices_saved_info),
                AccountPreviewPreferenceStringUtils.getSubtitleForDefaultFlow(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.AUTOFILL_WALLET_METADATA)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_emptyPreferredDataTypes_returnsNull() {
        assertNull(
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext, createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_unsupportedDataType_returnsNull() {
        assertNull(
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.READING_LIST)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_unsupportedFirstDataType_fallsThroughToSupported() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_bookmarks,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE,
                                DataType.READING_LIST,
                                DataType.BOOKMARKS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_bookmarks_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_bookmarks,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.BOOKMARKS)));
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_bookmarks,
                        mTabletStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_TABLET, DataType.BOOKMARKS)));
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_bookmarks,
                        mLaptopStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_DESKTOP, DataType.BOOKMARKS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_bookmarks_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_bookmarks),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.BOOKMARKS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_passwords_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_passwords,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.PASSWORDS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_passwords_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_passwords),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.PASSWORDS)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_autofill_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_saved_info,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE, DataType.AUTOFILL)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_autofill_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_saved_info),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.AUTOFILL)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_walletMetadata_withDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_saved_info,
                        mPhoneStr),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE,
                                DataType.AUTOFILL_WALLET_METADATA)));
    }

    @Test
    @SmallTest
    public void testGetSubtitleForWebSignin_walletMetadata_withoutDevice() {
        assertEquals(
                mContext.getString(
                        R.string
                                .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_saved_info),
                AccountPreviewPreferenceStringUtils.getSubtitleForWebSignin(
                        mContext,
                        createPref(
                                DeviceFormFactor.DEVICE_FORM_FACTOR_UNSPECIFIED,
                                DataType.AUTOFILL_WALLET_METADATA)));
    }

    private static AccountPreviewPreference createPref(
            DeviceFormFactor formFactor, @DataType int... dataTypes) {
        return new AccountPreviewPreference(GAIA_ID, dataTypes, formFactor);
    }
}
