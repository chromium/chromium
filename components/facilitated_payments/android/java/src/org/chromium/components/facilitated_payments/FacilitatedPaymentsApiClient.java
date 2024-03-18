// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

/**
 * Client for facilitated payment APIs, such as PIX. The default implementation cannot invoke
 * payments. An implementing subclass must provide a factory that builds its instances.
 * Example usage:
 *
 *  FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);
 *  apiClient.isAvailable();
 */
public class FacilitatedPaymentsApiClient {
    private static Factory sFactory;

    /** The delegate to notify of payment result. */
    protected final Delegate mDelegate;

    /**
     * Interface for overriding the type of object that is created by
     * FacilitatedPaymentsApiClient.create().
     * Example usage:
     *
     *  private static final class FactoryImpl implements Factory {
     *      @Override
     *      public FacilitatedPaymentsApiClient factoryCreate(Delegate delegate) {
     *          return new CustomSubclassOfFacilitatedPaymentsApiClient(delegate);
     *      }
     *  }
     *
     *  FacilitatedPaymentsApiClient.setFactory(new FactoryImpl());
     *  FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);
     */
    protected interface Factory {
        /**
         * Builds an instance of facilitated payment API client.
         *
         * @param delegate The delegate to notify of payment result.
         * @return An object that can invoke a facilitated payment API.
         */
        default FacilitatedPaymentsApiClient factoryCreate(Delegate delegate) {
            return null;
        }
    }

    /** The delegate for the facilitated payment API client. */
    public interface Delegate {
        /**
         * Notifies the delegate whether the facilitated payment API is available. If the API is not
         * available, the user should not be prompted with a payment UI.
         *
         * @param isAvailable Whether the facilitated payment API is available.
         */
        default void onIsAvailable(boolean isAvailable) {}

        /**
         * Provides an opaque client token to the delegate, which can use this token for initiating
         * a payment.
         *
         * @param clientToken An opaque client token for initiating a payment. Can be null or empty
         * to indicate a failure.
         */
        default void onGetClientToken(byte[] clientToken) {}

        /**
         * Notifies the delegate whether the facilitated payment was successful.
         *
         * @param isPurchaseActionSuccessful Whether the purchase action was successful.
         */
        default void onPurchaseActionResult(boolean isPurchaseActionSuccessful) {}
    }

    /**
     * Sets the factory that can build instances of facilitated payment API clients.
     *
     * @param factory Can build instances of facilitated payment API clients.
     */
    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    /**
     * Creates an instance of a facilitated payment API client.
     *
     * @param delegate The delegate to notify of payment result.
     * @return An object that can invoke facilitated payment APIs.
     */
    public static FacilitatedPaymentsApiClient create(Delegate delegate) {
        return sFactory != null
                ? sFactory.factoryCreate(delegate)
                : new FacilitatedPaymentsApiClient(delegate);
    }

    /**
     * Constructor for the facilitated payment API client.
     *
     * @param delegate The delegate to notify of payment result.
     */
    protected FacilitatedPaymentsApiClient(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Checks whether this client has the ability to invoke facilitated payment API. Will invoke a
     * delegate callback with the result.
     */
    public void isAvailable() {
        mDelegate.onIsAvailable(/* isAvailable= */ false);
    }

    /**
     * Retrieves the client token for initiating payment. Will invoke a delegate callback with the
     * result.
     */
    public void getClientToken() {
        mDelegate.onGetClientToken(/* clientToken= */ null);
    }

    /**
     * Initiates the payment flow UI. Will invoke a delegate callback with the result.
     *
     * @param actionToken An opaque token used for invoking the purchase action.
     */
    public void invokePurchaseAction(byte[] actionToken) {
        mDelegate.onPurchaseActionResult(/* isPurchaseActionSuccessful= */ false);
    }
}
