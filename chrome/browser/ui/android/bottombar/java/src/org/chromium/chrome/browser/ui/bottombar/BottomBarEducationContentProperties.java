// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties of the education content shared by the bottom bar bubble and the opt-in dialog. */
@NullMarked
class BottomBarEducationContentProperties {
    /** Illustration above the title. */
    static final ReadableObjectPropertyKey<Drawable> ILLUSTRATION =
            new ReadableObjectPropertyKey<>();

    /** Whether the illustration shows. It's hidden in landscape to save vertical space. */
    static final WritableBooleanPropertyKey IS_ILLUSTRATION_VISIBLE =
            new WritableBooleanPropertyKey();

    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    /** Body text, with a link that opens the Appearance settings. */
    static final ReadableObjectPropertyKey<CharSequence> BODY = new ReadableObjectPropertyKey<>();

    /** Whether the content shows its own buttons. The opt-in dialog uses its own instead. */
    static final ReadableBooleanPropertyKey IS_BUTTON_BAR_VISIBLE =
            new ReadableBooleanPropertyKey();

    /** Runs when the positive ("Show me") button is clicked. */
    static final ReadableObjectPropertyKey<Runnable> ON_POSITIVE_CLICKED =
            new ReadableObjectPropertyKey<>();

    /** Runs when the negative ("Switch back") button is clicked. */
    static final ReadableObjectPropertyKey<Runnable> ON_NEGATIVE_CLICKED =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ILLUSTRATION,
                IS_ILLUSTRATION_VISIBLE,
                TITLE,
                BODY,
                IS_BUTTON_BAR_VISIBLE,
                ON_POSITIVE_CLICKED,
                ON_NEGATIVE_CLICKED,
            };

    private BottomBarEducationContentProperties() {}
}
