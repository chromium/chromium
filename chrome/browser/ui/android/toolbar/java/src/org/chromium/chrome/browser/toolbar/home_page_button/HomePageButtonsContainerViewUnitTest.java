// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_page_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link HomePageButtonsContainerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomePageButtonsContainerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HomePageButtonData mHomeButtonData;
    @Mock private HomePageButtonData mNtpCustomizationButtonData;
    @Mock private HomePageButtonView mHomeButtonView;
    @Mock private HomePageButtonView mNtpCustomizationButtonView;

    private HomePageButtonsContainerView mHomePageButtonsContainerView;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mHomePageButtonsContainerView =
                (HomePageButtonsContainerView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.home_page_buttons_layout, null, false);
        mHomePageButtonsContainerView.setHomePageButtonsListForTesting(
                Collections.unmodifiableList(
                        Arrays.asList(mHomeButtonView, mNtpCustomizationButtonView)));
    }

    @Test
    public void testSetButtonVisibility() {
        mHomePageButtonsContainerView.setButtonVisibility(
                /* buttonIndex= */ 0, /* visible= */ true);
        verify(mHomeButtonView).setVisibility(true);
        mHomePageButtonsContainerView.setButtonVisibility(
                /* buttonIndex= */ 0, /* visible= */ false);
        verify(mHomeButtonView).setVisibility(false);

        mHomePageButtonsContainerView.setButtonVisibility(
                /* buttonIndex= */ 1, /* visible= */ true);
        verify(mNtpCustomizationButtonView).setVisibility(true);
        mHomePageButtonsContainerView.setButtonVisibility(
                /* buttonIndex= */ 1, /* visible= */ false);
        verify(mNtpCustomizationButtonView).setVisibility(false);
    }

    @Test
    public void testUpdateButtonData() {
        mHomePageButtonsContainerView.updateButtonData(/* buttonIndex= */ 0, mHomeButtonData);
        verify(mHomeButtonView).updateButtonData(mHomeButtonData);

        mHomePageButtonsContainerView.updateButtonData(
                /* buttonIndex= */ 1, mNtpCustomizationButtonData);
        verify(mNtpCustomizationButtonView).updateButtonData(mNtpCustomizationButtonData);
    }

    @Test
    public void testSetButtonBackgroundResource() {
        mHomePageButtonsContainerView.setButtonBackgroundResource(
                R.drawable.default_icon_background);
        verify(mHomeButtonView).setBackgroundResource(R.drawable.default_icon_background);
        verify(mNtpCustomizationButtonView)
                .setBackgroundResource(R.drawable.default_icon_background);
    }

    @Test
    public void testGetButtonByIndex() {
        assertEquals(
                mHomeButtonView,
                mHomePageButtonsContainerView.getButtonByIndex(/* buttonIndex= */ 0));
        assertEquals(
                mNtpCustomizationButtonView,
                mHomePageButtonsContainerView.getButtonByIndex(/* buttonIndex= */ 1));
    }
}
