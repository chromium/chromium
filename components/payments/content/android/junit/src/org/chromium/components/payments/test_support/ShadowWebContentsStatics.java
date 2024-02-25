// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.test_support;

import androidx.annotation.Nullable;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;

/** The shadow of WebContentsStatics. */
@Implements(WebContentsStatics.class)
public class ShadowWebContentsStatics {
    private static WebContents sWebContents;

    /**
     * Sets the WebContents to be returned from {@link #fromRenderFrameHost}.
     * @param webContents The WebContents to be returned.
     */
    public static void setWebContents(WebContents webContents) {
        sWebContents = webContents;
    }

    @Resetter
    public static void reset() {
        sWebContents = null;
    }

    @Implementation
    @Nullable
    public static WebContents fromRenderFrameHost(RenderFrameHost rfh) {
        return sWebContents;
    }
}
