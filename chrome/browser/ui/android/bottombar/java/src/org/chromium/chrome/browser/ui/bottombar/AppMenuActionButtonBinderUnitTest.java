// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.ImageButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.AppMenuActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.MenuButtonState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link AppMenuActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppMenuActionButtonBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomBarAppMenu mView;
    @Mock private ImageButton mInnerButton;
    @Mock private MenuButtonState mMenuButtonState;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        when(mView.getImageButton()).thenReturn(mInnerButton);
        mModel = new PropertyModel.Builder(AppMenuActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, AppMenuActionButtonBinder::bind);
    }

    @Test
    public void testShowUpdateBadge() {
        mModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, true);
        verify(mView).setAppMenuUpdateBadgeVisible(true);

        mModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, false);
        verify(mView).setAppMenuUpdateBadgeVisible(false);
    }

    @Test
    public void testUpdateBadgeButtonState() {
        mModel.set(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE, mMenuButtonState);
        verify(mView).setBadgeUpdateState(mMenuButtonState);
    }

    @Test
    public void testDelegateToImageButton() {
        mModel.set(ActionProperties.ICON_ID, 123);
        verify(mInnerButton).setImageResource(123);
    }

    @Test
    public void testFallbackToActionButtonBinder() {
        mModel.set(ActionProperties.IS_SELECTED, true);
        verify(mView).setSelected(true);
    }
}
