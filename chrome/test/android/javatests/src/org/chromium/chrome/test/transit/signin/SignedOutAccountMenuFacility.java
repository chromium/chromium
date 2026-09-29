// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.signin;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.page.CtaPageStation;

/**
 * The account menu popup opened by clicking the toolbar sign-in button on desktop, while signed out
 * and with sign-in allowed.
 */
public class SignedOutAccountMenuFacility extends Facility<CtaPageStation> {
    public final ViewElement<View> menuContainerElement;
    public final ViewElement<View> promoSigninButtonElement;

    public SignedOutAccountMenuFacility() {
        super("SignedOutAccountMenuFacility");
        menuContainerElement = declareView(withId(R.id.account_menu_container));
        promoSigninButtonElement =
                declareView(
                        menuContainerElement.descendant(withId(R.id.account_menu_signin_button)));
    }

    /** Dismisses the account menu by pressing back. */
    public void dismissViaBack() {
        pressBackTo().exitFacility();
    }
}
