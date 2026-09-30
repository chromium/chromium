// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.RecyclerViewSelectionController;

/** Tests for {@link ActionChipsView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsViewUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private static class TestActionChipsView extends ActionChipsView {
        int mSuperOnKeyDownCalls;

        TestActionChipsView(Context context) {
            super(context);
        }

        @Override
        public boolean superOnKeyDown(int keyCode, KeyEvent event) {
            mSuperOnKeyDownCalls++;
            return super.superOnKeyDown(keyCode, event);
        }
    }

    @Mock private RecyclerViewSelectionController mController;
    @Mock private View.OnClickListener mOnClickListener;

    private final View mChild = new View(ContextUtils.getApplicationContext());
    private final TestActionChipsView mView =
            new TestActionChipsView(ContextUtils.getApplicationContext());

    private void installAdapter() {
        mChild.setOnClickListener(mOnClickListener);
        mView.setSelectionControllerForTesting(mController);
        clearInvocations(mController);
        mView.mSuperOnKeyDownCalls = 0;
    }

    @Test
    public void keyDispatch_tabSelectsNextItem() {
        installAdapter();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB);

        doReturn(true).when(mController).selectNextItem();
        assertTrue(event.dispatch(mView));
        verify(mController).selectNextItem();
        verifyNoMoreInteractions(mController);
        clearInvocations(mController);

        doReturn(false).when(mController).selectNextItem();
        assertFalse(event.dispatch(mView));
        verify(mController).selectNextItem();
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void keyDispatch_shiftTabSelectsPreviousItem() {
        installAdapter();

        var event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_TAB,
                        0,
                        KeyEvent.META_SHIFT_ON);

        doReturn(true).when(mController).selectPreviousItem();
        assertTrue(event.dispatch(mView));
        verify(mController).selectPreviousItem();
        verifyNoMoreInteractions(mController);

        clearInvocations(mController);

        doReturn(false).when(mController).selectPreviousItem();
        assertFalse(event.dispatch(mView));
        verify(mController).selectPreviousItem();
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void keyDispatch_enterKeyPassedThroughWhenNoChipsSelected() {
        installAdapter();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertFalse(event.dispatch(mView));

        assertEquals(1, mView.mSuperOnKeyDownCalls);

        verify(mController).getSelectedView();
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void keyDispatch_enterKeyAcceptsSelectedChip() {
        installAdapter();

        doReturn(mChild).when(mController).getSelectedView();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        assertTrue(event.dispatch(mView));

        verify(mOnClickListener).onClick(mChild);
        assertEquals(0, mView.mSuperOnKeyDownCalls);

        verify(mController).getSelectedView();
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void keyDispatch_unhandledKeysArePassedToSuper() {
        installAdapter();

        var event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_T);
        assertFalse(event.dispatch(mView));

        assertEquals(1, mView.mSuperOnKeyDownCalls);

        verifyNoMoreInteractions(mController);
    }

    @Test
    public void selection_doesNothingWhenNoAdapter() {
        mView.setSelected(true);
        verifyNoMoreInteractions(mController);

        mView.setSelected(false);
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void selection_resetsCarouselSelectionWhenSelected() {
        installAdapter();

        mView.setSelected(true);
        verify(mController).reset();
        verifyNoMoreInteractions(mController);
    }

    @Test
    public void selection_resetsCarouselSelectionWhenDeselected() {
        installAdapter();

        mView.setSelected(false);
        verify(mController).reset();
        verifyNoMoreInteractions(mController);
    }
}
