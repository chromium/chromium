// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link NavigationAttachmentsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsCoordinatorUnitTest {
    public @Rule ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock ComposeBoxQueryControllerBridge.Natives mControllerMock;
    private @Mock Profile mProfileMock;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private NavigationAttachmentsCoordinator mCoordinator;
    private ViewGroup mParent;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        ComposeBoxQueryControllerBridgeJni.setInstanceForTesting(mControllerMock);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mWindowAndroid = new WindowAndroid(activity, false);
                            mParent = new ConstraintLayout(activity);
                            mActivity.setContentView(mParent);
                            LayoutInflater.from(activity)
                                    .inflate(R.layout.navigation_attachments_bar, mParent, true);
                        });
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureEnabled() {
        mCoordinator =
                new NavigationAttachmentsCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mParent,
                        mProfileSupplier,
                        mLocationBarDataProvider);

        doReturn(PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(anyBoolean());

        mProfileSupplier.set(mProfileMock);
        View navigationToolbar = mParent.findViewById(R.id.location_bar_navigation_toolbar);
        assertEquals(View.GONE, navigationToolbar.getVisibility());

        mCoordinator.onUrlFocusChange(true);
        assertEquals(View.VISIBLE, navigationToolbar.getVisibility());

        mCoordinator.onUrlFocusChange(false);
        assertEquals(View.GONE, navigationToolbar.getVisibility());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testAdapter_isSet() {
        mCoordinator =
                new NavigationAttachmentsCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mParent,
                        mProfileSupplier,
                        mLocationBarDataProvider);
        NavigationAttachmentsViewHolder viewHolder = mCoordinator.getViewHolderForTesting();
        assertNotNull(viewHolder);
        assertNotNull(viewHolder.attachmentsView.getAdapter());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testToolbarVisibility_featureDisabled() {
        mCoordinator =
                new NavigationAttachmentsCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mParent,
                        mProfileSupplier,
                        mLocationBarDataProvider);
        assertNull(mCoordinator.getViewHolderForTesting());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testAddButton_togglesPopup() {
        mCoordinator =
                new NavigationAttachmentsCoordinator(
                        mActivity,
                        mWindowAndroid,
                        mParent,
                        mProfileSupplier,
                        mLocationBarDataProvider);
        NavigationAttachmentsViewHolder viewHolder = mCoordinator.getViewHolderForTesting();
        assertNotNull(viewHolder);
        View addButton = viewHolder.addButton;
        assertNotNull(addButton);
        NavigationAttachmentsPopup popup = viewHolder.popup;
        assertNotNull(popup);

        // Popup should not be showing initially.
        assertFalse(popup.isShowing());

        // Click the add button to show the popup.
        addButton.performClick();
        assertTrue(popup.isShowing());

        // Click the add button again to hide the popup.
        addButton.performClick();
        assertFalse(popup.isShowing());
    }
}
