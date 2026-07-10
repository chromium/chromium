// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Custom view for the web content hairline container in Side UI. */
@NullMarked
@DoNotMock
/* package */ final class SideUiWebContentHairlineContainer extends FrameLayout {
    /** Constructor for inflating from XML. */
    public SideUiWebContentHairlineContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Returns the top hairline ImageView. */
    /* package */ ImageView getTopHairline() {
        return findViewById(R.id.top_web_content_hairline);
    }

    /** Returns the left hairline ImageView. */
    /* package */ ImageView getLeftHairline() {
        return findViewById(R.id.left_web_content_hairline);
    }

    /** Returns the right hairline ImageView. */
    /* package */ ImageView getRightHairline() {
        return findViewById(R.id.right_web_content_hairline);
    }

    /** Returns the left rounded corner ImageView. */
    /* package */ ImageView getLeftRoundedCorner() {
        return findViewById(R.id.left_web_content_rounded_corner);
    }

    /** Returns the right rounded corner ImageView. */
    /* package */ ImageView getRightRoundedCorner() {
        return findViewById(R.id.right_web_content_rounded_corner);
    }
}
