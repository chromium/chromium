// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.mockito.Mockito;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;

/** Exposes helper functions to be used in tests to instrument tab interaction. */
public class TabTestUtils {
    /**
     * @return The observers registered for the given tab.
     */
    public static ObserverList.RewindableIterator<TabObserver> getTabObservers(Tab tab) {
        return ((TabImpl) tab).getTabObservers();
    }

    /**
     * Initializes {@link Tab} with {@code webContents}. If {@code webContents} is {@code null} a
     * new {@link WebContents} will be created for this {@link Tab}.
     *
     * @see TabImpl#initialize()
     */
    public static void initialize(
            Tab tab,
            Tab parent,
            @Nullable @TabCreationState Integer creationState,
            @Nullable LoadUrlParams loadUrlParams,
            @Nullable String titleForLazyLoad,
            WebContents webContents,
            @Nullable TabDelegateFactory delegateFactory,
            boolean initiallyHidden,
            TabState tabState,
            boolean initializeRenderer) {
        ((TabImpl) tab)
                .initialize(
                        parent,
                        creationState,
                        loadUrlParams,
                        titleForLazyLoad,
                        webContents,
                        delegateFactory,
                        initiallyHidden,
                        tabState,
                        initializeRenderer);
    }

    /** Set the last hidden timestamp. */
    public static void setLastNavigationCommittedTimestampMillis(Tab tab, long ts) {
        ((TabImpl) tab).setLastNavigationCommittedTimestampMillis(ts);
    }

    /** Set a new {@link WebContentsState} to a given tab. */
    public static void setWebContentsState(Tab tab, WebContentsState webContentsState) {
        ((TabImpl) tab).setWebContentsState(webContentsState);
    }

    /**
     * Simulates the first visually non empty paint for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     */
    public static void simulateFirstVisuallyNonEmptyPaint(Tab tab) {
        RewindableIterator<TabObserver> observers = ((TabImpl) tab).getTabObservers();
        while (observers.hasNext()) observers.next().didFirstVisuallyNonEmptyPaint(tab);
    }

    /**
     * Simulates page loaded for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     */
    public static void simulatePageLoadFinished(Tab tab) {
        RewindableIterator<TabObserver> observers = ((TabImpl) tab).getTabObservers();
        while (observers.hasNext()) observers.next().onPageLoadFinished(tab, tab.getUrl());
    }

    /**
     * Simulates page load failed for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param errorCode Errorcode to send to the page.
     */
    public static void simulatePageLoadFailed(Tab tab, int errorCode) {
        RewindableIterator<TabObserver> observers = ((TabImpl) tab).getTabObservers();
        while (observers.hasNext()) observers.next().onPageLoadFailed(tab, errorCode);
    }

    /**
     * Simulates a crash of the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param sadTabShown Whether the sad tab was shown.
     */
    public static void simulateCrash(Tab tab, boolean sadTabShown) {
        setupSadTab(tab, sadTabShown);
        RewindableIterator<TabObserver> observers = ((TabImpl) tab).getTabObservers();
        while (observers.hasNext()) observers.next().onCrash(tab);
    }

    private static void setupSadTab(Tab tab, boolean show) {
        boolean isShowing = SadTab.isShowing(tab);
        if (!show && isShowing) {
            SadTab.get(tab).removeIfPresent();
        } else if (show && !isShowing) {
            SadTab sadTab =
                    new SadTab(tab) {
                        @Override
                        public View createView(
                                Context context,
                                Runnable suggestionAction,
                                Runnable buttonAction,
                                boolean showSendFeedbackView,
                                boolean isIncognito) {
                            return new View(context);
                        }
                    };
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SadTab.initForTesting(tab, sadTab);
                        sadTab.show(
                                ((TabImpl) tab).getThemedApplicationContext(), () -> {}, () -> {});
                    });
        }
    }

    /**
     * Simulates a change of theme color for the given |tab|.
     * @param tab Tab on which the simulated event will be sent.
     * @param color Color to send to the tab.
     */
    public static void simulateChangeThemeColor(Tab tab, int color) {
        RewindableIterator<TabObserver> observers = ((TabImpl) tab).getTabObservers();
        while (observers.hasNext()) observers.next().onDidChangeThemeColor(tab, color);
    }

    /**
     * Restore tab's internal states from a given {@link TabState}.
     * @param tab {@link Tab} to restore.
     * @param state {@link TabState} containing the state info to restore the tab with.
     */
    public static void restoreFieldsFromState(Tab tab, TabState state) {
        ((TabImpl) tab).restoreFieldsFromState(state);
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
        ((TabImpl) tab).swapWebContents(webContents, didStartLoad, didFinishLoad);
    }

    /**
     * @param tab {@link Tab} object.
     * @return {@link TabDelegateFactory} for a given tab.
     */
    public static TabDelegateFactory getDelegateFactory(Tab tab) {
        return ((TabImpl) tab).getDelegateFactory();
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
    public static TabWebContentsDelegateAndroidImpl getTabWebContentsDelegate(Tab tab) {
        return ((TabImpl) tab).getTabWebContentsDelegateAndroid();
    }

    /**
     * Open a new tab.
     * @param tab {@link Tab} object.
     * @param url URL to open.
     * @param extraHeaders   Extra headers to apply when requesting the tab's URL.
     * @param postData       Post-data to include in the tab URL's request body.
     * @param disposition         The new tab disposition, defined in
     *                            //ui/base/mojo/window_open_disposition.mojom.
     * @param isRendererInitiated Whether or not the renderer initiated this action.
     */
    public static void openNewTab(
            Tab tab,
            GURL url,
            String extraHeaders,
            ResourceRequestBody postData,
            int disposition,
            boolean isRendererInitiated) {
        getTabWebContentsDelegate(tab)
                .openNewTab(url, extraHeaders, postData, disposition, isRendererInitiated);
    }

    /** Show {@link org.chromium.chrome.browser.infobar.FrameBustBlockInfoBar}. */
    public static void showFramebustBlockInfobarForTesting(Tab tab, String url) {
        getTabWebContentsDelegate(tab).showFramebustBlockInfobarForTesting(url);
    }

    /**
     * Sets whether the tab is showing an error page.  This is reset whenever the tab finishes a
     * navigation.
     * @param tab {@link Tab} object.
     * @param isShowingErrorPage Whether the tab shows an error page.
     */
    public static void setIsShowingErrorPage(Tab tab, boolean isShowingErrorPage) {
        ((TabImpl) tab).setIsShowingErrorPage(isShowingErrorPage);
    }

    /** Mock Tab interface impl JNI for testing. */
    public static void mockTabJni(JniMocker jniMocker) {
        TabImpl.Natives tabImplJni = Mockito.mock(TabImpl.Natives.class);
        jniMocker.mock(TabImplJni.TEST_HOOKS, tabImplJni);
    }
}
