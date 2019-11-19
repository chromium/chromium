// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.ui;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;

/** Dummy activity to test UI components without Chrome browser initialization and natives. */
public class DummyUiActivity extends AppCompatActivity {
    private static int sTestTheme;
    private static int sTestLayout;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (sTestTheme != 0) {
            setTheme(sTestTheme);
        }
        if (sTestLayout != 0) {
            setContentView(sTestLayout);
        }
    }

    /**
     * Set the base theme for the dummy activity. Note that you can also call mActivity.setTheme()
     * in test code later if you want to set theme after activity launched.
     * @param resid The style resource describing the theme.
     */
    public static void setTestTheme(@IdRes int resid) {
        sTestTheme = resid;
    }

    /**
     * Set the activity content from a layout resource. Note that you can also call
     * mActivity.setContentView() in test code later if you want to set content view after activity
     * launched.
     * @param layoutResID Resource ID to be inflated.
     */
    public static void setTestLayout(@LayoutRes int layoutResID) {
        sTestLayout = layoutResID;
    }
}
