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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.LayoutParams;
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
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowDelegate;

/** Unit tests for {@link OmniboxSuggestionsDropdown}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 28)
public class OmniboxSuggestionsDropdownUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Runnable mDropdownScrollListener;
    private @Mock Runnable mDropdownScrollToTopListener;
    private @Mock WindowDelegate mWindowDelegate;
    private @Mock OmniboxSuggestionsDropdownAdapter mAdapter;

    private Context mContext;

    private TestOmniboxSuggestionsDropdown mDropdown;
    private OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener mListener;
    private OmniboxAlignment mOmniboxAlignment;
    private ObservableSupplierImpl<OmniboxAlignment> mOmniboxAlignmentSupplier =
            new ObservableSupplierImpl<>();
    private boolean mIsTablet;
    private boolean mAttachedToWindow;
    private OmniboxSuggestionsDropdownEmbedder mEmbedder =
            new OmniboxSuggestionsDropdownEmbedder() {
                @Override
                public boolean isTablet() {
                    return mIsTablet;
                }

                @Override
                public void onAttachedToWindow() {
                    mAttachedToWindow = true;
                }

                @Override
                public void onDetachedFromWindow() {
                    mAttachedToWindow = false;
                }

                @Override
                public OmniboxAlignment addAlignmentObserver(Callback<OmniboxAlignment> obs) {
                    return mOmniboxAlignmentSupplier.addObserver(obs);
                }

                @Override
                public void removeAlignmentObserver(Callback<OmniboxAlignment> obs) {
                    mOmniboxAlignmentSupplier.removeObserver(obs);
                }

                @Nullable
                @Override
                public OmniboxAlignment getCurrentAlignment() {
                    return mOmniboxAlignmentSupplier.get();
                }

                @Override
                public float getVerticalTranslationForAnimation() {
                    return 0.0f;
                }
            };

    // TODO(341377411): resolve issues with mockito not being able to stub the isInLayout method.
    private static class TestOmniboxSuggestionsDropdown extends OmniboxSuggestionsDropdown {
        private boolean mIsInLayout;

        public TestOmniboxSuggestionsDropdown(Context context) {
            super(context, null);
        }

        @Override
        public boolean isInLayout() {
            return mIsInLayout;
        }

        public void setIsInLayout(boolean isInLayout) {
            mIsInLayout = isInLayout;
        }
    }

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mDropdown = new TestOmniboxSuggestionsDropdown(mContext);
        mDropdown.setAdapter(mAdapter);
        mListener = mDropdown.getLayoutScrollListener();
    }

    @After
    public void tearDown() {
        mListener.resetKeyboardShownState();
    }

    /**
     * Simulate split screen window width.
     *
     * <p>Works in tandem with @Config(qualifiers = "sw###dp").
     */
    private Context getContextForWindowWidth(int windowWidthDp) {
        Configuration config = new Configuration();
        config.screenWidthDp = windowWidthDp;

        return mContext.createConfigurationContext(config);
    }

    @Test
    @Feature("Omnibox")
    public void testBackgroundColor() {
        assertEquals(
                OmniboxResourceProvider.getSuggestionsDropdownStandardBackgroundColor(mContext),
                ChromeColors.getSurfaceColor(
                        mContext, R.dimen.omnibox_suggestion_dropdown_bg_elevation));
        assertEquals(
                OmniboxResourceProvider.getSuggestionsDropdownIncognitoBackgroundColor(mContext),
                mContext.getColor(R.color.omnibox_dropdown_bg_incognito));
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
        mListener.resetKeyboardShownState();

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
    public void onOmniboxSessionStateChange_withEmbedder() {
        mDropdown.setEmbedder(mEmbedder);

        assertFalse(mAttachedToWindow);
        mDropdown.onOmniboxSessionStateChange(true);
        assertTrue(mAttachedToWindow);

        mDropdown.onOmniboxSessionStateChange(false);
        assertFalse(mAttachedToWindow);
    }

    @Test
    public void onOmniboxSessionStateChange_withoutEmbedder() {
        assertFalse(mAttachedToWindow);
        mDropdown.onOmniboxSessionStateChange(true);
        assertFalse(mAttachedToWindow);
        mDropdown.onOmniboxSessionStateChange(false);
        assertFalse(mAttachedToWindow);
    }

    @Test
    public void testAlignmentProvider_widthChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);

        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, 0, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);
        assertEquals(600, mDropdown.getMeasuredWidth());

        mOmniboxAlignment = new OmniboxAlignment(0, 100, 400, 0, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mDropdown.isLayoutRequested());

        layoutDropdown(600, 800);
        assertEquals(400, mDropdown.getMeasuredWidth());
        assertFalse(mDropdown.isLayoutRequested());
    }

    @Test
    public void testAlignmentProvider_topChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);

        mDropdown.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int marginTop = 100;
        int height = 800 - marginTop;
        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, height, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, height);

        MarginLayoutParams layoutParams = (MarginLayoutParams) mDropdown.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(marginTop, layoutParams.topMargin);

        mOmniboxAlignment = new OmniboxAlignment(0, 54, 600, 0, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, height);

        layoutParams = (MarginLayoutParams) mDropdown.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(54, layoutParams.topMargin);
    }

    @Test
    public void testAlignmentProvider_heightChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);

        mDropdown.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int height = 400;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, height, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        assertEquals(height, mDropdown.getMeasuredHeight());

        height = 300;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, height, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        assertEquals(height, mDropdown.getMeasuredHeight());
    }

    @Test
    public void testAlignmentProvider_bottomPaddingChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);
        mDropdown.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        int originalPaddingTop = mDropdown.getPaddingTop();
        int originalPaddingLeft = mDropdown.getPaddingLeft();
        int originalPaddingRight = mDropdown.getPaddingRight();
        int originalPaddingBottom = mDropdown.getPaddingBottom();

        int bottomPadding = 40;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, 400, 10, 10, bottomPadding);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        assertEquals(
                "The new bottom padding should be layered on the original base bottom padding.",
                originalPaddingBottom + bottomPadding,
                mDropdown.getPaddingBottom());
        assertEquals(
                "A change in the bottom padding should not affect the top padding.",
                originalPaddingTop,
                mDropdown.getPaddingTop());
        assertEquals(
                "A change in the bottom padding should not affect the left padding.",
                originalPaddingLeft,
                mDropdown.getPaddingLeft());
        assertEquals(
                "A change in the bottom padding should not affect the right padding.",
                originalPaddingRight,
                mDropdown.getPaddingRight());

        bottomPadding = 20;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, 400, 10, 10, bottomPadding);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        assertEquals(
                "The new bottom padding should be layered on the original base bottom padding.",
                originalPaddingBottom + bottomPadding,
                mDropdown.getPaddingBottom());
        assertEquals(
                "A change in the bottom padding should not affect the top padding.",
                originalPaddingTop,
                mDropdown.getPaddingTop());
        assertEquals(
                "A change in the bottom padding should not affect the left padding.",
                originalPaddingLeft,
                mDropdown.getPaddingLeft());
        assertEquals(
                "A change in the bottom padding should not affect the right padding.",
                originalPaddingRight,
                mDropdown.getPaddingRight());
    }

    @Test
    @LooperMode(Mode.PAUSED)
    public void testAlignmentProvider_changeDuringlayout() {
        mDropdown.setAdapter(mAdapter);
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);

        mDropdown.setIsInLayout(true);
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 400, 600, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        mDropdown.layout(0, 0, 600, 800);
        assertFalse(mDropdown.isLayoutRequested());

        // The posted task should re-request layout.
        ShadowLooper.runUiThreadTasks();
        assertTrue(mDropdown.isLayoutRequested());
    }

    @Test
    public void translateChildrenVertical() {
        mDropdown.setAdapter(mAdapter);
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);

        View childView = Mockito.mock(View.class);

        mDropdown.translateChildrenVertical(45.6f);
        mDropdown.onChildAttachedToWindow(childView);
        verify(childView).setTranslationY(45.6f);

        mDropdown.onChildDetachedFromWindow(childView);
        verify(childView).setTranslationY(0.0f);
    }

    @Test
    public void setChildAlpha() {
        mDropdown.setAdapter(mAdapter);
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onOmniboxSessionStateChange(true);

        View childView = Mockito.mock(View.class);

        mDropdown.setChildAlpha(0.6f);
        mDropdown.onChildAttachedToWindow(childView);
        verify(childView).setAlpha(0.6f);

        mDropdown.onChildDetachedFromWindow(childView);
        verify(childView).setAlpha(1.0f);
    }

    private void layoutDropdown(int width, int height) {
        doAnswer(
                        (invocation) -> {
                            Rect r = invocation.getArgument(0);
                            r.set(0, 0, 0, height);
                            return true;
                        })
                .when(mWindowDelegate)
                .getWindowVisibleDisplayFrame(any(Rect.class));
        int widthSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.AT_MOST);
        mDropdown.measure(widthSpec, heightSpec);
        mDropdown.layout(0, 0, mDropdown.getMeasuredWidth(), mDropdown.getMeasuredHeight());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void forcePhoneStyleOmnibox_forcing_noClippingWhenForced() {
        var dropdown = new OmniboxSuggestionsDropdown(mContext, null);
        dropdown.forcePhoneStyleOmnibox(true);
        assertFalse(dropdown.getClipToOutline());
        assertNull(dropdown.getOutlineProvider());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void forcePhoneStyleOmnibox_nonForcing_clipsOnTablets_narrowWindow() {
        var context = getContextForWindowWidth(DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1);
        var dropdown = new OmniboxSuggestionsDropdown(context, null);
        dropdown.forcePhoneStyleOmnibox(false);
        assertFalse(dropdown.getClipToOutline());
        assertNull(dropdown.getOutlineProvider());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void forcePhoneStyleOmnibox_nonForcing_clipsOnTablets_wideWindow() {
        var context = getContextForWindowWidth(DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP);
        var dropdown = new OmniboxSuggestionsDropdown(context, null);
        dropdown.forcePhoneStyleOmnibox(false);
        assertTrue(dropdown.getClipToOutline());
        assertNotNull(dropdown.getOutlineProvider());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void forcePhoneStyleOmnibox_nonForcing_noClippingOnPhones() {
        var dropdown = new OmniboxSuggestionsDropdown(mContext, null);
        dropdown.forcePhoneStyleOmnibox(false);
        assertFalse(dropdown.getClipToOutline());
        assertNull(dropdown.getOutlineProvider());
    }
}
