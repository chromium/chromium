// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.ACCESSIBILITY_TRAVERSAL_BEFORE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_BACKGROUND;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_DATA;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.BUTTON_TINT_LIST;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.CONTAINER_VISIBILITY;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.IS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.ON_KEY_LISTENER;
import static org.chromium.chrome.browser.toolbar.home_page_button.HomePageButtonsProperties.TRANSLATION_Y;

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
    @Mock private View.OnKeyListener mOnKeyListener;

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
    public void testSetContainerVisibility() {
        mModel.set(CONTAINER_VISIBILITY, View.VISIBLE);
        verify(mHomePageButtonsContainerView).setVisibility(View.VISIBLE);

        mModel.set(CONTAINER_VISIBILITY, View.GONE);
        verify(mHomePageButtonsContainerView).setVisibility(View.GONE);

        mModel.set(CONTAINER_VISIBILITY, View.INVISIBLE);
        verify(mHomePageButtonsContainerView).setVisibility(View.INVISIBLE);
    }

    @Test
    public void testSetAccessibilityTraversalBefore() {
        int testViewId = R.id.url_bar;
        mModel.set(ACCESSIBILITY_TRAVERSAL_BEFORE, testViewId);
        verify(mHomePageButtonsContainerView).setAccessibilityTraversalBefore(testViewId);
    }

    @Test
    public void testSetTranslationY() {
        float testTranslationY = 10.5f;
        mModel.set(TRANSLATION_Y, testTranslationY);
        verify(mHomePageButtonsContainerView).setTranslationY(testTranslationY);

        float testTranslationY2 = 0.0f;
        mModel.set(TRANSLATION_Y, testTranslationY2);
        verify(mHomePageButtonsContainerView).setTranslationY(testTranslationY2);
    }

    @Test
    public void testSetClickable() {
        mModel.set(IS_CLICKABLE, true);
        verify(mHomePageButtonsContainerView).setClickable(true);

        mModel.set(IS_CLICKABLE, false);
        verify(mHomePageButtonsContainerView).setClickable(false);
    }

    @Test
    public void testSetOnKeyListener() {
        mModel.set(ON_KEY_LISTENER, mOnKeyListener);
        verify(mHomePageButtonsContainerView).setOnKeyListener(mOnKeyListener);
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
