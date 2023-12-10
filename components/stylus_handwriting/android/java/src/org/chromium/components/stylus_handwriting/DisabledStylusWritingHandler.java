// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.view.PointerIcon.TYPE_NULL;

import android.content.Context;

import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;

/** A {@link StylusWritingHandler} that represents the feature being disabled. */
public class DisabledStylusWritingHandler implements StylusApiOption {
    @Override
    public void onWebContentsChanged(Context context, WebContents webContents) {
        // Setting the handler to null will turn off the feature.
        webContents.setStylusWritingHandler(null);
    }

    @Override
    public void onWindowFocusChanged(Context context, boolean hasFocus) {}

    @Override
    public int getStylusPointerIcon() {
        return TYPE_NULL;
    }
}
