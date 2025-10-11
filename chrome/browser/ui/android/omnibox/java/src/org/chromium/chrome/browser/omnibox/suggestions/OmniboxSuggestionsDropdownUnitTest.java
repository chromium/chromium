// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.test.R;

/** Unit tests for {@link OmniboxSuggestionsDropdown}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 29)
public class OmniboxSuggestionsDropdownUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Runnable mDropdownScrollListener;
    private @Mock Runnable mDropdownScrollToTopListener;
    private @Mock OmniboxSuggestionsDropdownAdapter mAdapter;
    private @Mock View mView;

    private Context mContext;
    private OmniboxSuggestionsDropdown mDropdown;
    private OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener mListener;
    private FrameLayout.LayoutParams mLayoutParams;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mListener = spy(new OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener(mContext));
        when(mListener.getItemCount()).thenReturn(3);
        when(mListener.findViewByPosition(anyInt())).thenReturn(mView);
        when(mView.isFocusable()).thenReturn(true);
        mDropdown = spy(new OmniboxSuggestionsDropdown(mContext, null, mListener));
        mDropdown.setId(R.id.omnibox_suggestions_dropdown);
        mDropdown.setAdapter(mAdapter);

        mLayoutParams = new FrameLayout.LayoutParams(0, 0);
        mDropdown.setLayoutParams(mLayoutParams);
    }

    @After
    public void tearDown() {
        mListener.resetScrollState();
    }

    @Test
    public void testScrollListener_keyboardShouldDismissOnScrollAttemptFromTop() {
        mListener.setSuggestionDropdownScrollListener(mDropdownScrollListener);

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Subsequent scroll events should pass through.
        // Keyboard should not be dismissed again.
        assertEquals(5, mListener.updateKeyboardVisibilityAndScroll(5, 10));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    public void testScrollListener_keyboardShouldDismissOnScrollAttemptFromScrolledList() {
        mListener.setSuggestionDropdownScrollListener(mDropdownScrollListener);

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Subsequent scroll events should pass through.
        // Keyboard should not be dismissed again.
        assertEquals(5, mListener.updateKeyboardVisibilityAndScroll(5, 10));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    public void testScrollListener_keyboardShouldShowOnScrollToTop() {
        mListener.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        mListener.setSuggestionDropdownOverscrolledToTopListener(mDropdownScrollToTopListener);

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Pretend we scroll up, while keyboard is hidden.
        assertEquals(-5, mListener.updateKeyboardVisibilityAndScroll(-5, -5));
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Overscroll to top. This is part of the same gesture.
        // Expect to see keyboard state unchanged.
        assertEquals(-5, mListener.updateKeyboardVisibilityAndScroll(-5, -10));
        verifyNoMoreInteractions(mDropdownScrollToTopListener);

        // Overscroll to top again, but this time as a new gesture.
        mListener.onNewGesture();
        assertEquals(-5, mListener.updateKeyboardVisibilityAndScroll(-5, -10));
        verify(mDropdownScrollToTopListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollToTopListener);

        // Overscroll again. Make sure we don't call the keyboard up again.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, -5));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    public void testScrollListener_dismissingKeyboardWhenScrollDoesNotHappen() {
        // In some cases the list may be long enough to stretch below the keyboard, but not long
        // enough to be scrollable. We want to dismiss the keyboard in these cases, too.
        mListener.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        mListener.setSuggestionDropdownOverscrolledToTopListener(mDropdownScrollToTopListener);

        // Pretend we're scrolling down (delta=10) but there is no content to move to (scroll=0).
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, 10));
        // Confirm that we're hiding the keyboard.
        verify(mDropdownScrollListener).run();

        // Simulate scroll up as part of the same gesture. Observe that no events are emitted.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, -10));
        verifyNoMoreInteractions(mDropdownScrollToTopListener);

        // Begin a new gesture.
        // Pretend we're scrolling up now (delta=-10) but we're already on top and can't move.
        mListener.onNewGesture();
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, -10));
        // Confirm that we're not trying to show the keyboard.
        verify(mDropdownScrollToTopListener).run();

        verifyNoMoreInteractions(mDropdownScrollListener, mDropdownScrollToTopListener);
    }

    @Test
    public void testScrollListener_dismissingKeyboardWhenTheListIsOnlyBarelyUnderTheKeyboard() {
        mListener.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        mListener.setSuggestionDropdownOverscrolledToTopListener(mDropdownScrollToTopListener);

        // We want to scroll by 10px, but there's only 1px of slack. This means the suggestions list
        // spans entirely under the keyboard. Hide the keyboard.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(1, 10));
        verify(mDropdownScrollListener).run();

        // Expect no more events emitted during the same gesture.
        assertEquals(-9, mListener.updateKeyboardVisibilityAndScroll(-9, -10));
        verifyNoMoreInteractions(mDropdownScrollToTopListener);

        // Reset keyboard state as part of the new gesture.
        mListener.onNewGesture();
        assertEquals(-9, mListener.updateKeyboardVisibilityAndScroll(-9, -10));
        verify(mDropdownScrollToTopListener).run();

        verifyNoMoreInteractions(mDropdownScrollListener, mDropdownScrollToTopListener);
    }

    @Test
    public void testScrollListener_reemitsKeyboardDismissOnReset() {
        mListener.setSuggestionDropdownScrollListener(mDropdownScrollListener);

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        // This time the scroll happens, even if just by one pixel.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Simulate lists being shown again.
        mListener.resetScrollState();

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        // Condition: the list is long enough that the scroll distance equals to delta.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(2)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    public void testScrollListener_inactiveWhenObserverNotEquipped() {
        // Note: do not equip the listeners (no calls to setSuggestionDropdownScrollListener() and
        // setSuggestionDropdownOverscrolledToTopListener).
        // None of the calls below should invoke listeners (and crash).
        // Scroll down from top.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        // Scroll down from the middle. Confirm new scroll position is accepted.
        assertEquals(10, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        // Overscroll to top.
        assertEquals(-10, mListener.updateKeyboardVisibilityAndScroll(-10, -10));
        verifyNoMoreInteractions(mDropdownScrollListener);
        verifyNoMoreInteractions(mDropdownScrollToTopListener);
    }

    @Test
    public void translateChildrenVertical() {
        View childView = Mockito.mock(View.class);

        mDropdown.translateChildrenVertical(45.6f);
        mDropdown.onChildAttachedToWindow(childView);
        verify(childView).setTranslationY(45.6f);

        mDropdown.onChildDetachedFromWindow(childView);
        verify(childView).setTranslationY(0.0f);
    }

    @Test
    public void setChildAlpha() {
        View childView = Mockito.mock(View.class);

        mDropdown.setChildAlpha(0.6f);
        mDropdown.onChildAttachedToWindow(childView);
        verify(childView).setAlpha(0.6f);

        mDropdown.onChildDetachedFromWindow(childView);
        verify(childView).setAlpha(1.0f);
    }

    @Test
    public void setShouldClipToOutline_clipsOutlineWhenSet() {
        var dropdown = new OmniboxSuggestionsDropdown(mContext, null);
        dropdown.setShouldClipToOutline(true);
        assertTrue(dropdown.getClipToOutline());
        assertNotNull(dropdown.getOutlineProvider());
    }

    @Test
    public void setShouldClipToOutline_doesNotClipOutlineWhenUnset() {
        var dropdown = new OmniboxSuggestionsDropdown(mContext, null);
        dropdown.setShouldClipToOutline(false);
        assertFalse(dropdown.getClipToOutline());
        assertNull(dropdown.getOutlineProvider());
    }

    @Test
    public void onSizeChanged_callsUpdateVisualScrollState() {
        mDropdown.onSizeChanged(1, 2, 3, 4);
        verify(mListener).updateVisualScrollState();
    }

    @Test
    public void updateVisualScrollState_atTop_scrolls() {
        mListener.updateVisualScrollState();
        verify(mListener).postOnAnimation(any());
    }

    @Test
    public void updateVisualScrollState_notAtTop_doesNotScroll() {
        // Scroll down to move away from the top.
        mListener.updateKeyboardVisibilityAndScroll(10, 10);
        Mockito.clearInvocations(mListener);

        mListener.updateVisualScrollState();
        verify(mListener, times(0)).postOnAnimation(any());
    }

    @Test
    public void testToolbarPosition() {
        // Feature OFF, Toolbar at the TOP.
        ChromeFeatureList.sAndroidBottomToolbarV2ReverseOrderSuggestionsList.setForTesting(false);
        mDropdown.setToolbarPosition(ControlsPosition.TOP);
        assertTrue(mDropdown.getToolbarOnTopForTesting());
        assertEquals(Gravity.TOP, mLayoutParams.gravity);

        // Feature OFF, Toolbar at the BOTTOM.
        mDropdown.setToolbarPosition(ControlsPosition.BOTTOM);
        assertTrue(mDropdown.getToolbarOnTopForTesting());
        assertEquals(Gravity.TOP, mLayoutParams.gravity);

        // Feature ON, Toolbar at the TOP.
        ChromeFeatureList.sAndroidBottomToolbarV2ReverseOrderSuggestionsList.setForTesting(true);
        mDropdown.setToolbarPosition(ControlsPosition.TOP);
        assertTrue(mDropdown.getToolbarOnTopForTesting());
        assertEquals(Gravity.TOP, mLayoutParams.gravity);

        // Feature ON, Toolbar at the BOTTOM.
        mDropdown.setToolbarPosition(ControlsPosition.BOTTOM);
        assertFalse(mDropdown.getToolbarOnTopForTesting());
        assertEquals(Gravity.BOTTOM, mLayoutParams.gravity);
    }

    @Test
    public void onKeyDown_beforeShownDoesNotHandleTabNavigation() {
        when(mDropdown.isShown()).thenReturn(false);
        assertFalse(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));
        assertFalse(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));
    }

    @Test
    public void onKeyDown_handlesTabNavigationEvents() {
        when(mDropdown.isShown()).thenReturn(true);

        // Tab should be handled the first time to put focus on the first item.
        assertTrue(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));

        // Tab should be handled to move to the next item.
        assertTrue(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));

        // Shift+Tab should be handled to bring focus back to the first item.
        assertTrue(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));

        // Other modifiers should be ignored.
        // We expect super.onKeyDown to be called, which we can't directly verify.
        // But we can verify that our adapter methods are not called.
        // And we can check the return value. Let's assume super.onKeyDown returns false.
        assertFalse(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_CTRL_ON)));
        assertFalse(
                mDropdown.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_ALT_ON)));
    }
}
