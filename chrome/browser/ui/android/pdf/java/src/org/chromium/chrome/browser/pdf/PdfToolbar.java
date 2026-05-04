// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.Context;
import android.util.AttributeSet;

import androidx.appcompat.widget.Toolbar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Toolbar for the PDF viewer. To handle clicks on the navigation icon, set a listener with
 * {@link #setNavigationOnClickListener(OnClickListener)}.
 */
@NullMarked
public class PdfToolbar extends Toolbar {
    public PdfToolbar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }
}
