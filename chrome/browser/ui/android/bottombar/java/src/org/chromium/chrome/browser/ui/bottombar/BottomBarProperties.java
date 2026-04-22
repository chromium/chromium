// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for the bottom bar. */
@NullMarked
public class BottomBarProperties {
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_VISIBLE, COLOR_SCHEME};
}
