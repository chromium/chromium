// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties defined here reflect the visible state of the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetProperties {
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {VISIBLE};

    private AtMemoryBottomSheetProperties() {}
}
