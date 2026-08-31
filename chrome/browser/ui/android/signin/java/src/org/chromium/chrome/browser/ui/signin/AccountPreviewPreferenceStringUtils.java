// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.AccountPreviewPreference;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.protocol.SyncEnums.DeviceFormFactor;

/**
 * Utility methods for formatting strings based on {@link AccountPreviewPreference}.
 *
 * <p>TODO(crbug.com/553530451): Simplify this with per-access-point string delegates.
 */
@NullMarked
public final class AccountPreviewPreferenceStringUtils {
    private AccountPreviewPreferenceStringUtils() {}

    /**
     * Returns the default subtitle string for sign-in UI showing the preferred account.
     *
     * <p>TODO(crbug.com/553530451): Simplify this with per-access-point string delegates.
     *
     * @param context The context used to retrieve strings.
     * @param preference The preference containing the preferred account's data types and the form
     *     factor of the other preferred device having synced data.
     * @return The formatted subtitle string, or {@code null} if no customized subtitle should be
     *     shown.
     */
    public static @Nullable String getSubtitleForDefaultFlow(
            Context context, AccountPreviewPreference preference) {
        return getSubtitle(
                context, preference, AccountPreviewPreferenceStringUtils::getDefaultStringId);
    }

    /**
     * Returns the subtitle string for web sign-in bottom sheet showing the preferred account.
     *
     * <p>TODO(crbug.com/553530451): Simplify this with per-access-point string delegates.
     *
     * @param context The context used to retrieve strings.
     * @param preference The preference containing the preferred account's data types and the form
     *     factor of the other preferred device having synced data.
     * @return The formatted subtitle string, or {@code null} if no customized subtitle should be
     *     shown.
     */
    public static @Nullable String getSubtitleForWebSignin(
            Context context, AccountPreviewPreference preference) {
        return getSubtitle(
                context, preference, AccountPreviewPreferenceStringUtils::getWebSigninStringId);
    }

    private static @Nullable String getSubtitle(
            Context context,
            AccountPreviewPreference preference,
            StringIdProvider stringIdProvider) {
        final @DataType int[] preferredDataTypes = preference.getPreferredDataTypes();
        if (preferredDataTypes.length == 0) {
            return null;
        }
        @Nullable String deviceType =
                getDeviceTypeString(context, preference.getOtherDeviceFormFactor());

        for (@DataType int dataType : preferredDataTypes) {
            @StringRes int stringId = stringIdProvider.getStringId(dataType, deviceType != null);
            if (stringId != 0) {
                return deviceType != null
                        ? context.getString(stringId, deviceType)
                        : context.getString(stringId);
            }
        }
        return null;
    }

    private static @StringRes int getWebSigninStringId(@DataType int dataType, boolean hasDevice) {
        return switch (dataType) {
            case DataType.BOOKMARKS ->
                    hasDevice
                            ? R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_bookmarks
                            : R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_bookmarks;
            case DataType.PASSWORDS ->
                    hasDevice
                            ? R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_passwords
                            : R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_passwords;
            case DataType.AUTOFILL, DataType.AUTOFILL_WALLET_METADATA ->
                    hasDevice
                            ? R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_web_signin_device_type_saved_info
                            : R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_web_signin_on_all_devices_saved_info;
            default -> 0;
        };
    }

    private static @StringRes int getDefaultStringId(@DataType int dataType, boolean hasDevice) {
        return switch (dataType) {
            case DataType.BOOKMARKS ->
                    hasDevice
                            ? R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_device_type_bookmarks
                            : R.string
                                    .signin_account_picker_bottom_sheet_subtitle_on_all_devices_bookmarks;
            case DataType.PASSWORDS ->
                    hasDevice
                            ? R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_device_type_passwords
                            : R.string
                                    .signin_account_picker_bottom_sheet_subtitle_on_all_devices_passwords;
            case DataType.AUTOFILL, DataType.AUTOFILL_WALLET_METADATA ->
                    hasDevice
                            ? R.string
                                    .signin_account_picker_bottom_sheet_subtitle_for_device_type_saved_info
                            : R.string
                                    .signin_account_picker_bottom_sheet_subtitle_on_all_devices_saved_info;
            default -> 0;
        };
    }

    private static @Nullable String getDeviceTypeString(
            Context context, DeviceFormFactor formFactor) {
        return switch (formFactor) {
            case DEVICE_FORM_FACTOR_PHONE -> context.getString(R.string.signin_device_type_phone);
            case DEVICE_FORM_FACTOR_TABLET -> context.getString(R.string.signin_device_type_tablet);
            case DEVICE_FORM_FACTOR_DESKTOP ->
                    context.getString(R.string.signin_device_type_laptop);
            default -> null;
        };
    }

    @FunctionalInterface
    private interface StringIdProvider {
        @StringRes
        int getStringId(@DataType int dataType, boolean hasDevice);
    }
}
