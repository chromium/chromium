// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** The properties associated with the Navigation Attachments bar. */
@NullMarked
class NavigationAttachmentsProperties {
    /** Whether the navigation toolbar is visible. */
    public static final WritableBooleanPropertyKey TOOLBAR_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TOOLBAR_VISIBLE};
}
