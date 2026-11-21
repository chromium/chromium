// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.split_view;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** The sole authority that decides whether to enable Android Split View. */
@NullMarked
public final class SplitViewEnabledFn {
    private SplitViewEnabledFn() {}

    /** Whether Android Split View should be enabled. */
    public static boolean isEnabled() {
        return ChromeFeatureList.sEnableAndroidSplitView.isEnabled();
    }
}
