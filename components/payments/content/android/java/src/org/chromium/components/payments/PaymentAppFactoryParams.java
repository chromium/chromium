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
    /** Returns the {@link WebContents} where the payment is being requested. */
    WebContents getWebContents();

    /** Returns the {@link RenderFrameHost} for the frame that initiates the payment request. */
    RenderFrameHost getRenderFrameHost();

    /** Returns the PaymentRequest object identifier. */
    default @Nullable String getId() {
        return null;
    }

    /**
     * Returns the scheme, host, and port of the last committed URL of the top-level context as
     * formatted by UrlFormatter.formatUrlForSecurityDisplay().
     */
    default String getTopLevelOrigin() {
        throw new UnsupportedOperationException();
    }

    /**
     * Returns the scheme, host, and port of the last committed URL of the iframe that invoked the
     * PaymentRequest API as formatted by UrlFormatter.formatUrlForSecurityDisplay().
     */
    default String getPaymentRequestOrigin() {
        throw new UnsupportedOperationException();
    }

    /**
     * Returns the origin of the iframe that invoked the PaymentRequest API. Can be opaque. Used by
     * security features like 'Sec-Fetch-Site' and 'Cross-Origin-Resource-Policy'. Should not be
     * null.
     */
    default Origin getPaymentRequestSecurityOrigin() {
        throw new UnsupportedOperationException();
    }

    /**
     * Returns the certificate chain of the top-level context as returned by
     * CertificateChainHelper.getCertificateChain(). Can be null when
     * ANDROID_PAYMENT_INTENTS_OMIT_DEPRECATED_PARAMETERS is enabled or for localhost or local file,
     * which are secure contexts without SSL. Each byte array cannot be null.
     */
    default byte @Nullable [][] getCertificateChain() {
        return null;
    }

    /**
     * Returns whether crawling the web for just-in-time installable payment handlers is enabled.
     */
    default boolean getMayCrawl() {
        return false;
    }

    /**
     * Returns the listener for payment method, shipping address, and shipping option change events.
     */
    default @Nullable PaymentRequestUpdateEventListener getPaymentRequestUpdateEventListener() {
        return null;
    }

    /** Returns the Payment Request information received from the merchant. */
    default PaymentRequestSpec getSpec() {
        throw new UnsupportedOperationException();
    }

    /**
     * Returns the Android package name of the Trusted Web Activity that invoked Chrome, if running
     * in TWA mode. Otherwise null or empty string.
     */
    default @Nullable String getTwaPackageName() {
        return null;
    }

    /**
     * Returns whether the merchant {@link WebContents}'s profile is in off-the-record mode. Returns
     * true if the tab profile is not accessible from the {@link WebContents}.
     */
    boolean isOffTheRecord();

    /** Returns whether the "can make payment" preference is enabled. */
    boolean prefsCanMakePayment();

    /** Returns the Content-Security-Policy (CSP) checker. */
    CSPChecker getCSPChecker();

    /** Returns an instance of a dialog for displaying informational or warning messages. */
    default @Nullable DialogController getDialogController() {
        return null;
    }

    /** Returns the launcher for Android intent-based payment app. */
    default @Nullable AndroidIntentLauncher getAndroidIntentLauncher() {
        return null;
    }

    /**
     * Used to check whether payment apps are required to handle shipping address and contact
     * information, when merchant websites request that information. This information can be
     * returned either from payment apps or from Chrome's autofill. Result of this method does not
     * guarantee the payment. Even if this method returns true, there could be no payment apps to
     * support providing shipping address or contact information.
     *
     * @return Whether payment apps are required to provide shipping address and contact
     *     information.
     */
    default boolean isFullDelegationRequired() {
        return false;
    }
}
