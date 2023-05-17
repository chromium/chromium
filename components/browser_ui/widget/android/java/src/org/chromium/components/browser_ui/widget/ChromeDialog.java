// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.LinearLayout;

import androidx.activity.ComponentDialog;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.StyleRes;
import androidx.appcompat.widget.Toolbar;

import org.chromium.base.BuildInfo;

/**
 * Dialog class in Chrome
 *
 * This class will automatically add the back button toolbar to automotive devices in full screen
 * Dialogs.
 */
public class ChromeDialog extends ComponentDialog {
    private boolean mIsFullScreen;

    public ChromeDialog(@NonNull Context context, @StyleRes int themeResId) {
        super(context, themeResId);
        if (themeResId == R.style.ThemeOverlay_BrowserUI_Fullscreen) {
            mIsFullScreen = true;
        } else {
            mIsFullScreen = false;
        }
    }

    @Override
    public void setContentView(@LayoutRes int layoutResID) {
        if (BuildInfo.getInstance().isAutomotive && mIsFullScreen) {
            super.setContentView(R.layout.automotive_layout_with_back_button_toolbar);
            setAutomotiveToolbarBackButtonAction();
            ViewStub stub = findViewById(R.id.original_layout);
            stub.setLayoutResource(layoutResID);
            stub.inflate();
        } else {
            super.setContentView(layoutResID);
        }
    }

    @Override
    public void setContentView(View view) {
        if (BuildInfo.getInstance().isAutomotive && mIsFullScreen) {
            super.setContentView(R.layout.automotive_layout_with_back_button_toolbar);
            setAutomotiveToolbarBackButtonAction();
            LinearLayout linearLayout = findViewById(R.id.automotive_base_linear_layout);
            linearLayout.addView(view);
        } else {
            super.setContentView(view);
        }
    }

    @Override
    public void setContentView(View view, ViewGroup.LayoutParams params) {
        if (BuildInfo.getInstance().isAutomotive && mIsFullScreen) {
            super.setContentView(R.layout.automotive_layout_with_back_button_toolbar);
            setAutomotiveToolbarBackButtonAction();
            LinearLayout linearLayout = findViewById(R.id.automotive_base_linear_layout);
            linearLayout.setLayoutParams(params);
            linearLayout.addView(view);
        } else {
            super.setContentView(view, params);
        }
    }

    private void setAutomotiveToolbarBackButtonAction() {
        Toolbar backButtonToolbarForAutomotive = findViewById(R.id.back_button_toolbar);
        if (backButtonToolbarForAutomotive != null) {
            backButtonToolbarForAutomotive.setNavigationOnClickListener(
                    backButtonClick -> { getOnBackPressedDispatcher().onBackPressed(); });
        }
    }
}
