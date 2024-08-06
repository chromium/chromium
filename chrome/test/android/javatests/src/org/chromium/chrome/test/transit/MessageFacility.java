// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.os.Build;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.action.ViewActions;

import org.hamcrest.CoreMatchers;
import org.hamcrest.Matcher;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
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
    public static final Matcher<View> MESSAGE_TITLE_MATCHER = withId(R.id.message_title);
    public static final Matcher<View> MESSAGE_PRIMARY_BUTTON_MATCHER =
            withId(R.id.message_primary_button);

    public static final ViewSpec MESSAGE_BANNER = viewSpec(withId(R.id.message_banner));
    public static final ViewSpec MESSAGE_ICON = viewSpec(withId(R.id.message_icon));

    @CallSuper
    @Override
    public void declareElements(Elements.Builder elements) {
        // Unscoped because other messages can appear and fail the exit condition.
        elements.declareView(MESSAGE_BANNER, ViewElement.unscopedOption());
        elements.declareView(MESSAGE_ICON, ViewElement.unscopedOption());
    }

    /** Dismiss the message banner. */
    public void dismiss() {
        Trigger dismissTrigger;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // b/329707221: For S+, GeneralSwipeAction is not working in the current version of
            // Espresso (3.2). Workaround by using the test helpers to dismiss the popup
            // programmatically.
            dismissTrigger =
                    () ->
                            ThreadUtils.runOnUiThreadBlocking(
                                    () -> {
                                        MessageDispatcher messageDispatcher =
                                                MessageDispatcherProvider.from(
                                                        mHostStation
                                                                .getActivity()
                                                                .getWindowAndroid());
                                        assert messageDispatcher != null;
                                        List<MessageStateHandler> messages =
                                                MessagesTestHelper.getEnqueuedMessages(
                                                        messageDispatcher,
                                                        MessageIdentifier.POPUP_BLOCKED);
                                        assert !messages.isEmpty();
                                        PropertyModel m =
                                                MessagesTestHelper.getCurrentMessage(
                                                        messages.get(0));
                                        messageDispatcher.dismissMessage(m, DismissReason.GESTURE);
                                    });
        } else {
            dismissTrigger =
                    () ->
                            MESSAGE_BANNER.perform(
                                    ViewActions.actionWithAssertions(
                                            new GeneralSwipeAction(
                                                    Swipe.FAST,
                                                    GeneralLocation.CENTER,
                                                    GeneralLocation.TOP_CENTER,
                                                    Press.FINGER)));
        }
        mHostStation.exitFacilitySync(this, dismissTrigger);
    }

    /** Create a {@link ViewSpec} expecting the message's |title|. */
    protected static ViewSpec titleViewSpec(String title) {
        Matcher<View> viewMatcher = CoreMatchers.allOf(MESSAGE_TITLE_MATCHER, withText(title));
        return viewSpec(viewMatcher);
    }

    /** Create a {@link ViewSpec} expecting the message's primary button with |text|. */
    protected static ViewSpec primaryButtonViewSpec(String text) {
        Matcher<View> viewMatcher =
                CoreMatchers.allOf(MESSAGE_PRIMARY_BUTTON_MATCHER, withText(text));
        return viewSpec(viewMatcher);
    }
}
