// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.content_public.browser.BrowserContextHandle;

/** {@link SiteSettingsCategory} for dealing with Javascript-optimizer category. */
@NullMarked
public class JavascriptOptimizerCategory extends SiteSettingsCategory {
    private final boolean mBlockedByOs;
    private final boolean mBlockAddingException;

    public JavascriptOptimizerCategory(BrowserContextHandle browserContextHandle) {
        super(browserContextHandle, Type.JAVASCRIPT_OPTIMIZER, /* androidPermission= */ "");
        mBlockedByOs =
                !WebsitePreferenceBridge.isCategoryEnabled(
                                browserContextHandle, ContentSettingsType.JAVASCRIPT_OPTIMIZER)
                        && WebsitePreferenceBridge.isJavascriptOptimizerOsProvidedSetting(
                                browserContextHandle, ContentSettingsType.JAVASCRIPT_OPTIMIZER);
        mBlockAddingException =
                !WebsitePreferenceBridge.canAddExceptionsForJavascriptOptimizerSetting();
    }

    @Override
    protected boolean shouldDisableToggle() {
        return mBlockedByOs;
    }

    @Override
    protected @Nullable String getMessageWhyToggleIsDisabled(Context context) {
        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        return (provider == null) ? null : provider.getJavascriptOptimizerMessage(context);
    }

    @Override
    Drawable getDisabledInAndroidIcon(Context context) {
        Drawable icon =
                ApiCompatibilityUtils.getDrawable(
                        context.getResources(), R.drawable.secured_by_brand_shield_24);
        icon.mutate();
        int disabledColor = SemanticColorUtils.getDefaultControlColorActive(context);
        icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
        return icon;
    }

    @Override
    protected int getBlockAddingExceptionsReasonResourceId() {
        return mBlockAddingException
                ? R.string.website_settings_js_opt_add_exceptions_disabled_reason
                : 0;
    }
}
