// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.loading;

import static org.mockito.ArgumentMatchers.eq;
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
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link LoadingFullscreenCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LoadingFullscreenCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ScrimManager mScrimManager;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelArgumentCaptor;

    private Activity mActivity;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivity.getLayoutInflater().inflate(R.layout.loading_fullscreen, null);
    }

    @Test
    public void testShowLoading() {
        LoadingFullscreenCoordinator loadingCoordinator =
                new LoadingFullscreenCoordinator(
                        mActivity,
                        mScrimManager,
                        mActivity.findViewById(R.id.loading_fullscreen_container));

        loadingCoordinator.startLoading(
                () -> {
                    loadingCoordinator.closeLoadingScreen();
                });
        verify(mScrimManager).showScrim(mPropertyModelArgumentCaptor.capture());
        PropertyModel propertyModel = mPropertyModelArgumentCaptor.getValue();
        Runnable closeScrimRunnable = propertyModel.get(ScrimProperties.CLICK_DELEGATE);

        closeScrimRunnable.run();
        verify(mScrimManager).hideScrim(eq(propertyModel), eq(true));
    }
}
