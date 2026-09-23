// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.OmniboxUma;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link LocationBarNavigator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarNavigatorUnitTest {
    private static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private OverrideUrlLoadingDelegate mOverrideUrlLoadingDelegate;
    @Mock private LocaleManager mLocaleManager;
    @Mock private OmniboxUma mOmniboxUma;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private ResourceRequestBody.Natives mResourceRequestBodyJni;
    @Mock private AutocompleteLoadCallback mAutocompleteLoadCallback;
    @Mock private LoadUrlParams mLoadUrlParams;
    @Mock private LoadUrlResult mLoadUrlResult;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<OmniboxLoadUrlParams> mOmniboxLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();
    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private final SettableMonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier =
            ObservableSuppliers.createMonotonic();
    private LocationBarNavigator mNavigator;

    @Before
    public void setUp() {
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        ResourceRequestBody.setNativesForTesting(mResourceRequestBodyJni);

        lenient().doReturn(mWebContents).when(mTab).getWebContents();
        lenient().doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();

        mProfileSupplier.set(mProfile);
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        mTabModelSelectorSupplier.set(mTabModelSelector);
        lenient().doReturn(mTabModel).when(mTabModelSelector).getModel(anyBoolean());

        mNavigator =
                new LocationBarNavigator(
                        mLocationBarDataProvider,
                        mProfileSupplier,
                        mTemplateUrlServiceSupplier,
                        mTabModelSelectorSupplier,
                        mOverrideUrlLoadingDelegate,
                        mLocaleManager,
                        mOmniboxUma);
    }

    private void setUpTab(GURL url, boolean isNativePage) {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(false)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());
        doReturn(isNativePage).when(mTab).isNativePage();
        doReturn(url).when(mTab).getUrl();
    }

    private void setUpNtpTab(boolean isIncognito) {
        setUpTab(JUnitTestGURLs.NTP_URL, /* isNativePage= */ true);
        doReturn(isIncognito).when(mTab).isIncognito();
        assertTrue(UrlUtilities.isNtpUrl(JUnitTestGURLs.NTP_URL));
    }

    @Test
    public void testLoadUrl_OverrideLoadingDelegate() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true)
                .when(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(any(), anyBoolean());

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());

        verify(mOverrideUrlLoadingDelegate)
                .willHandleLoadUrlWithPostData(mOmniboxLoadUrlParamsCaptor.capture(), anyBoolean());

        var params = mOmniboxLoadUrlParamsCaptor.getValue();
        assertEquals(TEST_URL, params.url);
        assertEquals(PageTransition.TYPED, params.transitionType);
        assertEquals(0, params.inputStartTimestamp);
        assertNull(null, params.postData);
        assertTrue(params.extraHeaders.isEmpty());
        assertFalse(params.openInNewTab);
        verify(mTab, never()).loadUrl(any());
    }

    @Test
    public void testLoadUrl_chromeExtensionScheme() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        String url = UrlConstants.CHROME_EXTENSION_SCHEME + "://id/popup.html";
        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(url, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(url, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
    }

    private void testLoadUrl_base() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlNoPostDelayedTaskFocusTab() {
        testLoadUrl_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlPostDelayedTaskFocusTab() {
        testLoadUrl_base();
    }

    private void testLoadUrlWithAutocompleteLoadCallback_base() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .setAutocompleteLoadCallback(mAutocompleteLoadCallback)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onLoadUrl(mTab, mLoadUrlParams, mLoadUrlResult);
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mAutocompleteLoadCallback).onLoadUrl(mLoadUrlParams, mLoadUrlResult);
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlWithAutocompleteLoadCallbackNoPostDelayedTaskFocusTab() {
        testLoadUrlWithAutocompleteLoadCallback_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrlWithAutocompleteLoadCallbackPostDelayedTaskFocusTab() {
        testLoadUrlWithAutocompleteLoadCallback_base();
    }

    @Test
    public void testLoadUrlWithExtraHeaders() {
        Map<String, String> headers = new HashMap<>();
        headers.put("Authorization", "Bearer token123");
        headers.put("Custom-Header", "custom-value");
        headers.put("Content-Type", "application/json");

        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setExtraHeaders(headers)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());
        String verbatimHeaders = mLoadUrlParamsCaptor.getValue().getVerbatimHeaders();
        assertTrue(verbatimHeaders.contains("Authorization: Bearer token123"));
        assertTrue(verbatimHeaders.contains("Custom-Header: custom-value"));
        assertTrue(verbatimHeaders.contains("Content-Type: application/json"));
    }

    @Test
    public void testLoadUrlWithPostData() {
        String text = "text";
        byte[] data = new byte[] {0, 1, 2, 3, 4};

        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(data).when(mResourceRequestBodyJni).createResourceRequestBodyFromBytes(any());

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setPostData(data)
                        .setExtraHeaders(Map.of("Content-Type", text))
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());
        assertTrue(mLoadUrlParamsCaptor.getValue().getVerbatimHeaders().contains(text));
        assertEquals(data, mLoadUrlParamsCaptor.getValue().getPostData().getEncodedNativeForm());
    }

    @Test
    public void testLoadUrl_openInNewWindow() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(mActivity).when(mTab).getContext();
        doReturn(1).when(mTab).getParentId();

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewWindow(true)
                        .build());

        verify(mMultiInstanceOrchestrator)
                .openUrlInOtherWindow(
                        eq(mActivity),
                        mLoadUrlParamsCaptor.capture(),
                        eq(1),
                        /* preferNew= */ eq(true),
                        /* isIncognito= */ eq(false));
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());
    }

    private void testLoadUrl_openInNewTab_base() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(false).when(mTab).isIncognitoBranded();

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ true)
                        .build());

        verify(mTabModelSelector)
                .openNewTab(
                        mLoadUrlParamsCaptor.capture(),
                        eq(TabLaunchType.FROM_OMNIBOX),
                        eq(mTab),
                        /* incognito= */ eq(false));
        assertEquals(TEST_URL, mLoadUrlParamsCaptor.getValue().getUrl());
        assertEquals(
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mLoadUrlParamsCaptor.getValue().getTransitionType());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrl_openInNewTabNoPostDelayedTaskFocusTab() {
        testLoadUrl_openInNewTab_base();
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.POST_DELAYED_TASK_FOCUS_TAB})
    public void testLoadUrl_openInNewTabPostDelayedTaskFocusTab() {
        testLoadUrl_openInNewTab_base();
    }

    @Test
    public void testLoadUrl_openInBackground() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(false).when(mTab).isIncognitoBranded();

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ true)
                        .setOpenInBackground(true)
                        .build());

        verify(mTabModelSelector)
                .openNewTab(
                        mLoadUrlParamsCaptor.capture(),
                        eq(TabLaunchType.FROM_OMNIBOX_BACKGROUND),
                        eq(null),
                        /* incognito= */ eq(false));
    }

    @Test
    public void testLoadUrl_removesExtraHeadersOnCrossOriginRedirect() {
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setOpenInNewTab(/* openInNewTab= */ false)
                        .build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertTrue(mLoadUrlParamsCaptor.getValue().getRemoveExtraHeadersOnCrossOriginRedirect());
    }

    @Test
    public void testRecordNavigationOnNtp_onNtp() {
        setUpNtpTab(/* isIncognito= */ false);

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ true);

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.GENERATED).build());
        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.GENERATED, /* isNtp= */ true);
    }

    @Test
    public void testRecordNavigationOnNtp_onNtp_incognito() {
        setUpNtpTab(/* isIncognito= */ true);

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build());

        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ false);
    }

    @Test
    public void testRecordNavigationOnNtp_otherNativePage() {
        setUpTab(JUnitTestGURLs.BLUE_1, /* isNativePage= */ true);
        assertFalse(UrlUtilities.isNtpUrl(JUnitTestGURLs.BLUE_1));
        doReturn(false).when(mTab).isIncognito();

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build());

        verify(mOmniboxUma)
                .recordNavigationOnNtp(TEST_URL, PageTransition.TYPED, /* isNtp= */ false);
    }

    @Test
    public void testRecordNavigationOnNtp_nonNativePage_doesNotRecord() {
        setUpTab(JUnitTestGURLs.BLUE_1, /* isNativePage= */ false);
        assertFalse(UrlUtilities.isNtpUrl(JUnitTestGURLs.BLUE_1));

        mNavigator.loadUrl(
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build());

        verify(mOmniboxUma, never()).recordNavigationOnNtp(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testLoadUrl_emptyUrlOnNtp_reloadsNtp() {
        setUpNtpTab(/* isIncognito= */ false);

        mNavigator.loadUrl(new OmniboxLoadUrlParams.Builder("", PageTransition.TYPED).build());

        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(JUnitTestGURLs.NTP_URL.getSpec(), mLoadUrlParamsCaptor.getValue().getUrl());
    }
}
