// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Interface implemented by the embedder to access embedder-specific logic. */
@NullMarked
public interface TrackingProtectionDelegate {
    /**
     * @return whether block all 3PC pref is enabled.
     */
    boolean isBlockAll3pcEnabled();

    /** Set the value of the block all 3PC pref. */
    void setBlockAll3pc(boolean enabled);

    /**
     * @return the browser context associated with the settings page.
     */
    BrowserContextHandle getBrowserContext();

    /** @return the site settings delegate object. */
    SiteSettingsDelegate getSiteSettingsDelegate(Context context);
}
