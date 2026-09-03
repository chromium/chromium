// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController.TraversalMode;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link OmniboxSuggestionsDropdown}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = BaseRobolectricTestRunner.MIN_SDK)
public class OmniboxSuggestionsDropdownUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Runnable mDropdownScrollListener;
    @Mock private Runnable mDropdownScrollToTopListener;
    @Mock private OmniboxSuggestionsDropdownAdapter mAdapter;
    @Mock private View mView;
    @Mock private OmniboxSuggestionsDropdown.NavigationListener mNavigationListener;

    private Context mContext;
    private OmniboxSuggestionsDropdown mDropdown;
    private OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener mListener;
    private FrameLayout.LayoutParams mLayoutParams;
    private final SettableNonNullObservableSupplier<Boolean> mChipVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mListener = spy(new OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener(mContext));
        lenient().doReturn(3).when(mListener).getItemCount();
        lenient().doReturn(mView).when(mListener).findViewByPosition(anyInt());
        lenient().doReturn(true).when(mView).isFocusable();
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
        verify(mDropdownScrollListener).run();
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
        verify(mDropdownScrollListener).run();
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
        verify(mDropdownScrollListener).run();
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
        verify(mDropdownScrollToTopListener).run();
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
        verify(mDropdownScrollListener).run();
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

        mDropdown.translateChildrenVertical(45.6f);
        mDropdown.onChildAttachedToWindow(mView);
        verify(mView).setTranslationY(45.6f);

        mDropdown.onChildDetachedFromWindow(mView);
        verify(mView).setTranslationY(0.0f);
    }

    @Test
    public void setChildAlpha() {

        mDropdown.setChildAlpha(0.6f);
        mDropdown.onChildAttachedToWindow(mView);
        verify(mView).setAlpha(0.6f);

        mDropdown.onChildDetachedFromWindow(mView);
        verify(mView).setAlpha(1.0f);
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
        clearInvocations(mListener);

        mListener.updateVisualScrollState();
        verify(mListener, never()).postOnAnimation(any());
    }

    @Test
    public void onKeyDown_beforeShownDoesNotHandleTabNavigation() {
        doReturn(false).when(mDropdown).isShown();
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
        doReturn(true).when(mDropdown).isShown();

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

    @Test
    @EnableFeatures(OmniboxFeatureList.RESET_SUGGESTIONS_SCROLL)
    public void testOnLayoutChildren_flagEnabled_scrolledToTop() {
        mListener.onLayoutChildren(null, new RecyclerView.State());
        verify(mListener).scrollToPositionWithOffset(0, 0);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.RESET_SUGGESTIONS_SCROLL)
    public void testOnLayoutChildren_flagDisabled_noScroll() {
        mListener.onLayoutChildren(null, new RecyclerView.State());
        verify(mListener, never()).scrollToPositionWithOffset(anyInt(), anyInt());
    }

    @Test
    public void testSetSelectionMode() {
        SelectionController controller = mDropdown.getSelectionControllerForTesting();

        mDropdown.setSelectionMode(TraversalMode.WRAPPING_WITH_SENTINEL);
        assertTrue(controller.isParkedAtSentinel());

        mDropdown.setSelectionMode(TraversalMode.SENTINEL_THEN_WRAPPING);
        assertTrue(controller.isParkedAtSentinel());

        mDropdown.setSelectionMode(TraversalMode.WRAPPING);
        assertFalse(controller.isParkedAtSentinel());
    }

    @Test
    public void testNavigationListener_notifiedOnKeyDown() {
        mDropdown.setNavigationListener(mNavigationListener);
        doReturn(true).when(mDropdown).isShown();

        mDropdown.onKeyDown(
                KeyEvent.KEYCODE_TAB,
                new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB, 0));

        verify(mNavigationListener).onNavigationStateChange(anyBoolean());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    public void testRecycledViewPool_NotClearedAndReused() {
        ModelList listItems = new ModelList();
        var listener = new SuggestionLayoutScrollListener(mContext);
        OmniboxSuggestionsDropdown dropdown =
                new OmniboxSuggestionsDropdown(mContext, null, listener);
        // Setting model list initializes the real adapter and view pool.
        dropdown.setModelList(listItems);

        PreWarmingRecycledViewPool pool =
                (PreWarmingRecycledViewPool) dropdown.getRecycledViewPool();

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Verify pool is initially pre-warmed.
        assertEquals(
                PreWarmingRecycledViewPool.PRE_WARMED_DEFAULT_VIEW_COUNT,
                pool.getRecycledViewCount(OmniboxSuggestionUiType.DEFAULT));

        listItems.add(
                new ListItem(
                        OmniboxSuggestionUiType.DEFAULT,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS)));

        // Force layout to trigger recycler interactions (binding the item).
        dropdown.measure(0, 0);
        dropdown.layout(0, 0, 100, 100);

        // Verify that the pool was not cleared and one view was reused.
        assertEquals(
                PreWarmingRecycledViewPool.PRE_WARMED_DEFAULT_VIEW_COUNT - 1,
                pool.getRecycledViewCount(OmniboxSuggestionUiType.DEFAULT));
    }
}
