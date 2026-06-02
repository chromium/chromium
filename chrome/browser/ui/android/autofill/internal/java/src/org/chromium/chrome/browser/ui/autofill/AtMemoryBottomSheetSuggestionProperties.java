// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties for an individual suggestion displayed in the AtMemoryBottomSheetView. */
@NullMarked
public class AtMemoryBottomSheetSuggestionProperties {
    public static final ReadableIntPropertyKey ICON = new ReadableIntPropertyKey();
    public static final ReadableObjectPropertyKey<@Nullable String> TITLE =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<@Nullable String> DETAILS =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> ON_SUGGESTION_CLICKED =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> ON_FLYOUT_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_PROPERTIES = {
        ICON, TITLE, DETAILS, ON_SUGGESTION_CLICKED, ON_FLYOUT_CLICKED,
    };

    private AtMemoryBottomSheetSuggestionProperties() {}
}
