// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Properties defined here reflect the visible state of the {@link AtMemoryFlyoutContent}. */
@NullMarked
class AtMemoryFlyoutProperties {
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> SOURCE_TEXT = new ReadableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<List<Pair<String, String>>> CHIPS_DATA =
            new WritableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Runnable> ON_BACK_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Runnable> ON_SOURCE_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Runnable> ON_MANAGE_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<Callback<String>> ON_CHIP_CLICKED =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        TITLE,
        SOURCE_TEXT,
        CHIPS_DATA,
        ON_BACK_CLICKED,
        ON_SOURCE_CLICKED,
        ON_MANAGE_CLICKED,
        ON_CHIP_CLICKED
    };

    private AtMemoryFlyoutProperties() {}
}
