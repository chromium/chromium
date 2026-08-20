// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;

import org.junit.After;
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
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** Unit tests for {@link UrlBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlBarCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private UrlBar mUrlBar;
    private @Mock UrlBarDelegate mDelegate;
    private @Mock KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private @Mock Callback<UrlBarFocusChangeInfo> mFocusChangeCallback;
    private @Captor ArgumentCaptor<Runnable> mRunnableCaptor;

    private Context mContext;
    private UrlBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        OmniboxFeatures.setDebounceKeyboardVisibilityForTesting(true);
        OmniboxResourceProvider.setUrlBarPrimaryTextColorForTesting(Color.LTGRAY);
        OmniboxResourceProvider.setUrlBarHintTextColorForTesting(Color.LTGRAY);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mContext = activity;
        mUrlBar = spy(new UrlBarApi26(activity, null));
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
        verify(mUrlBar).postDelayed(mRunnableCaptor.capture(), eq(150L));
        mRunnableCaptor.getValue().run();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_showFromHidden_schedulesDebounce() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);

        verify(mUrlBar).postDelayed(mRunnableCaptor.capture(), eq(150L));
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());

        // When runnable fires, keyboard is shown
        mRunnableCaptor.getValue().run();
        verify(mKeyboardVisibilityDelegate).showKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_showWhenAlreadyShowingOrShown_noOp() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        verify(mUrlBar, times(1)).postDelayed(any(), eq(150L));

        // Subsequent show requests while SHOWING are no-ops
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        verify(mUrlBar, times(1)).postDelayed(any(), eq(150L));

        // When confirmed SHOWN, show requests are still no-ops
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        verify(mUrlBar, times(1)).postDelayed(any(), eq(150L));
    }

    @Test
    public void setKeyboardVisibility_hideWhileShowing_cancelsDebounceWithoutCallingHide() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        verify(mUrlBar).postDelayed(mRunnableCaptor.capture(), eq(150L));

        // Hide requested while show is still pending: cancels without scheduling hide
        clearInvocations(mUrlBar);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);

        verify(mUrlBar).removeCallbacks(mRunnableCaptor.getValue());
        verify(mUrlBar, never()).postDelayed(any(), anyLong());
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());
    }

    @Test
    public void setKeyboardVisibility_hideFromShown_schedulesDebounce() {
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);

        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        verify(mUrlBar).postDelayed(mRunnableCaptor.capture(), eq(150L));
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());

        // When runnable fires, keyboard is hidden
        mRunnableCaptor.getValue().run();
        verify(mKeyboardVisibilityDelegate).hideKeyboard(mUrlBar);
    }

    @Test
    public void setKeyboardVisibility_hideWhenAlreadyHidingOrHidden_noOp() {
        // Initial state is HIDDEN -> hide is no-op
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        verify(mUrlBar, never()).postDelayed(any(), anyLong());

        // Move to SHOWN
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);

        // First hide schedules debounce
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        verify(mUrlBar, times(1)).postDelayed(any(), eq(150L));

        // Second hide while HIDING is a no-op
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);
        verify(mUrlBar, times(1)).postDelayed(any(), eq(150L));
    }

    @Test
    public void setKeyboardVisibility_showWhileHiding_cancelsHideWithoutCallingShow() {
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ false, /* shouldDelayHiding= */ false);

        verify(mUrlBar).postDelayed(mRunnableCaptor.capture(), eq(150L));
        Runnable hideRunnable = mRunnableCaptor.getValue();

        clearInvocations(mUrlBar);
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);

        verify(mUrlBar).removeCallbacks(hideRunnable);
        verify(mUrlBar, never()).postDelayed(any(), anyLong());
        verify(mKeyboardVisibilityDelegate, never()).showKeyboard(any());
        verify(mKeyboardVisibilityDelegate, never()).hideKeyboard(any());
    }

    @Test
    public void keyboardVisibilityChanged_updatesCursorAndCancelsPending() {
        mCoordinator.setKeyboardVisibility(
                /* showKeyboard= */ true, /* shouldDelayHiding= */ false);
        verify(mUrlBar).postDelayed(mRunnableCaptor.capture(), eq(150L));

        clearInvocations(mUrlBar);
        // OS notifies that keyboard showed
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ true);
        verify(mUrlBar).removeCallbacks(mRunnableCaptor.getValue());
        verify(mUrlBar).setCursorVisible(true);

        // OS notifies that keyboard hid
        mCoordinator.keyboardVisibilityChanged(/* isKeyboardShowing= */ false);
        verify(mUrlBar).setCursorVisible(false);
    }
}
