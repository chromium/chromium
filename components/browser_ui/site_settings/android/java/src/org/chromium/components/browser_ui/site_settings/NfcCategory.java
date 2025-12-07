// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.content_public.browser.BrowserContextHandle;

/** A class for dealing with the NFC category. */
@NullMarked
public class NfcCategory extends SiteSettingsCategory {
    public NfcCategory(BrowserContextHandle browserContextHandle) {
        // As NFC is not a per-app permission, passing an empty string means the NFC permission is
        // always enabled for Chrome.
        super(browserContextHandle, Type.NFC, /* androidPermission= */ "");
    }

    @Override
    protected boolean supportedGlobally() {
        return NfcSystemLevelSetting.isNfcAccessPossible();
    }

    @Override
    protected String getMessageIfNotSupported(Context context) {
        return context.getString(R.string.android_nfc_unsupported);
    }

    @Override
    protected boolean enabledGlobally() {
        return NfcSystemLevelSetting.isNfcSystemLevelSettingEnabled();
    }

    @Override
    protected @Nullable Intent getIntentToEnableOsGlobalPermission(Context context) {
        return NfcSystemLevelSetting.getNfcSystemLevelSettingIntent();
    }

    @Override
    protected String getMessageForEnablingOsGlobalPermission(Context context) {
        return context.getString(R.string.android_nfc_off_globally);
    }
}
