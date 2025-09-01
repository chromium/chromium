// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/** Exception information for a given origin. */
@NullMarked
public class ContentSettingException implements Serializable {
    private final @ContentSettingsType.EnumType int mContentSettingType;
    private final String mPrimaryPattern;
    private final @Nullable String mSecondaryPattern;
    // TODO(crbug.com/40231949): Convert {@link #mSource} to enum to enable merging {@link #mSource}
    // and {@link #mIsEmbargoed}.
    private final @ProviderType.EnumType int mSource;
    private final @Nullable Integer mExpirationInDays;
    private final boolean mIsEmbargoed;
    private @ContentSetting int mContentSetting;

    /**
     * Construct a ContentSettingException.
     *
     * @param type The content setting type this exception covers.
     * @param primaryPattern The primary host/domain pattern this exception covers.
     * @param secondaryPattern The secondary host/domain pattern this exception covers.
     * @param setting The setting for this exception, e.g. ALLOW or BLOCK.
     * @param source The source for this exception.
     * @param isEmbargoed Whether the site is under embargo for {@link type}.
     */
    public ContentSettingException(
            @ContentSettingsType.EnumType int type,
            String primaryPattern,
            @Nullable String secondaryPattern,
            @ContentSetting int setting,
            @ProviderType.EnumType int source,
            final @Nullable Integer expirationInDays,
            boolean isEmbargoed) {
        mContentSettingType = type;
        mPrimaryPattern = primaryPattern;
        mSecondaryPattern = secondaryPattern;
        mContentSetting = setting;
        mSource = source;
        mExpirationInDays = expirationInDays;
        mIsEmbargoed = isEmbargoed;
    }

    /**
     * Construct a ContentSettingException. Same as above but defaults secondaryPattern to wildcard.
     */
    public ContentSettingException(
            @ContentSettingsType.EnumType int type,
            String primaryPattern,
            @ContentSetting int setting,
            @ProviderType.EnumType int source,
            boolean isEmbargoed) {
        this(
                type,
                primaryPattern,
                SITE_WILDCARD,
                setting,
                source,
                /* expirationInDays= */ null,
                isEmbargoed);
    }

    public String getPrimaryPattern() {
        return mPrimaryPattern;
    }

    public @Nullable String getSecondaryPattern() {
        return mSecondaryPattern;
    }

    private String getSecondaryPatternSafe() {
        return (mSecondaryPattern == null) ? SITE_WILDCARD : mSecondaryPattern;
    }

    public String getDisplayPattern() {
        return mPrimaryPattern.equals(SITE_WILDCARD)
                ? getSecondaryPatternSafe()
                : getPrimaryPattern();
    }

    public @ProviderType.EnumType int getSource() {
        return mSource;
    }

    public @ContentSetting int getContentSetting() {
        return mContentSetting;
    }

    public @ContentSettingsType.EnumType int getContentSettingType() {
        return mContentSettingType;
    }

    public boolean hasExpiration() {
        return mExpirationInDays != null;
    }

    public @Nullable Integer getExpirationInDays() {
        return mExpirationInDays;
    }

    public boolean isEmbargoed() {
        return mIsEmbargoed;
    }

    /** Sets the content setting value for this exception. */
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSetting int value) {
        mContentSetting = value;
        WebsitePreferenceBridge.setContentSettingCustomScope(
                browserContextHandle,
                mContentSettingType,
                mPrimaryPattern,
                getSecondaryPatternSafe(),
                value);
    }
}
