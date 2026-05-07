// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
public class ExtensionActionHoverCardProperties {
    public static final WritableObjectPropertyKey<String> ACTION_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SITE_ACCESS_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SITE_ACCESS_DESC =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> POLICY_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        ACTION_TITLE, SITE_ACCESS_TITLE, SITE_ACCESS_DESC, POLICY_TEXT
    };
}
