// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.contextmenu;

import static androidx.test.espresso.intent.Intents.intended;

import android.app.Activity;
import android.app.Instrumentation;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator;
import org.chromium.chrome.browser.contextmenu.ContextMenuHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/** A utility class to help open and interact with the context menu. */
public class ContextMenuUtils {
    /** Callback helper that also provides access to the last displayed context menu. */
    private static class OnContextMenuShownHelper extends CallbackHelper {
        private ContextMenuCoordinator mCoordinator;

        public void notifyCalled(ContextMenuCoordinator coordinator) {
            mCoordinator = coordinator;
            notifyCalled();
        }

        ContextMenuCoordinator getContextMenuCoordinator() {
            assert getCallCount() > 0;
            return mCoordinator;
        }
    }

    /**
     * Opens a context menu.
     *
     * @param tab The tab to open a context menu for.
     * @param openerDOMNodeId The DOM node to long press to open the context menu for.
     * @return The {@link ContextMenuCoordinator} of the context menu.
     */
    public static ContextMenuCoordinator openContextMenu(Tab tab, String openerDOMNodeId)
            throws TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        return openContextMenuByJs(tab, jsCode);
    }

    /**
     * Opens a context menu.
     *
     * @param tab The tab to open a context menu for.
     * @param jsCode The javascript to get the DOM node to long press to open the context menu for.
     * @return The {@link ContextMenuCoordinator} of the context menu.
     */
    private static ContextMenuCoordinator openContextMenuByJs(Tab tab, String jsCode)
            throws TimeoutException {
        final OnContextMenuShownHelper helper = new OnContextMenuShownHelper();
        ContextMenuHelper.setMenuShownCallbackForTests(helper::notifyCalled);

        int callCount = helper.getCallCount();
        DOMUtils.longPressNodeByJs(tab.getWebContents(), jsCode);

        helper.waitForCallback(callCount);
        return helper.getContextMenuCoordinator();
    }

    /**
     * Opens and selects an item from a context menu.
     *
     * @param tab The tab to open a context menu for.
     * @param openerDOMNodeId The DOM node to long press to open the context menu for.
     * @param itemId The context menu item ID to select.
     * @param activity The activity to assert for gaining focus after click or null.
     */
    public static void selectContextMenuItem(
            Instrumentation instrumentation,
            Activity activity,
            Tab tab,
            String openerDOMNodeId,
            final int itemId)
            throws TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        selectContextMenuItemByJs(instrumentation, activity, tab, jsCode, itemId);
    }

    /**
     * Opens and selects an item from a context menu asserting that an intent will be sent with a
     * specific package name. the app will then send an intent to (which is verified by a downstream
     * assertion).
     *
     * @param instrumentation Instrumentation module used for executing test behavior.
     * @param expectedActivity The activity to assert for gaining focus after click or null.
     * @param tab The tab to open a context menu for.
     * @param openerDOMNodeId The DOM node to long press to open the context menu for.
     * @param itemId The context menu item ID to select.
     * @param expectedIntentPackage If firing an external intent the expected package name of the
     *     target.
     */
    public static void selectContextMenuItemWithExpectedIntent(
            Instrumentation instrumentation,
            Activity expectedActivity,
            Tab tab,
            String openerDOMNodeId,
            final int itemId,
            String expectedIntentPackage)
            throws TimeoutException {
        String jsCode = "document.getElementById('" + openerDOMNodeId + "')";
        selectContextMenuItemByJs(
                instrumentation, expectedActivity, tab, jsCode, itemId, expectedIntentPackage);
    }

    /**
     * Selects the context menu chip asserting that an intent will be sent with a specific package
     * name. Note that this method does not open the context menu.
     *
     * @param instrumentation Instrumentation module used for executing test behavior.
     * @param expectedActivity The activity to assert for gaining focus after click or null.
     * @param menuCoordinator The menu coordinator which manages the context menu.
     * @param openerDOMNodeId The DOM node to long press to open the context menu for.
     * @param itemId The context menu item ID to select.
     * @param expectedIntentPackage If firing an external intent the expected package name of the
     *     target.
     */
    public static void selectAlreadyOpenedContextMenuChipWithExpectedIntent(
            Instrumentation instrumentation,
            Activity expectedActivity,
            ContextMenuCoordinator menuCoordinator,
            String openerDOMNodeId,
            final int itemId,
            String expectedIntentPackage)
            throws TimeoutException {
        Assert.assertNotNull("Menu coordinator was not provided.", menuCoordinator);

        selectAlreadyOpenedContextMenuChip(
                instrumentation, expectedActivity, menuCoordinator, itemId, expectedIntentPackage);
    }

    /**
     * Long presses to open and selects an item from a context menu.
     *
     * @param instrumentation Instrumentation module used for executing test behavior.
     * @param expectedActivity The activity to assert for gaining focus after click or null.
     * @param tab The tab to open a context menu for.
     * @param jsCode The javascript to get the DOM node to long press to open the context menu for.
     * @param itemId The context menu item ID to select.
     * @param expectedIntentPackage If expecting an external intent the expected package name.
     */
    public static void selectContextMenuItemByJs(
            Instrumentation instrumentation,
            Activity expectedActivity,
            Tab tab,
            String jsCode,
            final int itemId,
            String expectedIntentPackage)
            throws TimeoutException {
        ContextMenuCoordinator menu = openContextMenuByJs(tab, jsCode);
        Assert.assertNotNull("Failed to open context menu", menu);

        selectOpenContextMenuItem(
                instrumentation, expectedActivity, menu, itemId, expectedIntentPackage);
    }

    /**
     * Long presses to open and selects an item from a context menu.
     *
     * @param tab The tab to open a context menu for.
     * @param jsCode The javascript to get the DOM node to long press to open the context menu for.
     * @param itemId The context menu item ID to select.
     * @param activity The activity to assert for gaining focus after click or null.
     */
    private static void selectContextMenuItemByJs(
            Instrumentation instrumentation,
            Activity activity,
            Tab tab,
            String jsCode,
            final int itemId)
            throws TimeoutException {
        ContextMenuCoordinator menuCoordinator = openContextMenuByJs(tab, jsCode);
        Assert.assertNotNull("Failed to open context menu", menuCoordinator);

        selectOpenContextMenuItem(instrumentation, activity, menuCoordinator, itemId);
    }

    private static void selectOpenContextMenuItem(
            Instrumentation instrumentation,
            final Activity activity,
            final ContextMenuCoordinator menuCoordinator,
            final int itemId) {
        instrumentation.runOnMainSync(() -> menuCoordinator.clickListItemForTesting(itemId));

        if (activity != null) {
            CriteriaHelper.pollInstrumentationThread(activity::hasWindowFocus);
        }
    }

    private static void selectOpenContextMenuItem(
            Instrumentation instrumentation,
            final Activity expectedActivity,
            final ContextMenuCoordinator menuCoordinator,
            final int itemId,
            final String expectedIntentPackage) {
        if (expectedIntentPackage != null) {
            Intents.init();
        }

        instrumentation.runOnMainSync(() -> menuCoordinator.clickListItemForTesting(itemId));

        if (expectedActivity != null) {
            CriteriaHelper.pollInstrumentationThread(expectedActivity::hasWindowFocus);
        }

        if (expectedIntentPackage != null) {
            // This line must only execute after all test behavior has completed
            // or it will intefere with the expected behavior.
            intended(IntentMatchers.hasPackage(expectedIntentPackage));
            Intents.release();
        }
    }

    private static void selectAlreadyOpenedContextMenuChip(
            Instrumentation instrumentation,
            final Activity expectedActivity,
            final ContextMenuCoordinator menuCoordinator,
            final int itemId,
            final String expectedIntentPackage) {
        if (expectedIntentPackage != null) {
            Intents.init();
        }

        instrumentation.runOnMainSync(() -> menuCoordinator.clickChipForTesting());

        if (expectedActivity != null) {
            CriteriaHelper.pollInstrumentationThread(expectedActivity::hasWindowFocus);
        }

        if (expectedIntentPackage != null) {
            // This line must only execute after all test behavior has completed
            // or it will intefere with the expected behavior.
            intended(IntentMatchers.hasPackage(expectedIntentPackage));
            Intents.release();
        }
    }
}
