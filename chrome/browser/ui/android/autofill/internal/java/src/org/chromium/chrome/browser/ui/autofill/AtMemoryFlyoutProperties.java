// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.util.List;

/** Properties defined here reflect the visible state of the {@link AtMemoryFlyoutContent}. */
@NullMarked
class AtMemoryFlyoutProperties {
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> SOURCE_TEXT = new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<List<Pair<String, String>>> CHIPS_DATA =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {TITLE, SOURCE_TEXT, CHIPS_DATA};

    private AtMemoryFlyoutProperties() {}
}
