// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.constraintlayout.widget.Group;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.omnibox.OmniboxFeatureList;

/** Unit tests for {@link NavigationAttachmentsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ViewGroup mParent;
    private @Mock Group mNavigationView;

    private Context mContext;
    private NavigationAttachmentsCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        doReturn(mNavigationView).when(mParent).findViewById(R.id.location_bar_navigation_toolbar);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void initialState_toolbarIsHidden() {
        mCoordinator = new NavigationAttachmentsCoordinator(mContext, mParent);
        verify(mNavigationView).setVisibility(View.GONE);
    }

    private void setUpCoordinatorAndClearInvocations() {
        mCoordinator = new NavigationAttachmentsCoordinator(mContext, mParent);
        clearInvocations(mNavigationView);
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void onUrlFocusChange_showsToolbarWhenFeatureEnabledAndFocused() {
        setUpCoordinatorAndClearInvocations();
        mCoordinator.onUrlFocusChange(true);
        verify(mNavigationView).setVisibility(View.VISIBLE);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void onUrlFocusChange_hidesToolbarWhenFeatureDisabled() {
        setUpCoordinatorAndClearInvocations();
        mCoordinator.onUrlFocusChange(true);
        verify(mNavigationView, times(0)).setVisibility(anyInt());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void onUrlFocusChange_hidesToolbarWhenNotFocused() {
        setUpCoordinatorAndClearInvocations();
        // Show it first
        mCoordinator.onUrlFocusChange(true);
        verify(mNavigationView).setVisibility(View.VISIBLE);
        clearInvocations(mNavigationView);

        // Then hide it
        mCoordinator.onUrlFocusChange(false);
        verify(mNavigationView).setVisibility(View.GONE);
    }
}
