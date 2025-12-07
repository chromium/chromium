// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ui;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewElementMatchesCondition;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;

/** Facility for {@link ModalDialogView}. */
public class ModalDialogFacility extends Facility<Station<ChromeTabbedActivity>> {
    public ViewElement<ModalDialogView> dialogElement;
    public ViewElement<TextView> titleElement;
    public ViewElement<View> customViewElement;
    public ViewElement<Button> positiveButtonElement;
    public ViewElement<Button> negativeButtonElement;

    public ModalDialogFacility() {
        dialogElement = declareView(ModalDialogView.class, withId(R.id.modal_dialog_view));
    }

    /** Declare a ViewElement for the title of the dialog, expecting it to show |text|. */
    public void declareTitle(String text) {
        declareTitle();
        declareEnterCondition(new ViewElementMatchesCondition(titleElement, withText(text)));
    }

    /** Declare a ViewElement for the title of the dialog, expecting no particular title. */
    public void declareTitle() {
        titleElement = declareView(dialogElement.descendant(TextView.class, withId(R.id.title)));
    }

    /** Declare a ViewElement for the custom view outside the scrollable area of the dialog. */
    public void declareCustomView() {
        customViewElement =
                declareView(dialogElement.descendant(withId(R.id.custom_view_not_in_scrollable)));
    }

    /** Declare a ViewElement for the negative button, expecting it to show |text|. */
    public void declareNegativeButton(String text) {
        declareNegativeButton();
        declareEnterCondition(
                new ViewElementMatchesCondition(negativeButtonElement, withText(text)));
    }

    /** Declare a ViewElement for the negative button, expecting no particular text. */
    public void declareNegativeButton() {
        negativeButtonElement =
                declareView(dialogElement.descendant(Button.class, withId(R.id.negative_button)));
    }

    /** Declare a ViewElement for the positive button, expecting it to show |text|. */
    public void declarePositiveButton(String text) {
        declarePositiveButton();
        declareEnterCondition(
                new ViewElementMatchesCondition(positiveButtonElement, withText(text)));
    }

    /** Declare a ViewElement for the positive button, expecting no particular text. */
    public void declarePositiveButton() {
        positiveButtonElement =
                declareView(dialogElement.descendant(Button.class, withId(R.id.positive_button)));
    }

    /** Click Cancel to close the dialog with no action. */
    public void clickCancel() {
        negativeButtonElement.clickTo().exitFacility();
    }

    /** Press the back button to dismiss the dialog. */
    public void pressBackToDismiss() {
        pressBackTo().exitFacility();
    }
}
