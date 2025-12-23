// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ui;

import static org.chromium.base.test.transit.Condition.notFulfilled;
import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.PayloadCallbackCondition;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Station;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.widget.AnchoredPopupWindow;

/**
 * Facility for a list menu that appears upon long press or right click.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public abstract class ListMenuFacility<HostStationT extends Station<?>>
        extends ScrollableFacility<HostStationT> {
    private final PayloadCallbackCondition<AnchoredPopupWindow> mPopupWindowCallbackCondition;
    public final Element<AnchoredPopupWindow> popupWindowElement;

    /** Constructor. Expects a specific title and selected color. */
    public ListMenuFacility() {
        mPopupWindowCallbackCondition = new PayloadCallbackCondition<>("Popup Window");
        popupWindowElement = declareEnterConditionAsElement(mPopupWindowCallbackCondition);
        declareEnterCondition(
                uiThreadCondition(
                        "Menu is showing",
                        mPopupWindowCallbackCondition,
                        m -> {
                            if (m == null) {
                                return notFulfilled("Menu is null");
                            }
                            return whether(m.isShowing());
                        }));
    }

    @Override
    protected void onTransitionToStarted() {
        super.onTransitionToStarted();
        ListMenuHost.setMenuChangedListenerForTesting(
                menu -> {
                    mPopupWindowCallbackCondition.notifyCalled(menu);
                    return menu;
                });
    }

    @Override
    protected void onTransitionToFinished() {
        super.onTransitionToFinished();
        ListMenuHost.setMenuChangedListenerForTesting(null);
    }
}
