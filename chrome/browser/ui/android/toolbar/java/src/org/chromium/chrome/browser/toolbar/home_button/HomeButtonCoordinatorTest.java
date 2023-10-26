// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import com.google.common.collect.ImmutableMap;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Map;

/** Unit tests for HomeButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeButtonCoordinatorTest {
    private static final GURL NTP_URL = JUnitTestGURLs.NTP_URL;
    private static final GURL NOT_NTP_URL = JUnitTestGURLs.EXAMPLE_URL;

    private static final ImmutableMap<Integer, String> ID_TO_STRING_MAP =
            ImmutableMap.of(
                    R.string.iph_ntp_with_feed_text,
                    "feed",
                    R.string.iph_ntp_without_feed_text,
                    "no_feed",
                    R.string.iph_ntp_with_feed_accessibility_text,
                    "feed_a11y",
                    R.string.iph_ntp_without_feed_accessibility_text,
                    "no_feed_ally");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private HomeButton mHomeButton;
    @Mock private android.content.res.Resources mResources;
    @Mock private UserEducationHelper mUserEducationHelper;

    @Captor private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private boolean mIsIncognito;
    private final OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier =
            new OneshotSupplierImpl<>();
    private boolean mIsFeedEnabled;
    private boolean mIsHomepageNonNtp;
    private boolean mIsHomeButtonMenuDisabled;
    private HomeButtonCoordinator mHomeButtonCoordinator;

    @Before
    public void setUp() {
        when(mContext.getResources()).thenReturn(mResources);
        for (Map.Entry<Integer, String> idAndString : ID_TO_STRING_MAP.entrySet()) {
            when(mResources.getString(idAndString.getKey())).thenReturn(idAndString.getValue());
        }
        when(mHomeButton.getRootView()).thenReturn(Mockito.mock(View.class));
        when(mHomeButton.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(LayoutInflater.from(ContextUtils.getApplicationContext()));
        // Defaults most test cases expect, can be overridden by each test though.
        when(mHomeButton.isShown()).thenReturn(true);
        mIsFeedEnabled = true;
        mIsHomepageNonNtp = false;
        mIsIncognito = false;
        mIsHomeButtonMenuDisabled = false;

        mHomeButtonCoordinator =
                new HomeButtonCoordinator(
                        mContext,
                        mHomeButton,
                        mUserEducationHelper,
                        () -> mIsIncognito,
                        mPromoShownOneshotSupplier,
                        () -> mIsHomepageNonNtp,
                        () -> mIsFeedEnabled,
                        new ObservableSupplierImpl<>(),
                        (context) -> {},
                        () -> mIsHomeButtonMenuDisabled);
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
        verifyIphShownWithStringIds(
                R.string.iph_ntp_without_feed_text,
                R.string.iph_ntp_without_feed_accessibility_text);
    }

    private void verifyIphShownWithStringIds(int contentId, int accessibilityId) {
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());
        // Note that we aren't actually showing the IPH (unit tests) so resources aren't resolved
        // unless we force it.
        mIPHCommandCaptor.getValue().fetchFromResources();
        assertEquals(
                "Wrong feature name",
                FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
                mIPHCommandCaptor.getValue().featureName);
        assertEquals(
                "Wrong text id",
                ID_TO_STRING_MAP.get(contentId),
                mIPHCommandCaptor.getValue().contentString);
        assertEquals(
                "Wrong accessibility text id",
                ID_TO_STRING_MAP.get(accessibilityId),
                mIPHCommandCaptor.getValue().accessibilityText);

        // Reset so subsequent checks in the test start with a clean slate.
        Mockito.reset(mUserEducationHelper);
    }

    @Test
    public void testDestroy() {
        mHomeButtonCoordinator.destroy();

        // Supplier calls should be dropped, and not crash.
        mPromoShownOneshotSupplier.set(false);
    }

    @Test
    public void testIphDefault() {
        mPromoShownOneshotSupplier.set(false);

        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphWithoutFeed() {
        mIsFeedEnabled = false;
        mPromoShownOneshotSupplier.set(false);

        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithoutFeed();
    }

    @Test
    public void testIphLoadNtp() {
        mPromoShownOneshotSupplier.set(false);

        mHomeButtonCoordinator.handlePageLoadFinished(NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphHomepageNotNtp() {
        mIsHomepageNonNtp = true;
        mPromoShownOneshotSupplier.set(false);

        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        mHomeButtonCoordinator.handlePageLoadFinished(NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphIncognito() {
        mPromoShownOneshotSupplier.set(false);

        mIsIncognito = true;
        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        mIsIncognito = false;
        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphIsShown() {
        mPromoShownOneshotSupplier.set(false);

        when(mHomeButton.isShown()).thenReturn(false);
        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        when(mHomeButton.isShown()).thenReturn(true);
        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testIphShowedPromo() {
        mPromoShownOneshotSupplier.set(true);

        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();
    }

    @Test
    public void testIphDelayedPromoShown() {
        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphNotShown();

        mPromoShownOneshotSupplier.set(false);
        mHomeButtonCoordinator.handlePageLoadFinished(NOT_NTP_URL);
        verifyIphShownWithFeed();
    }

    @Test
    public void testListMenu() {
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton).showMenu();
        assertEquals(1, mHomeButtonCoordinator.getMenuForTesting().size());
    }

    @Test
    public void testListMenuDisabled() {
        mIsHomeButtonMenuDisabled = true;
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton, never()).showMenu();
    }
}
