// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

import org.chromium.payments.mojom.GooglePaymentMethodData;
import org.chromium.payments.mojom.PaymentAddress;
import org.chromium.payments.mojom.PaymentDetails;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentResponse;

/**
 * A helper to manage the request / response patching for the Skip-to-GPay experimental flow.
 * TODO(crbug.com/984694): Retire this helper after general delegation of shipping and contact
 * information is available.
 */
public class SkipToGPayHelper {
    /** Copied from PaymentOptions from PaymentRequest constructor. */
    private final boolean mPaymentOptionsRequestPayerPhone;
    private final boolean mPaymentOptionsRequestPayerName;
    private final boolean mPaymentOptionsRequestPayerEmail;
    private final boolean mPaymentOptionsRequestShipping;

    /**
     * These flags record the additional fields that are set in GPay request that must be redacted
     * from the response before returning it to the renderer.
     */
    private final boolean mPhoneRequested;
    private final boolean mNameRequested;
    private final boolean mEmailRequested;
    private final boolean mShippingRequested;

    /**
     * The selected shipping option ID. This is saved at the beginning of request processing because
     * the skip-to-GPay flow does not run the standard shipping option selection algorithm.
     */
    private String mSelectedShippingOptionId;

    /**
     * Constructs a SkipToGPayHeper.
     * @param options The PaymentOptions specified by the merchant when constructing the
     * PaymentRequest. The |request*| fields indicate which shipping or contact information the
     * merchant requested initially.
     * @param gpayData Metadata from the renderer indicating which fields are overridden in GPay
     * request to coax GPay into sending shipping and contact information.
     */
    public SkipToGPayHelper(PaymentOptions options, GooglePaymentMethodData gpayData) {
        mPaymentOptionsRequestPayerPhone = options != null && options.requestPayerPhone;
        mPaymentOptionsRequestPayerName = options != null && options.requestPayerName;
        mPaymentOptionsRequestPayerEmail = options != null && options.requestPayerEmail;
        mPaymentOptionsRequestShipping = options != null && options.requestShipping;

        mPhoneRequested = gpayData.phoneRequested;
        mNameRequested = gpayData.nameRequested;
        mEmailRequested = gpayData.emailRequested;
        mShippingRequested = gpayData.shippingRequested;
    }

    /**
     * Save default shipping option if shipping is requested. This is used to set shipping option in
     * PaymentResponse because the usual shipping option selection logic is not run in skip-UI flow.
     * @return True if a shipping is requested and a single pre-selected shipping option exists.
     * False otherwise.
     */
    public boolean setShippingOptionIfValid(PaymentDetails details) {
        if (!mPaymentOptionsRequestShipping) return true;
        if (details.shippingOptions == null || details.shippingOptions.length != 1
                || !details.shippingOptions[0].selected) {
            return false;
        }

        mSelectedShippingOptionId = details.shippingOptions[0].id;
        return true;
    }

    /**
     * Copies shipping and contact information from GPay details to PaymentResponse. Redact the
     * fields from GPay details if they were not originally requested by the merchant.
     *
     * @param response The PaymentResponse to patch. It is assumed to have come from Google Pay
     * native Android app.
     * @return True if the patch is successful, false if any error occurred.
     */
    public boolean patchPaymentResponse(PaymentResponse response) {
        assert response.methodName.equals(MethodStrings.GOOGLE_PAY);

        // If GPay fails to return data, pass through without handling.
        if (response.stringifiedDetails.isEmpty()) return true;

        try {
            JSONObject object = new JSONObject(new JSONTokener(response.stringifiedDetails));

            if (mPaymentOptionsRequestPayerEmail && object.has("email")) {
                response.payer.email = object.getString("email");
                if (mEmailRequested) object.remove("email");
            }

            if (mPaymentOptionsRequestShipping) {
                response.shippingOption = mSelectedShippingOptionId;

                JSONObject shippingAddress = object.optJSONObject("shippingAddress");
                if (shippingAddress != null) {
                    response.shippingAddress = new PaymentAddress();
                    response.shippingAddress.country = shippingAddress.optString("countryCode");
                    response.shippingAddress.region =
                            shippingAddress.optString("administrativeArea");
                    response.shippingAddress.city = shippingAddress.optString("locality");
                    response.shippingAddress.postalCode = shippingAddress.optString("postalCode");
                    response.shippingAddress.sortingCode = shippingAddress.optString("sortingCode");
                    response.shippingAddress.recipient = shippingAddress.optString("name");
                    // GPay API does not have the concepts of dependentLocality or organization.
                    // See https://developers.google.com/pay/api/web/reference/object#Address
                    response.shippingAddress.dependentLocality = "";
                    response.shippingAddress.organization = "";
                    // GPay does not return shipping address phone number by default. We chose not
                    // to request it because Payment Request API only allows merchant to request
                    // payer phone, but the shipping address may be for a different person. So we
                    // do not want to expose this information to the merchant since they could not
                    // have explicitly requested it.
                    response.shippingAddress.phone = "";

                    String address1 = shippingAddress.optString("address1");
                    String address2 = shippingAddress.optString("address2");
                    String address3 = shippingAddress.optString("address3");
                    int numAddressLines = (address1.isEmpty() ? 0 : 1)
                            + (address2.isEmpty() ? 0 : 1) + (address3.isEmpty() ? 0 : 1);
                    response.shippingAddress.addressLine = new String[numAddressLines];
                    if (numAddressLines > 0) {
                        int index = 0;
                        if (!address1.isEmpty()) {
                            response.shippingAddress.addressLine[index++] = address1;
                        }
                        if (!address2.isEmpty()) {
                            response.shippingAddress.addressLine[index++] = address2;
                        }
                        if (!address3.isEmpty()) {
                            response.shippingAddress.addressLine[index++] = address3;
                        }
                    }
                }
                if (mShippingRequested) {
                    object.remove("shippingAddress");
                }
            }

            // optInt() returns 0 if the "apiVersion" key does not exist.
            if (object.optInt("apiVersion") < 2) {
                extractDataFromGPayV1Response(object, response);
            } else {
                extractDataFromGPayV2Response(object, response);
            }

            response.stringifiedDetails = object.toString();
        } catch (JSONException e) {
            return false;
        }

        return true;
    }

    private void extractDataFromGPayV1Response(JSONObject object, PaymentResponse response) {
        JSONObject cardInfo = object == null ? null : object.optJSONObject("cardInfo");
        extractDataFromGPayCardInfo(cardInfo, response);
    }

    private void extractDataFromGPayV2Response(JSONObject object, PaymentResponse response) {
        JSONObject paymentMethodData = object.optJSONObject("paymentMethodData");
        JSONObject cardInfo =
                paymentMethodData == null ? null : paymentMethodData.optJSONObject("info");
        extractDataFromGPayCardInfo(cardInfo, response);
    }

    private void extractDataFromGPayCardInfo(JSONObject cardInfo, PaymentResponse response) {
        JSONObject billingAddress =
                cardInfo == null ? null : cardInfo.optJSONObject("billingAddress");
        if (billingAddress != null) {
            if (mPaymentOptionsRequestPayerName) {
                response.payer.name = billingAddress.optString("name");
            }
            if (mPaymentOptionsRequestPayerPhone) {
                response.payer.phone = billingAddress.optString("phoneNumber");
            }

            // GPay API only allows phone number to be requested as part of billing address. If the
            // billing address request comes from the renderer not the merchant (i.e.
            // |mNameRequested| is true), then we should redact the entire billing address. If the
            // billing address request comes from the merchant and the renderer only additionally
            // requested phone (i.e. !mNameRequested && mPhoneRequested), then only redact the phone
            // number.
            if (mNameRequested) {
                cardInfo.remove("billingAddress");
            } else if (mPhoneRequested) {
                billingAddress.remove("phoneNumber");
            }
        }
    }
}
