// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.test;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

/**
 * A {@link FragmentActivity} for junit tests that gets the necessary themes and theme overlays
 * applied to prevent issues caused by missing styles.
 */
public class BrowserUiDummyFragmentActivity extends FragmentActivity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTheme(R.style.Theme_BrowserUI_DayNight);
    }
}
