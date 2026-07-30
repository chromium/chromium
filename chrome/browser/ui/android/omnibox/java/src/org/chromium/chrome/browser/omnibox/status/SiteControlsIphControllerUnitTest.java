// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

/** Unit tests for {@link SiteControlsIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SiteControlsIphControllerUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private View mAnchorView;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private SiteControlsIphController mController;

    @Before
    public void setUp() {
        doReturn(true).when(mAnchorView).isShown();
        mController =
                new SiteControlsIphController(mUserEducationHelper, mAnchorView, mAppMenuHandler);
    }

    @Test
    public void testShowIph() {
        mController.showIph();

        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.info_menu_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }

    @Test
    public void testShowIph_nullAnchorView() {
        SiteControlsIphController controller =
                new SiteControlsIphController(mUserEducationHelper, (View) null, mAppMenuHandler);
        controller.showIph();

        verifyNoInteractions(mUserEducationHelper);
    }

    @Test
    public void testShowIph_anchorViewNotShown() {
        doReturn(false).when(mAnchorView).isShown();
        mController.showIph();

        verifyNoInteractions(mUserEducationHelper);
    }
}
