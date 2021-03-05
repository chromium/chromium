// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.user_education.IPHCommandBuilder;

/** An implementation of the {@link ButtonData}. */
public class ButtonDataImpl implements ButtonData {
    private boolean mCanShow;
    private boolean mIsEnabled;

    private ButtonSpec mButtonSpec;

    public ButtonDataImpl(boolean canShow, Drawable drawable, View.OnClickListener onClickListener,
            int contentDescriptionResId, boolean supportsTinting,
            IPHCommandBuilder iphCommandBuilder, boolean isEnabled) {
        mCanShow = canShow;
        mIsEnabled = isEnabled;
        mButtonSpec = new ButtonSpec(drawable, onClickListener, contentDescriptionResId,
                supportsTinting, iphCommandBuilder);
    }

    @Override
    public boolean canShow() {
        return mCanShow;
    }

    @Override
    public boolean isEnabled() {
        return mIsEnabled;
    }

    @Override
    public ButtonSpec getButtonSpec() {
        return mButtonSpec;
    }

    public void setCanShow(boolean canShow) {
        mCanShow = canShow;
    }

    public void setEnabled(boolean enabled) {
        mIsEnabled = enabled;
    }

    public void setButtonSpec(ButtonSpec buttonSpec) {
        mButtonSpec = buttonSpec;
    }
}
