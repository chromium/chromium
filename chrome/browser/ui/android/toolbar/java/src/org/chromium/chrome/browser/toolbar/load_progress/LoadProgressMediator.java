// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import androidx.annotation.NonNull;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressProperties.CompletionState;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the load progress bar. Listens for changes to the loading state of the current tab
 * and adjusts its property model accordingly.
 */
public class LoadProgressMediator {
    static final float MINIMUM_LOAD_PROGRESS = 0.05f;

    private final PropertyModel mModel;
    private final CurrentTabObserver mTabObserver;
    private final LoadProgressSimulator mLoadProgressSimulator;
    private boolean mPreventUpdates;

    /**
     * @param tabSupplier An observable supplier of the current {@link Tab}.
     * @param model MVC property model instance used for load progress bar.
     */
    public LoadProgressMediator(
            @NonNull ObservableSupplier<Tab> tabSupplier, @NonNull PropertyModel model) {
        mModel = model;
        mLoadProgressSimulator = new LoadProgressSimulator(model);
        mTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onDidStartNavigationInPrimaryMainFrame(
                                    Tab tab, NavigationHandle navigation) {
                                if (navigation.isSameDocument()) {
                                    return;
                                }

                                if (NativePage.isNativePageUrl(
                                        navigation.getUrl(),
                                        tab.isIncognito(),
                                        navigation.isPdf())) {
                                    finishLoadProgress(false);
                                    return;
                                }

                                mLoadProgressSimulator.cancel();
                                startLoadProgress();
                                updateLoadProgress(tab.getProgress());
                            }

                            @Override
                            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                                if (!toDifferentDocument) return;

                                // If we made some progress, fast-forward to complete, otherwise
                                // just dismiss any MINIMUM_LOAD_PROGRESS that had been set.
                                if (tab.getProgress() > MINIMUM_LOAD_PROGRESS
                                        && tab.getProgress() < 1) {
                                    updateLoadProgress(1.0f);
                                }
                                finishLoadProgress(true);
                            }

                            @Override
                            public void onLoadProgressChanged(Tab tab, float progress) {
                                if (tab.getUrl() == null
                                        || UrlUtilities.isNtpUrl(tab.getUrl())
                                        || NativePage.isNativePageUrl(
                                                tab.getUrl(),
                                                tab.isIncognito(),
                                                tab.isNativePage()
                                                        && tab.getNativePage().isPdf())) {
                                    return;
                                }

                                updateLoadProgress(progress);
                            }

                            @Override
                            public void onWebContentsSwapped(
                                    Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                                // If loading both started and finished before we swapped in the
                                // WebContents, we won't get any load progress signals. Otherwise,
                                // we should receive at least one real signal so we don't need to
                                // simulate them.
                                if (didStartLoad && didFinishLoad && !mPreventUpdates) {
                                    mLoadProgressSimulator.start();
                                }
                            }

                            @Override
                            public void onCrash(Tab tab) {
                                finishLoadProgress(false);
                            }
                        },
                        this::onNewTabObserved);

        onNewTabObserved(tabSupplier.get());
    }

    /** Simulates progressbar being filled over a short time. */
    void simulateLoadProgressCompletion() {
        mLoadProgressSimulator.start();
    }

    /**
     * Whether progressbar should be updated on tab progress changes.
     * @param preventUpdates If true, prevents updating progressbar when the tab it's observing
     *                       is being loaded.
     */
    void setPreventUpdates(boolean preventUpdates) {
        mPreventUpdates = preventUpdates;
    }

    private void onNewTabObserved(Tab tab) {
        if (tab == null) {
            return;
        }

        if (tab.isLoading()) {
            if (NativePage.isNativePageUrl(
                    tab.getUrl(),
                    tab.isIncognito(),
                    tab.isNativePage() && tab.getNativePage().isPdf())) {
                finishLoadProgress(false);
            } else {
                startLoadProgress();
                updateLoadProgress(tab.getProgress());
            }
        } else {
            finishLoadProgress(false);
        }
    }

    private void startLoadProgress() {
        if (mPreventUpdates) return;

        mModel.set(LoadProgressProperties.COMPLETION_STATE, CompletionState.UNFINISHED);
    }

    private void updateLoadProgress(float progress) {
        if (mPreventUpdates) return;

        progress = Math.max(progress, MINIMUM_LOAD_PROGRESS);
        mModel.set(LoadProgressProperties.PROGRESS, progress);
        if (MathUtils.areFloatsEqual(progress, 1)) finishLoadProgress(true);
    }

    private void finishLoadProgress(boolean animateCompletion) {
        mLoadProgressSimulator.cancel();
        @CompletionState
        int completionState =
                animateCompletion
                        ? CompletionState.FINISHED_DO_ANIMATE
                        : CompletionState.FINISHED_DONT_ANIMATE;
        mModel.set(LoadProgressProperties.COMPLETION_STATE, completionState);
    }

    /** Destroy load progress bar object. */
    public void destroy() {
        mTabObserver.destroy();
    }
}
