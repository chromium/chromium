// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.os.Build;
import android.view.View;

import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.action.ViewActions;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Represents a message banner shown over a PageStation.
 *
 * <p>Subclass for specific messages types to specify expected title, button text and behavior.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public class MessageFacility<HostStationT extends PageStation> extends Facility<HostStationT> {
    public ViewElement<View> bannerElement;
    public ViewElement<View> iconElement;

    public MessageFacility() {
        // Unscoped because other messages can appear and fail the exit condition.
        bannerElement = declareView(withId(R.id.message_banner), ViewElement.unscopedOption());
        iconElement = declareView(withId(R.id.message_icon), ViewElement.unscopedOption());
    }

    /** Dismiss the message banner. */
    public void dismiss() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // b/329707221: For S+, GeneralSwipeAction is not working in the current version of
            // Espresso (3.2). Workaround by using the test helpers to dismiss the popup
            // programmatically.
            mHostStation.exitFacilitySync(
                    this,
                    Transition.runTriggerOnUiThreadOption(),
                    () -> {
                        MessageDispatcher messageDispatcher =
                                MessageDispatcherProvider.from(
                                        mHostStation.getActivity().getWindowAndroid());
                        assert messageDispatcher != null;
                        List<MessageStateHandler> messages =
                                MessagesTestHelper.getEnqueuedMessages(
                                        messageDispatcher, MessageIdentifier.POPUP_BLOCKED);
                        assert !messages.isEmpty();
                        PropertyModel m = MessagesTestHelper.getCurrentMessage(messages.get(0));
                        messageDispatcher.dismissMessage(m, DismissReason.GESTURE);
                    });
        } else {
            mHostStation.exitFacilitySync(
                    this,
                    bannerElement.getPerformTrigger(
                            ViewActions.actionWithAssertions(
                                    new GeneralSwipeAction(
                                            Swipe.FAST,
                                            GeneralLocation.CENTER,
                                            GeneralLocation.TOP_CENTER,
                                            Press.FINGER))));
        }
    }

    /** Declare a ViewElement with the message's |title|. */
    protected ViewElement<View> declareTitleView(String title) {
        return declareView(viewSpec(withId(R.id.message_title), withText(title)));
    }

    /** Declare a ViewElement for the primary button.. */
    protected ViewElement<View> declarePrimaryButtonView(String text) {
        return declareView(viewSpec(withId(R.id.message_primary_button), withText(text)));
    }
}
