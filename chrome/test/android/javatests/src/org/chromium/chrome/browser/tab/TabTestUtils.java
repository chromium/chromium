// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.view.View;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Exposes helper functions to be used in tests to instrument tab interaction.
 */
public class TabTestUtils {
    /**
     * @return The observers registered for the given tab.
     */
    public static ObserverList.RewindableIterator<TabObserver> getTabObservers(Tab tab) {
        return tab.getTabObservers();
    }

    /**
     * Simulates the first visually non empty paint for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     */
    public static void simulateFirstVisuallyNonEmptyPaint(Tab tab) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().didFirstVisuallyNonEmptyPaint(tab);
    }

    /**
     * Simulates page loaded for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     */
    public static void simulatePageLoadFinished(Tab tab) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onPageLoadFinished(tab, tab.getUrl());
    }

    /**
     * Simulates page load failed for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param errorCode Errorcode to send to the page.
     */
    public static void simulatePageLoadFailed(Tab tab, int errorCode) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onPageLoadFailed(tab, errorCode);
    }

    /**
     * Simulates a crash of the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param sadTabShown Whether the sad tab was shown.
     */
    public static void simulateCrash(Tab tab, boolean sadTabShown) {
        setupSadTab(tab, sadTabShown);
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onCrash(tab);
    }

    private static void setupSadTab(Tab tab, boolean show) {
        boolean isShowing = SadTab.isShowing(tab);
        if (!show && isShowing) {
            SadTab.get(tab).removeIfPresent();
        } else if (show && !isShowing) {
            SadTab sadTab = new SadTab(tab) {
                @Override
                public View createView(Runnable suggestionAction, Runnable buttonAction,
                        boolean showSendFeedbackView, boolean isIncognito) {
                    return new View(tab.getThemedApplicationContext());
                }
            };
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                SadTab.initForTesting(tab, sadTab);
                sadTab.show();
            });
        }
    }

    /**
     * Simulates a change of theme color for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param color Color to send to the tab.
     */
    public static void simulateChangeThemeColor(Tab tab, int color) {
        RewindableIterator<TabObserver> observers = tab.getTabObservers();
        while (observers.hasNext()) observers.next().onDidChangeThemeColor(tab, color);
    }

    /**
     * Restore tab's internal states from a given {@link TabState}.
     * @param tab {@link Tab} to restore.
     * @param state {@link TabState} containing the state info to restore the tab with.
     */
    public static void restoreFieldsFromState(Tab tab, TabState state) {
        tab.restoreFieldsFromState(state);
    }

    /**
     * Swap {@link WebContents} object being used in a tab.
     * @param tab {@link Tab} object.
     * @param webContents {@link WebContents} to swap in.
     * @param didStartLoad Whether the content started loading.
     * @param didFinishLoad Whether the content finished loading.
     */
    public static void swapWebContents(
            Tab tab, WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        tab.swapWebContents(webContents, didStartLoad, didFinishLoad);
    }

    /**
     * @param tab {@link Tab} object.
     * @return {@link TabDelegateFactory} for a given tab.
     */
    public static TabDelegateFactory getDelegateFactory(Tab tab) {
        return tab.getDelegateFactory();
    }

    /**
     * @param tab {@link Tab} object.
     * @return {@code true} if the current tab is a custom tab.
     */
    public static boolean isCustomTab(Tab tab) {
        return getTabWebContentsDelegate(tab).isCustomTab();
    }

    /**
     * @param tab {@link Tab} object.
     * @return {@link TabWebContentsDelegateAndroid} object for a given tab.
     */
    public static TabWebContentsDelegateAndroid getTabWebContentsDelegate(Tab tab) {
        return tab.getTabWebContentsDelegateAndroid();
    }
}
