// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntDefPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with a single fusebox attachment item. */
@NullMarked
class FuseboxAttachmentProperties {
    /** The complete attachment data. */
    public static final WritableObjectPropertyKey<FuseboxAttachment> ATTACHMENT =
            new WritableObjectPropertyKey<>();

    /** The variant of {@link BrandedColorScheme} to apply to the UI elements. */
    public static final WritableIntDefPropertyKey<BrandedColorScheme> COLOR_SCHEME =
            new WritableIntDefPropertyKey<>(BrandedColorScheme.APP_DEFAULT);

    /** The handler for a remove button click. */
    public static final WritableObjectPropertyKey<Runnable> ON_REMOVE =
            new WritableObjectPropertyKey<>();

    /** Whether the remove button should be selected. */
    public static final WritableBooleanPropertyKey REMOVE_BUTTON_SELECTED =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        ATTACHMENT, COLOR_SCHEME, ON_REMOVE, REMOVE_BUTTON_SELECTED
    };
}
