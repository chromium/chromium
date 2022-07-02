// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.WebContents;

/**
 * Java representation of the native PageLoadTriggerContext.
 */
public class PageLoadTriggerContext extends TriggerContext {
    public final WebContents webContents;

    @CalledByNative
    private static PageLoadTriggerContext createPageLoadTriggerContext(
            @Nullable WebContents webContents) {
        return new PageLoadTriggerContext(webContents);
    }

    /** Constructor. */
    public PageLoadTriggerContext(@Nullable WebContents webContents) {
        this.webContents = webContents;
    }
}
