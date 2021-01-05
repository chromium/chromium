// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.user_education.IPHCommandBuilder;

/**
 * Data holder for fields needed to display an optional button in the browsing mode toolbar.
 */
public class ButtonData {
    public boolean canShow;
    public Drawable drawable;
    public View.OnClickListener onClickListener;
    public int contentDescriptionResId;
    public boolean supportsTinting;
    // This controls the enabled state of the button. Disabled buttons are also not clickable.
    public boolean isEnabled;
    /**
     * Builder used to show IPH on the button as it's shown. This should include at a minimum the
     * feature name, content string, and accessibility text, but not the anchor view.
     */
    public IPHCommandBuilder iphCommandBuilder;

    public ButtonData(boolean canShow, Drawable drawable, View.OnClickListener onClickListener,
            int contentDescriptionResId, boolean supportsTinting,
            IPHCommandBuilder iphCommandBuilder, boolean isEnabled) {
        this.canShow = canShow;
        this.drawable = drawable;
        this.onClickListener = onClickListener;
        this.contentDescriptionResId = contentDescriptionResId;
        this.supportsTinting = supportsTinting;
        this.iphCommandBuilder = iphCommandBuilder;
        this.isEnabled = isEnabled;
    }
}
