// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.Origin;

/** Interface for providing information to a payment app factory. */
@NullMarked
public interface PaymentAppFactoryParams extends PaymentRequestParams {
    /** @return The web contents where the payment is being requested. */
    WebContents getWebContents();

    /** @return The RenderFrameHost for the frame that initiates the payment request. */
    RenderFrameHost getRenderFrameHost();

    /** @return The PaymentRequest object identifier. */
    default @Nullable String getId() {
        return null;
    }

    /**
     * @return The scheme, host, and port of the last committed URL of the top-level context as
     * formatted by UrlFormatter.formatUrlForSecurityDisplay().
     */
    default String getTopLevelOrigin() {
        throw new UnsupportedOperationException();
    }

    /**
     * @return The scheme, host, and port of the last committed URL of the iframe that invoked the
     * PaymentRequest API as formatted by UrlFormatter.formatUrlForSecurityDisplay().
     */
    default String getPaymentRequestOrigin() {
        throw new UnsupportedOperationException();
    }

    /**
     * @return The origin of the iframe that invoked the PaymentRequest API. Can be opaque. Used by
     * security features like 'Sec-Fetch-Site' and 'Cross-Origin-Resource-Policy'. Should not be
     * null.
     */
    default Origin getPaymentRequestSecurityOrigin() {
        throw new UnsupportedOperationException();
    }

    /**
     * @return The certificate chain of the top-level context as returned by
     * CertificateChainHelper.getCertificateChain(). Can be null when
     * ANDROID_PAYMENT_INTENTS_OMIT_DEPRECATED_PARAMETERS is enabled or for localhost or local file,
     * which are secure contexts without SSL. Each byte array cannot be null.
     */
    default byte @Nullable [][] getCertificateChain() {
        return null;
    }

    /** @return Whether crawling the web for just-in-time installable payment handlers is enabled. */
    default boolean getMayCrawl() {
        return false;
    }

    /**
     * @return The listener for payment method, shipping address, and shipping option change events.
     */
    default @Nullable PaymentRequestUpdateEventListener getPaymentRequestUpdateEventListener() {
        return null;
    }

    /**
     * @return The Payment Request information received from the merchant.
     */
    default PaymentRequestSpec getSpec() {
        throw new UnsupportedOperationException();
    }

    /**
     * @return The Android package name of the Trusted Web Activity that invoked Chrome, if running
     * in TWA mode. Otherwise null or empty string.
     */
    default @Nullable String getTwaPackageName() {
        return null;
    }

    /**
     * @return Whether the merchant WebContents's profile is in off-the-record mode. Return true
     *         if the tab profile is not accessible from the WebContents.
     */
    boolean isOffTheRecord();
}
