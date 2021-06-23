// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.theme;

import android.app.Activity;

/** Simple interface for exposing downstream functionality around handling colors. */
public interface ColorDelegate {
    /**
     * Calls {link @Activity#setTheme(int)} if dynamic colors are available.
     * @param activity The activity to add dynamic colors to.
     */
    void applyDynamicColorsIfAvailable(Activity activity);
}