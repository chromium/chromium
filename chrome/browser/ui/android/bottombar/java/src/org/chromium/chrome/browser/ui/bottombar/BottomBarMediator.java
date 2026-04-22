// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bottom bar */
@NullMarked
public class BottomBarMediator {
    private final PropertyModel mModel;
    private final ThemeColorProvider mThemeColorProvider;

    /**
     * @param model The property model to update.
     * @param themeColorProvider The provider to observe theme changes from.
     */
    public BottomBarMediator(PropertyModel model, ThemeColorProvider themeColorProvider) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;

        mModel.set(BottomBarProperties.COLOR_SCHEME, mThemeColorProvider.getBrandedColorScheme());
    }

    /** Remove observers. */
    public void destroy() {}
}
