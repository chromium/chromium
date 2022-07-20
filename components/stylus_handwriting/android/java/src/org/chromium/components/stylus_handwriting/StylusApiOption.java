// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;

import org.chromium.content_public.browser.WebContents;

/**
 * This interface allows {@link StylusWritingController} to abstract over
 * {@link AndroidStylusWritingHandler}, {@link DirectWritingTrigger} and
 * {@link DisabledStylusWritingHandler}.
 *
 * We can't just add the methods here to
 * {@link org.chromium.content_public.browser.StylusWritingHandler}, because content_public should
 * only contain functionality calling between the contents and the embedder.
 */
public interface StylusApiOption {
    void onWebContentsChanged(Context context, WebContents webContents);

    default void onWindowFocusChanged(Context context, boolean hasFocus) {}
}
