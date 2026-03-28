// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel.test;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** Supports {@code side_panel_coordinator_android_browsertest.cc}. */
@NullMarked
public final class SidePanelCoordinatorAndroidBrowserTestSupport {

    private SidePanelCoordinatorAndroidBrowserTestSupport() {}

    @CalledByNative
    @SuppressLint("SetTextI18n")
    private static View createTestView(WindowAndroid windowAndroid) {
        Context context = windowAndroid.getContext().get();
        assert context != null : "null context in WindowAndroid";

        TextView view = new TextView(context);
        view.setText("Test Side Panel View");
        view.setBackgroundColor(Color.GREEN);
        view.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        view.setGravity(Gravity.CENTER);

        return view;
    }
}
