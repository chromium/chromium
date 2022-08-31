// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.autofill.VirtualCardEnrollmentState;

/** A collection of helper methods for FastCheckout tests. */
class FastCheckoutTestUtils {
    /** Creates a detailed {@link FastCheckoutAutofillProfile}. */
    static FastCheckoutAutofillProfile createDetailedProfile(String name, String streetAddress,
            String city, String postalCode, String email, String phoneNumber) {
        return new FastCheckoutAutofillProfile(/* guid= */ "", /* origin= */ "",
                /* isLocal= */ true, /* honorificPrefix= */ "", name,
                /* companyName= */ "", /* streetAddress= */ streetAddress,
                /* region= */ "", /* locality= */ "", /* dependentLocality= */ "",
                /* postalCode= */ postalCode, /* sortingCode= */ "",
                /* countryCode= */ "", /* countryName= */ "", /* phoneNumber= */ "", email,
                /* languageCode= */ "en-US");
    }

    /** Creates a simple {@link FastCheckoutAutofillProfile}.  */
    static FastCheckoutAutofillProfile createDummyProfile(String name, String email) {
        return createDetailedProfile(name, /*streetAddress=*/"", /*city=*/"", /*postalCode=*/"",
                email, /*phoneNumber=*/"");
    }

    /** Creates a simple {@link FastCheckoutCreditCard}.  */
    static FastCheckoutCreditCard createDummyCreditCard(String origin, String number) {
        return new FastCheckoutCreditCard(/* guid= */ "john", origin, /* isLocal= */ true,
                /* isCached= */ true, "John Doe", number, "1111", "12", "2050", "visa",
                /* billingAddressId= */ "", /* billingAddressId= */ "john",
                /* serverId= */ "", /* instrumentId= */ 0, /* nickname= */ "",
                /* cardArtUrl= */ null,
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                /* productDescription= */ "");
    }
}
