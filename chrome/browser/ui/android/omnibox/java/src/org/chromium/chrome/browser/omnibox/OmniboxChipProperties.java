// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties for the omnibox chip component. */
@NullMarked
class OmniboxChipProperties {
    /** The text shown in the chip. */
    public static final WritableObjectPropertyKey<String> TEXT = new WritableObjectPropertyKey<>();

    /** The icon shown in the chip. */
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    /** The content description of the chip. */
    public static final WritableObjectPropertyKey<String> CONTENT_DESC =
            new WritableObjectPropertyKey<>();

    /** The callback to be notified when the chip is clicked. */
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK =
            new WritableObjectPropertyKey<>();

    /** The available width for the chip to display itself. */
    public static final WritableIntPropertyKey AVAILABLE_WIDTH = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TEXT, ICON, CONTENT_DESC, ON_CLICK, AVAILABLE_WIDTH};
}
