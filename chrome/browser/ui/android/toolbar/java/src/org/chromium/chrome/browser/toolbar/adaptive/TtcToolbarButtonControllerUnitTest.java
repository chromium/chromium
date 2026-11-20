// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
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
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.ttc.TtcEntryPoint;

/** Unit tests for {@link TtcToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TtcToolbarButtonControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private TtcEntryPoint mEntryPoint;
    @Mock private ButtonDataObserver mButtonDataObserver;
    @Captor private ArgumentCaptor<TtcEntryPoint.Observer> mEntryPointObserverCaptor;

    private Context mContext;
    private TtcToolbarButtonController mController;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        mController = new TtcToolbarButtonController(mContext, () -> mTab, mEntryPoint);
        verify(mEntryPoint).addObserver(mEntryPointObserverCaptor.capture());
        mController.addObserver(mButtonDataObserver);
    }

    @Test
    public void testButtonIsTheTtcShortcut() {
        ButtonData.ButtonSpec spec = mController.get(mTab).getButtonSpec();
        assertEquals(AdaptiveToolbarButtonVariant.TTC, spec.getButtonVariant());
        assertEquals(
                mContext.getString(R.string.ttc_entrypoint_label), spec.getContentDescription());
    }

    @Test
    public void testShowsWhenTtcSessionsCanRun() {
        when(mEntryPoint.shouldShow()).thenReturn(true);
        assertTrue(mController.get(mTab).canShow());

        when(mEntryPoint.shouldShow()).thenReturn(false);
        assertFalse(mController.get(mTab).canShow());
    }

    @Test
    public void testHiddenWithoutTab() {
        when(mEntryPoint.shouldShow()).thenReturn(true);
        assertFalse(mController.get(/* tab= */ null).canShow());
    }

    @Test
    public void testHiddenOnIncognitoTabs() {
        when(mEntryPoint.shouldShow()).thenReturn(true);
        when(mTab.isIncognito()).thenReturn(true);
        assertFalse(mController.get(mTab).canShow());
    }

    @Test
    public void testEntryPointChangesNotifyObservers() {
        when(mEntryPoint.shouldShow()).thenReturn(true);
        mEntryPointObserverCaptor.getValue().onEntryPointStateChanged();
        verify(mButtonDataObserver).buttonDataChanged(true);

        when(mEntryPoint.shouldShow()).thenReturn(false);
        mEntryPointObserverCaptor.getValue().onEntryPointStateChanged();
        verify(mButtonDataObserver).buttonDataChanged(false);
    }

    @Test
    public void testClickTogglesSession() {
        when(mEntryPoint.shouldShow()).thenReturn(true);
        mController.get(mTab).getButtonSpec().getOnClickListener().onClick(new View(mContext));
        verify(mEntryPoint).toggleSession();
    }

    @Test
    public void testDestroyStopsObservingEntryPoint() {
        mController.destroy();
        verify(mEntryPoint).removeObserver(mEntryPointObserverCaptor.getValue());
        // Observers were cleared, so later entry point changes aren't forwarded.
        mEntryPointObserverCaptor.getValue().onEntryPointStateChanged();
        verify(mButtonDataObserver, never()).buttonDataChanged(anyBoolean());
    }
}
