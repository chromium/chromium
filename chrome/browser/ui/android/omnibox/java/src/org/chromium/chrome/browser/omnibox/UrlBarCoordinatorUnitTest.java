// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** Unit tests for {@link UrlBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlBarCoordinatorUnitTest {
    private static class TestUrlBar extends UrlBarApi26 {
        private boolean mCursorVisible;
        private Callback<Boolean> mTextWrappingChangeListener;

        TestUrlBar(Context context) {
            super(context, null);
        }

        @Override
        public void setCursorVisible(boolean visible) {
            mCursorVisible = visible;
            super.setCursorVisible(visible);
        }

        @Override
        public void setUrlTextWrappingChangeListener(Callback<Boolean> listener) {
            mTextWrappingChangeListener = listener;
            super.setUrlTextWrappingChangeListener(listener);
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private TestUrlBar mUrlBar;
    @Mock private UrlBarDelegate mDelegate;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private Callback<UrlBarFocusChangeInfo> mFocusChangeCallback;

    private Context mContext;
    private UrlBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        OmniboxFeatures.setDebounceKeyboardVisibilityForTesting(true);
        OmniboxResourceProvider.setUrlBarPrimaryTextColorForTesting(Color.LTGRAY);
        OmniboxResourceProvider.setUrlBarHintTextColorForTesting(Color.LTGRAY);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mContext = activity;
        mUrlBar = new TestUrlBar(activity);
        activity.setContentView(mUrlBar);
        doReturn(false).when(mKeyboardVisibilityDelegate).isKeyboardShowing(mUrlBar);
        mCoordinator =
                new UrlBarCoordinator(
                        mContext,
                        mUrlBar,
                        /* actionModeCallback= */ null,
                        mFocusChangeCallback,
                        mDelegate,
                        mKeyboardVisibilityDelegate,
                        /* isIncognitoBranded= */ false,
                        /* onLongClickListener= */ null,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null,
                        /* keyDownListener= */ null);
    }

    @After
    public void tearDown() {
        OmniboxFeatures.setDebounceKeyboardVisibilityForTesting(null);
    }

    @Test
    public void setKeyboardVisibility_flagDisabled_usesOriginalLogic() {
        OmniboxFeatures.setDebounceKeyboardVisibilityForTesting(false);

        // Show keyboard is called immediately
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        verify(mKeyboardVisibilityDelegate).showKeyboard(mUrlBar);

        // Hide keyboard with delay schedules mKeyboardHideTask
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ true);
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_showFromHidden_schedulesDebounce() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);

        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());

        // When runnable fires, keyboard is shown
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).showKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_showWhenAlreadyShowingOrShown_noOp() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);

        // Subsequent show requests while SHOWING are no-ops
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).showKeyboard(mUrlBar);
        clearInvocations(mKeyboardVisibilityDelegate);

        // When confirmed SHOWN, show requests are still no-ops
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());
    }

    @Test
    public void setKeyboardVisibility_hideWhileShowing_cancelsDebounceWithoutCallingHide() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);

        // Hide requested while show is still pending: cancels without scheduling hide
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());
    }

    @Test
    public void setKeyboardVisibility_hideFromShown_schedulesDebounce() {
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);

        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());

        // When runnable fires, keyboard is hidden
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_hideWhenAlreadyHidingOrHidden_noOp() {
        // Initial state is HIDDEN -> hide is no-op
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());

        // Move to SHOWN
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);

        // First hide schedules debounce; second hide while HIDING is a no-op
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_showWhileHiding_cancelsHideWithoutCallingShow() {
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);

        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
    }

    @Test
    public void keyboardVisibilityChanged_updatesCursorAndCancelsPending() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);

        // OS notifies that keyboard showed
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());
        assertTrue(mUrlBar.mCursorVisible);

        // OS notifies that keyboard hid
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ false);
        assertFalse(mUrlBar.mCursorVisible);
    }

    @Test
    public void setKeyboardVisibility_hideAfterShowRunnableFiresBeforeOsCallback_schedulesHide() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).showKeyboard(mUrlBar);

        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);

        // Late OS show callback must not cancel the pending hide runnable.
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_hideWhenStateHiddenButDelegateReportsShowing_schedulesHide() {
        // State is HIDDEN, but the OS soft keyboard is still showing (e.g. after a transient
        // inset dip or focus transfer).
        doReturn(true).when(mKeyboardVisibilityDelegate).isKeyboardShowing(mUrlBar);

        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void testSelectAllText_delegates() {
        mUrlBar.setText("test");
        mCoordinator.selectAllText();
        assertEquals(0, mUrlBar.getSelectionStart());
        assertEquals(4, mUrlBar.getSelectionEnd());
    }

    @Test
    public void testUrlTextWrappingSupplier() {
        var supplier = mCoordinator.getUrlTextWrappingSupplier();
        assertFalse(supplier.get());
        assertFalse(mCoordinator.isTextWrapped());

        assertNotNull(mUrlBar.mTextWrappingChangeListener);

        mUrlBar.mTextWrappingChangeListener.onResult(true);
        assertTrue(supplier.get());
        assertTrue(mCoordinator.isTextWrapped());

        mUrlBar.mTextWrappingChangeListener.onResult(false);
        assertFalse(supplier.get());
        assertFalse(mCoordinator.isTextWrapped());
    }
}
