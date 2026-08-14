// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.AimLaunchResult;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link AiModeActionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AiModeActionCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActionRegistry mActionRegistry;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private View mView;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Tracker mTracker;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mAiModeActionModelSupplier =
            ObservableSuppliers.createNullable();

    private Activity mActivity;
    private AiModeActionCoordinator mCoordinator;
    private PropertyModel mAiModeActionModel;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        TrackerFactory.setTrackerForTests(mTracker);
        mUserActionTester = new UserActionTester();

        mAiModeActionModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        when(mActionRegistry.get(ActionId.AI_MODE)).thenReturn(mAiModeActionModelSupplier);
        when(mTab.getProfile()).thenReturn(mProfile);
        mTabSupplier.set(mTab);

        mCoordinator =
                new AiModeActionCoordinator(
                        mActivity, mActionRegistry, mTabSupplier, mUserEducationHelper);
    }

    @After
    public void tearDown() {
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        if (mUserActionTester != null) {
            mUserActionTester.tearDown();
        }
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    @Test
    public void testOnModelChanged_bindsOnPressCallback() {
        assertNull(mAiModeActionModel.get(ActionProperties.ON_PRESS_CALLBACK));

        // Trigger model supplier change.
        mAiModeActionModelSupplier.set(mAiModeActionModel);

        // Verify callback is bound.
        Callback<View> onPressCallback = mAiModeActionModel.get(ActionProperties.ON_PRESS_CALLBACK);
        assertNotNull(onPressCallback);

        // Verify UserEducationHelper is set.
        assertEquals(
                mUserEducationHelper,
                mAiModeActionModel.get(ActionProperties.USER_EDUCATION_HELPER));
    }

    @Test
    public void testOnAiModePressed_loadsComposeUrl_recordsSuccess() {
        GURL composeUrl = JUnitTestGURLs.EXAMPLE_URL;
        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(composeUrl);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.Aim.LaunchResult",
                        BottomBarMetrics.AimLaunchResult.SUCCESS);

        // Bind model.
        mAiModeActionModelSupplier.set(mAiModeActionModel);
        Callback<View> onPressCallback = mAiModeActionModel.get(ActionProperties.ON_PRESS_CALLBACK);

        // Trigger press.
        onPressCallback.onResult(mView);

        // Verify URL is loaded in the tab.
        verify(mTab).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(composeUrl.getSpec(), mLoadUrlParamsCaptor.getValue().getUrl());

        // Verify tracker event and user action.
        verify(mTracker).notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_AIM_USED);
        assertTrue(mUserActionTester.getActions().contains("MobileBottomBar.Aim.Clicked"));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnAiModePressed_noopWhenUrlInvalid_recordsMissingOrInvalidUrl() {
        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(GURL.emptyGURL());

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.Aim.LaunchResult",
                        AimLaunchResult.MISSING_OR_INVALID_URL);

        // Bind model.
        mAiModeActionModelSupplier.set(mAiModeActionModel);
        Callback<View> onPressCallback = mAiModeActionModel.get(ActionProperties.ON_PRESS_CALLBACK);

        // Trigger press.
        onPressCallback.onResult(mView);

        // Verify no URL is loaded.
        verify(mTab, never()).loadUrl(any());
        verify(mTracker).notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_AIM_USED);
        assertTrue(mUserActionTester.getActions().contains("MobileBottomBar.Aim.Clicked"));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnAiModePressed_nullTab_recordsTabNull() {
        mTabSupplier.set(null);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.Aim.LaunchResult", AimLaunchResult.TAB_NULL);

        // Bind model.
        mAiModeActionModelSupplier.set(mAiModeActionModel);
        Callback<View> onPressCallback = mAiModeActionModel.get(ActionProperties.ON_PRESS_CALLBACK);

        // Trigger press.
        onPressCallback.onResult(mView);

        verify(mTab, never()).loadUrl(any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy_removesObserver() {
        mCoordinator.destroy();

        // Triggering model change after destroy should not bind the callback.
        mAiModeActionModelSupplier.set(mAiModeActionModel);
        assertNull(mAiModeActionModel.get(ActionProperties.ON_PRESS_CALLBACK));
    }
}
