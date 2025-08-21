// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with the Navigation Attachments bar. */
@NullMarked
class NavigationAttachmentsProperties {
    /** Whether the navigation toolbar is visible. */
    public static final WritableBooleanPropertyKey TOOLBAR_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Add button. */
    public static final WritableObjectPropertyKey<Runnable> BUTTON_ADD_CLICKED =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {TOOLBAR_VISIBLE, BUTTON_ADD_CLICKED};
}
