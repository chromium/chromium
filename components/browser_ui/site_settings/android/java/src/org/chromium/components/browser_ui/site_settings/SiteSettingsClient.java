// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;

/**
 * An interface implemented by the embedder that allows the Site Settings UI to access
 * embedder-specific logic.
 */
// TODO(crbug.com/1077007): Clean up this interface.
public interface SiteSettingsClient {
    /**
     * @return The BrowserContextHandle that should be used to read and update settings.
     */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * @return the ManagedPreferenceDelegate instance that should be used when rendering
     *         Preferences.
     */
    ManagedPreferenceDelegate getManagedPreferenceDelegate();

    /**
     * @return The SiteSettingsHelpClient that should be used to provide help functionality to the
     *     Site Settings UI.
     */
    SiteSettingsHelpClient getSiteSettingsHelpClient();

    /**
     * @return The WebappSettingsClient that should be used when showing the Site Settings UI.
     */
    WebappSettingsClient getWebappSettingsClient();

    /**
     * Asynchronously looks up the locally cached favicon image for the given URL, generating a
     * fallback if one isn't available.
     *
     * @param faviconUrl The URL of the page to get the favicon for. If a favicon for the full URL
     *     can't be found, the favicon for its host will be used as a fallback.
     * @param callback A callback that will be called with the favicon bitmap, or null if no
     *     favicon could be found or generated.
     */
    void getFaviconImageForURL(String faviconUrl, Callback<Bitmap> callback);

    /**
     * @return true if the given category type should be shown in the SiteSettings Fragment.
     */
    boolean isCategoryVisible(@SiteSettingsCategory.Type int type);

    /**
     * @return true if the QuietNotificationPrompts Feature is enabled.
     */
    boolean isQuietNotificationPromptsFeatureEnabled();

    /**
     * @return The id of the notification channel associated with the given origin.
     */
    // TODO(crbug.com/1069895): Remove this once WebLayer supports notifications.
    String getChannelIdForOrigin(String origin);

    /**
     * @return The name of the app the settings are associated with.
     */
    String getAppName();

    /**
     * @return The user visible name of the app that will handle permission delegation for the
     *     origin and content setting type.
     */
    @Nullable
    String getDelegateAppNameForOrigin(Origin origin, @ContentSettingsType int type);

    /**
     * @return The package name of the app that should handle permission delegation for the origin
     *     and content setting type.
     */
    @Nullable
    String getDelegatePackageNameForOrigin(Origin origin, @ContentSettingsType int type);

    /**
     * @return true if PageInfo V2 is enabled.
     */
    boolean isPageInfoV2Enabled();
}
