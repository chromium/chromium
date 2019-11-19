// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.contextmenu;

import android.app.Activity;
import android.app.Instrumentation;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.contextmenu.ContextMenuHelper;
import org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * A utility class to help open and interact with the Revamped Context Menu.
 */
public class RevampedContextMenuUtils {
    /**
     * Callback helper that also provides access to the last displayed Revamped Context Menu.
     */
    private static class OnContextMenuShownHelper extends CallbackHelper {
        private RevampedContextMenuCoordinator mCoordinator;

        public void notifyCalled(RevampedContextMenuCoordinator coordinator) {
            mCoordinator = coordinator;
            notifyCalled();
        }

        RevampedContextMenuCoordinator getRevampedContextMenuCoordinator() {
            assert getCallCount() > 0;
            return mCoordinator;
        }
    }

    /**
     * Opens a context menu.
     * @param tab                   The tab to open a context menu for.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @return                      The {@link RevampedContextMenuCoordinator} of the context menu.
     * @throws TimeoutException
     */
    public static RevampedContextMenuCoordinator openContextMenu(Tab tab, String openerDOMNodeId)
            throws TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        return openContextMenuByJs(tab, jsCode);
    }

    /**
     * Opens a context menu.
     * @param tab                   The tab to open a context menu for.
     * @param jsCode                The javascript to get the DOM node to long press to
     *                              open the context menu for.
     * @return                      The {@link RevampedContextMenuCoordinator} of the context menu.
     * @throws TimeoutException
     */
    private static RevampedContextMenuCoordinator openContextMenuByJs(Tab tab, String jsCode)
            throws TimeoutException {
        final OnContextMenuShownHelper helper = new OnContextMenuShownHelper();
        ContextMenuHelper.sRevampedContextMenuShownCallback = ((coordinator) -> {
            helper.notifyCalled(coordinator);
            ContextMenuHelper.sRevampedContextMenuShownCallback = null;
        });

        int callCount = helper.getCallCount();
        DOMUtils.longPressNodeByJs(tab.getWebContents(), jsCode);

        helper.waitForCallback(callCount);
        return helper.getRevampedContextMenuCoordinator();
    }

    /**
     * Opens and selects an item from a context menu.
     * @param tab                   The tab to open a context menu for.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @param itemId                The context menu item ID to select.
     * @param activity              The activity to assert for gaining focus after click or null.
     * @throws TimeoutException
     */
    public static void selectContextMenuItem(Instrumentation instrumentation, Activity activity,
            Tab tab, String openerDOMNodeId, final int itemId) throws TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        selectContextMenuItemByJs(instrumentation, activity, tab, jsCode, itemId);
    }

    /**
     * Long presses to open and selects an item from a context menu.
     * @param tab                   The tab to open a context menu for.
     * @param jsCode                The javascript to get the DOM node to long press
     *                              to open the context menu for.
     * @param itemId                The context menu item ID to select.
     * @param activity              The activity to assert for gaining focus after click or null.
     * @throws TimeoutException
     */
    private static void selectContextMenuItemByJs(Instrumentation instrumentation,
            Activity activity, Tab tab, String jsCode, final int itemId) throws TimeoutException {
        RevampedContextMenuCoordinator menuCoordinator = openContextMenuByJs(tab, jsCode);
        Assert.assertNotNull("Failed to open context menu", menuCoordinator);

        selectOpenContextMenuItem(instrumentation, activity, menuCoordinator, itemId);
    }

    private static void selectOpenContextMenuItem(Instrumentation instrumentation,
            final Activity activity, final RevampedContextMenuCoordinator menuCoordinator,
            final int itemId) {
        instrumentation.runOnMainSync(() -> menuCoordinator.clickListItemForTesting(itemId));

        if (activity != null) {
            CriteriaHelper.pollInstrumentationThread(
                    new Criteria("Activity did not regain focus.") {
                        @Override
                        public boolean isSatisfied() {
                            return activity.hasWindowFocus();
                        }
                    });
        }
    }
}
