// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A delegate that provides the button that triggers the app menu. */
@NullMarked
public interface MenuButtonDelegate {
    /**
     * @return The {@link View} for the menu button, used to anchor the app menu.
     */
    @Nullable
    View getMenuButtonView();
}
