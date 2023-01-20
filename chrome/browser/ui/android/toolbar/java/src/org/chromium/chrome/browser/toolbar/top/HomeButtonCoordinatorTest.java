// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import com.google.common.collect.ImmutableMap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for HomeButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomeButtonCoordinatorTest.ShadowChromeFeatureList.class})
@DisableFeatures(ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS)
@EnableFeatures(ChromeFeatureList.ENABLE_IPH)
public class HomeButtonCoordinatorTest {
    private static final GURL NTP_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL);
    private static final GURL NOT_NTP_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    private static final ImmutableMap<Integer, String> ID_TO_STRING_MAP = ImmutableMap.of(
            R.string.iph_ntp_with_feed_text, "feed", R.string.iph_ntp_without_feed_text, "no_feed",
            R.string.iph_ntp_with_feed_accessibility_text, "feed_a11y",
            R.string.iph_ntp_without_feed_accessibility_text, "no_feed_ally");

    private static final ToolbarIntentMetadata DEFAULT_INTENT_METADATA =
            new ToolbarIntentMetadata(/*isMainIntent*/ true, /*isIntentWithEffect*/ false);

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static Map<String, String> sParamMap;
        @Implementation
        public static String getFieldTrialParamByFeature(String featureName, String paramName) {
            Assert.assertEquals("Wrong feature name",
                    FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE, featureName);
            if (sParamMap.containsKey(paramName)) return sParamMap.get(paramName);
            return "";
        }
    }

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private Context mContext;
    @Mock
    private HomeButton mHomeButton;
    @Mock
    private android.content.res.Resources mResources;
    @Mock
    private UserEducationHelper mUserEducationHelper;

    @Captor
    private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private boolean mIsIncognito;
    private final OneshotSupplierImpl<ToolbarIntentMetadata> mIntentMetadataOneshotSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier =
            new OneshotSupplierImpl<>();
    private boolean mIsFeedEnabled;
    private boolean mIsHomepageNonNtp;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mContext.getResources()).thenReturn(mResources);
        for (Map.Entry<Integer, String> idAndString : ID_TO_STRING_MAP.entrySet()) {
            when(mResources.getString(idAndString.getKey())).thenReturn(idAndString.getValue());
        }
        // Defaults most test cases expect, can be overridden by each test though.
        when(mHomeButton.isShown()).thenReturn(true);
        ShadowChromeFeatureList.sParamMap = new HashMap<>();
        mIsFeedEnabled = true;
        mIsHomepageNonNtp = false;
        mIsIncognito = false;
    }

    private HomeButtonCoordinator newHomeButtonCoordinator(View view) {
        // clang-format off
        return new HomeButtonCoordinator(mContext, view, mUserEducationHelper, () -> mIsIncognito,
                mIntentMetadataOneshotSupplier, mPromoShownOneshotSupplier,
                () -> mIsHomepageNonNtp, () -> mIsFeedEnabled, new ObservableSupplierImpl<>());
        // clang-format on
    }

    private void verifyIphNotShown() {
        verify(mUserEducationHelper, never()).requestShowIPH(any());

        // Reset so subsequent checks in the test start with a clean slate.
        Mockito.reset(mUserEducationHelper);
    }

    private void verifyIphShownWithFeed() {
        verifyIphShownWithStringIds(
                R.string.iph_ntp_with_feed_text, R.string.iph_ntp_with_feed_accessibility_text);
    }
    private void verifyIphShownWithoutFeed() {
        verifyIphShownWithStringIds(R.string.iph_ntp_without_feed_text,
                R.string.iph_ntp_without_feed_accessibility_text);
    }

    private void verifyIphShownWithStringIds(int contentId, int accessibilityId) {
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());
        Assert.assertEquals("Wrong feature name", FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
                mIPHCommandCaptor.getValue().featureName);
        Assert.assertEquals("Wrong text id", ID_TO_STRING_MAP.get(contentId),
                mIPHCommandCaptor.getValue().contentString);
        Assert.assertEquals("Wrong accessibility text id", ID_TO_STRING_MAP.get(accessibilityId),
                mIPHCommandCaptor.getValue().accessibilityText);

        // Reset so subsequent checks in the test start with a clean slate.
        Mockito.reset(mUserEducationHelper);
    }

    @Test
    public void testDestroy() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);

        homeButtonCoordinator.destroy();

        // Supplier calls should be dropped, and not crash.
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);
    }

    @Test
    public void testIphDefault() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphWithoutFeed() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIsFeedEnabled = false;
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithoutFeed();
    }

    @Test
    public void testIphLoadNtp() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        homeButtonCoordinator.handlePageLoadFinished(NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphHomepageNotNtp() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIsHomepageNonNtp = true;
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        homeButtonCoordinator.handlePageLoadFinished(NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphNoView() {
        HomeButtonCoordinator homeButtonCoordinator = newHomeButtonCoordinator(/*view*/ null);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphIncognito() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        mIsIncognito = true;
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        mIsIncognito = false;
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphIsShown() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(false);

        when(mHomeButton.isShown()).thenReturn(false);
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        when(mHomeButton.isShown()).thenReturn(true);
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphMainIntentFalse() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(
                new ToolbarIntentMetadata(/*isMainIntent*/ false, /*isIntentWithEffect*/ false));
        mPromoShownOneshotSupplier.set(false);

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME, "");
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME, "false");
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.MAIN_INTENT_FROM_LAUNCHER_PARAM_NAME, "true");
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphIntentWithEffectTrue() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(
                new ToolbarIntentMetadata(/*isMainIntent*/ true, /*isIntentWithEffect*/ true));
        mPromoShownOneshotSupplier.set(false);

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.INTENT_WITH_EFFECT_PARAM_NAME, "");
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.INTENT_WITH_EFFECT_PARAM_NAME, "false");
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        ShadowChromeFeatureList.sParamMap.put(
                HomeButtonCoordinator.INTENT_WITH_EFFECT_PARAM_NAME, "true");
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphShowedPromo() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        mPromoShownOneshotSupplier.set(true);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphDelayedIntentMetadata() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mPromoShownOneshotSupplier.set(false);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphDelayedPromoShown() {
        HomeButtonCoordinator homeButtonCoordinator =
                newHomeButtonCoordinator(/*view*/ mHomeButton);
        mIntentMetadataOneshotSupplier.set(DEFAULT_INTENT_METADATA);

        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        mPromoShownOneshotSupplier.set(false);
        homeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }
}
