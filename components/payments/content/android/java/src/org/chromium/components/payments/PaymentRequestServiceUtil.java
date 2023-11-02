// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;

/** The utility class for PaymentRequestFactory and WebLayerPaymentRequestFactory. */
public final class PaymentRequestServiceUtil {
    /**
     * Gets the WebContents from a RenderFrameHost if the WebContents has not been destroyed;
     * otherwise, return null.
     * @param renderFrameHost The {@link RenderFrameHost} of any frame in which the intended
     *         WebContents contains.
     * @return The WebContents.
     */
    @Nullable
    public static WebContents getLiveWebContents(RenderFrameHost renderFrameHost) {
        WebContents webContents = WebContentsStatics.fromRenderFrameHost(renderFrameHost);
        return webContents != null && !webContents.isDestroyed() ? webContents : null;
    }

    public static boolean isWebContentsActive(RenderFrameHost renderFrameHost) {
        WebContents webContents = getLiveWebContents(renderFrameHost);
        return webContents != null && webContents.getVisibility() == Visibility.VISIBLE;
    }
}
