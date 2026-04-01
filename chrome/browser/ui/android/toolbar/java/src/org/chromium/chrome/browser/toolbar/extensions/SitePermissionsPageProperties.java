// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the site permissions page in the extensions menu. */
@NullMarked
public class SitePermissionsPageProperties {
    /** The ID of the extension whose permissions are being displayed. */
    public static final WritableObjectPropertyKey<String> EXTENSION_ID =
            new WritableObjectPropertyKey<>();

    /** Click listener for the back button to return to the main menu page. */
    public static final WritableObjectPropertyKey<View.OnClickListener> BACK_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {EXTENSION_ID, BACK_CLICK_LISTENER};
}
