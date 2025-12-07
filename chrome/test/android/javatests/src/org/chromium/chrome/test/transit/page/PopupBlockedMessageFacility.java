// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import android.view.View;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.transit.ui.MessageFacility;

/**
 * Represents a "Pop-up blocked" message.
 *
 * @param <HostStationT> the type of host {@link WebPageStation} this is scoped to.
 */
public class PopupBlockedMessageFacility<HostStationT extends WebPageStation>
        extends MessageFacility<HostStationT> {
    public ViewElement<View> titleElement;
    public ViewElement<View> alwaysShowButtonElement;

    public PopupBlockedMessageFacility(int count) {
        String title;
        if (count == 1) {
            title = "Pop-up blocked";
        } else {
            title = String.format("%s pop-ups blocked", count);
        }
        titleElement = declareTitleView(title);
        alwaysShowButtonElement = declarePrimaryButtonView("Always show");
    }

    public WebPageStation clickAlwaysAllow() {
        return alwaysShowButtonElement
                .clickTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initFrom(mHostStation)
                                .initOpeningNewTab()
                                .build());
    }
}
