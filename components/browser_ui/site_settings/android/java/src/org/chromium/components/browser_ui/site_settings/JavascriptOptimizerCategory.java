// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.content_public.browser.BrowserContextHandle;

/** {@link SiteSettingsCategory} for dealing with Javascript-optimizer category. */
public class JavascriptOptimizerCategory extends SiteSettingsCategory {
    private boolean mBlockedByOs;

    public JavascriptOptimizerCategory(BrowserContextHandle browserContextHandle) {
        super(browserContextHandle, Type.JAVASCRIPT_OPTIMIZER, /* androidPermission= */ "");
        mBlockedByOs =
                !WebsitePreferenceBridge.isCategoryEnabled(
                                browserContextHandle, ContentSettingsType.JAVASCRIPT_OPTIMIZER)
                        && WebsitePreferenceBridge.isJavascriptOptimizerOsProvidedSetting(
                                browserContextHandle, ContentSettingsType.JAVASCRIPT_OPTIMIZER);
    }

    @Override
    protected boolean enabledGlobally() {
        return !mBlockedByOs;
    }

    @Override
    protected boolean isToggleDisabled() {
        return mBlockedByOs;
    }

    @Override
    protected boolean shouldShowWarningWhenBlocked() {
        return mBlockedByOs;
    }

    @Override
    protected String getMessageForEnablingOsGlobalPermission(Context context) {
        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        return (provider == null) ? null : provider.getJavascriptOptimizerMessage(context);
    }
}
