// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ssl;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.widget.TextView;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.ui.ModalDialogFacility;

/** The Ask-before-HTTP (HTTPS-First Mode) fallback dialog. */
public class AskBeforeHttpDialogFacility extends ModalDialogFacility {
    public ViewElement<TextView> descriptionElement;

    public AskBeforeHttpDialogFacility() {
        super();

        // The title of the Ask-before-HTTP dialog.
        declareView(
                dialogElement.descendant(
                        TextView.class, withText("This site doesn’t support a secure connection")));

        // The description text containing the Learn More link.
        descriptionElement =
                declareView(
                        dialogElement.descendant(
                                TextView.class,
                                withText(
                                        org.hamcrest.Matchers.containsString(
                                                "Learn more about this warning"))));

        // The "Continue to site" button is the negative button.
        declareNegativeButton("Continue to site");

        // The "Go back" button is the positive button.
        declarePositiveButton("Go back");
    }

    /** Click the "Learn more" link. */
    public WebPageStation clickLearnMore(BasePageStation<?> currentStation) {
        return descriptionElement
                .performViewActionTo(org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan(0))
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initOpeningNewTab()
                                .withExpectedUrlSubstring("p=first_mode")
                                .build());
    }

    /** Click the "Go back" button. */
    public WebPageStation clickGoBack(BasePageStation<?> currentStation, String destinationUrl) {
        return positiveButtonElement
                .clickTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initForLoadingUrlOnSameTab(destinationUrl, currentStation)
                                .build());
    }

    /** Click the "Continue to site" button. */
    public WebPageStation clickContinue(BasePageStation<?> currentStation, String destinationUrl) {
        return negativeButtonElement
                .clickTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initForLoadingUrlOnSameTab(destinationUrl, currentStation)
                                .build());
    }

    /** Press back to dismiss the dialog and go back. */
    public WebPageStation pressBack(BasePageStation<?> currentStation, String destinationUrl) {
        return pressBackTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initForLoadingUrlOnSameTab(destinationUrl, currentStation)
                                .build());
    }
}
