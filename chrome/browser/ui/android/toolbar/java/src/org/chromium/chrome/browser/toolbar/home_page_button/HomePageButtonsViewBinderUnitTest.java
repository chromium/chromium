// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_BACKGROUND;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_DATA;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_TINT_LIST;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_CONTAINER_VISIBLE;

import android.content.res.ColorStateList;
import android.view.View;

import androidx.core.util.Pair;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link HomePageButtonsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class HomePageButtonsViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomePageButtonsContainerView mHomePageButtonsContainerView;
    @Mock private HomePageButtonData mHomeButtonData;
    @Mock private HomePageButtonData mNtpCustomizationButtonData;
    @Mock public ColorStateList mThemeColorStateList;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() {
        mModel = new PropertyModel(HomePageButtonsProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mHomePageButtonsContainerView, HomePageButtonsViewBinder::bind);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mModel = null;
    }

    @Test
    public void testSetHomePageButtonsContainerVisibility() {
        mModel.set(IS_CONTAINER_VISIBLE, true);
        verify(mHomePageButtonsContainerView).setVisibility(View.VISIBLE);

        mModel.set(IS_CONTAINER_VISIBLE, false);
        verify(mHomePageButtonsContainerView).setVisibility(View.GONE);
    }

    @Test
    public void testSetHomePageButtonVisibility() {
        mModel.set(IS_BUTTON_VISIBLE, new Pair<>(0, true));
        verify(mHomePageButtonsContainerView).setButtonVisibility(0, true);
        mModel.set(IS_BUTTON_VISIBLE, new Pair<>(0, false));
        verify(mHomePageButtonsContainerView).setButtonVisibility(0, false);

        mModel.set(IS_BUTTON_VISIBLE, new Pair<>(1, true));
        verify(mHomePageButtonsContainerView).setButtonVisibility(1, true);
        mModel.set(IS_BUTTON_VISIBLE, new Pair<>(1, false));
        verify(mHomePageButtonsContainerView).setButtonVisibility(1, false);
    }

    @Test
    public void testUpdateButtonData() {
        mModel.set(BUTTON_DATA, new Pair<>(0, mHomeButtonData));
        verify(mHomePageButtonsContainerView).updateButtonData(0, mHomeButtonData);
        mModel.set(BUTTON_DATA, new Pair<>(1, mNtpCustomizationButtonData));
        verify(mHomePageButtonsContainerView).updateButtonData(1, mNtpCustomizationButtonData);
    }

    @Test
    public void testSetButtonBackgroundResource() {
        mModel.set(BUTTON_BACKGROUND, R.drawable.default_icon_background_baseline);
        verify(mHomePageButtonsContainerView)
                .setButtonBackgroundResource(R.drawable.default_icon_background_baseline);
    }

    @Test
    public void testSetButtonColorStateList() {
        mModel.set(BUTTON_TINT_LIST, mThemeColorStateList);
        verify(mHomePageButtonsContainerView).setColorStateList(mThemeColorStateList);
    }
}
