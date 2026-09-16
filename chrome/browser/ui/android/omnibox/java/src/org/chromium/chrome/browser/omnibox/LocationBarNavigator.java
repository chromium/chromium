// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.OmniboxUma;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;

/**
 * Controller responsible for URL navigation originating from the Omnibox.
 *
 * <p>Handles extension scheme routing, NTP navigation logging, geolocation/verbatim headers, post
 * data, multi-window/new-tab opening, and tab load URL dispatching.
 */
@NullMarked
class LocationBarNavigator {
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final OneshotSupplier<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    private final LocaleManager mLocaleManager;
    private final OmniboxUma mOmniboxUma;

    public LocationBarNavigator(
            LocationBarDataProvider locationBarDataProvider,
            MonotonicObservableSupplier<Profile> profileSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            LocaleManager localeManager,
            OmniboxUma omniboxUma) {
        mLocationBarDataProvider = locationBarDataProvider;
        mProfileSupplier = profileSupplier;
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mOverrideUrlLoadingDelegate = overrideUrlLoadingDelegate;
        mLocaleManager = localeManager;
        mOmniboxUma = omniboxUma;
    }

    /**
     * Loads the URL described by {@link OmniboxLoadUrlParams}.
     *
     * @param omniboxLoadUrlParams Parameters describing the URL load request.
     * @return false if the load was consumed by the override delegate or an extension; true
     *     otherwise.
     */
    public boolean loadUrl(OmniboxLoadUrlParams omniboxLoadUrlParams) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarNavigator.loadUrl")) {
            Tab currentTab = mLocationBarDataProvider.getTab();

            // TODO(crbug.com/40693835): Should be taking a full loaded LoadUrlParams.
            if (mOverrideUrlLoadingDelegate.willHandleLoadUrlWithPostData(
                    omniboxLoadUrlParams, mLocationBarDataProvider.isIncognito())) {
                return false;
            }

            String url = omniboxLoadUrlParams.url;
            if (currentTab != null) {
                url = handleNtpNavigationAndGetUrl(currentTab, omniboxLoadUrlParams);
                attachTabLoadObserver(currentTab, omniboxLoadUrlParams);
            }

            if (currentTab != null && !url.isEmpty()) {
                LoadUrlParams loadUrlParams = buildLoadUrlParams(omniboxLoadUrlParams, url);
                dispatchUrlLoad(currentTab, loadUrlParams, omniboxLoadUrlParams);
                RecordUserAction.record("MobileOmniboxUse");
            }

            mLocaleManager.recordLocaleBasedSearchMetrics(
                    /* isFromSearchWidget= */ false, url, omniboxLoadUrlParams.transitionType);
            return true;
        }
    }

    private String handleNtpNavigationAndGetUrl(
            Tab currentTab, OmniboxLoadUrlParams omniboxLoadUrlParams) {
        boolean isCurrentTabNtpUrl = OmniboxUrlUtils.isNtpUrl(currentTab.getUrl());
        if (currentTab.isNativePage() || isCurrentTabNtpUrl) {
            mOmniboxUma.recordNavigationOnNtp(
                    omniboxLoadUrlParams.url,
                    omniboxLoadUrlParams.transitionType,
                    !currentTab.isIncognito() && isCurrentTabNtpUrl);
            // Passing in an empty string should not do anything unless the user is at the
            // NTP. Since the NTP has no url, pressing enter while clicking on the URL bar
            // should refresh the page as it does when you click and press enter on any
            // other site.
            if (omniboxLoadUrlParams.url.isEmpty()) {
                return currentTab.getUrl().getSpec();
            }
        }
        return omniboxLoadUrlParams.url;
    }

    private void attachTabLoadObserver(Tab currentTab, OmniboxLoadUrlParams omniboxLoadUrlParams) {
        if (omniboxLoadUrlParams.callback == null) return;

        currentTab.addObserver(
                new TabObserver() {
                    @Override
                    public void onLoadUrl(
                            Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
                        omniboxLoadUrlParams.callback.onLoadUrl(params, loadUrlResult);
                        tab.removeObserver(this);
                    }
                });
    }

    private LoadUrlParams buildLoadUrlParams(
            OmniboxLoadUrlParams omniboxLoadUrlParams, String url) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        try (TimingMetric record =
                TimingMetric.shortUptime("Android.Omnibox.SetGeolocationHeadersTime")) {
            loadUrlParams.setVerbatimHeaders(
                    GeolocationHeader.getGeoHeader(
                            url,
                            assertNonNull(mProfileSupplier.get()),
                            mTemplateUrlServiceSupplier.get()));
        }
        loadUrlParams.setRemoveExtraHeadersOnCrossOriginRedirect(true);
        loadUrlParams.setTransitionType(
                omniboxLoadUrlParams.transitionType | PageTransition.FROM_ADDRESS_BAR);
        if (omniboxLoadUrlParams.inputStartTimestamp != 0) {
            loadUrlParams.setInputStartTimestamp(omniboxLoadUrlParams.inputStartTimestamp);
        }

        if (!omniboxLoadUrlParams.extraHeaders.isEmpty()) {
            StringBuilder headers = new StringBuilder();
            for (var entry : omniboxLoadUrlParams.extraHeaders.entrySet()) {
                headers.append(entry.getKey());
                headers.append(": ");
                headers.append(entry.getValue());
                headers.append("\r\n");
            }
            String previousHeaders = loadUrlParams.getVerbatimHeaders();
            if (!TextUtils.isEmpty(previousHeaders)) {
                headers.append(previousHeaders);
            }

            loadUrlParams.setVerbatimHeaders(headers.toString());
        }

        if (omniboxLoadUrlParams.postData != null && omniboxLoadUrlParams.postData.length != 0) {
            loadUrlParams.setPostData(
                    ResourceRequestBody.createFromBytes(omniboxLoadUrlParams.postData));
        }

        return loadUrlParams;
    }

    private void dispatchUrlLoad(
            Tab currentTab,
            LoadUrlParams loadUrlParams,
            OmniboxLoadUrlParams omniboxLoadUrlParams) {
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        boolean processed = false;
        if (omniboxLoadUrlParams.openInNewWindow) {
            Context tabContext = currentTab.getContext();
            if (tabContext instanceof Activity sourceActivity) {
                processed =
                        MultiInstanceOrchestratorFactory.getInstance()
                                .openUrlInOtherWindow(
                                        sourceActivity,
                                        loadUrlParams,
                                        currentTab.getParentId(),
                                        /* preferNew= */ true,
                                        currentTab.isIncognitoBranded());
            }
        } else if (omniboxLoadUrlParams.openInNewTab && tabModelSelector != null) {
            tabModelSelector.openNewTab(
                    loadUrlParams,
                    TabLaunchType.FROM_OMNIBOX,
                    currentTab,
                    currentTab.isIncognito());
            processed = true;
        }
        if (!processed) {
            currentTab.loadUrl(loadUrlParams);
        }
    }
}
