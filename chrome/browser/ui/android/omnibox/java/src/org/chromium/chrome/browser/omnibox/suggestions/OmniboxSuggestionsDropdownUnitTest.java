// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertTrue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.LayoutParams;
import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.WindowDelegate;

/**
 * Unit tests for {@link OmniboxSuggestionsDropdown}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownUnitTest {
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Runnable mDropdownScrollListener;
    private @Mock Runnable mDropdownScrollToTopListener;
    private @Mock WindowDelegate mWindowDelegate;
    private @Mock OmniboxSuggestionsDropdownAdapter mAdapter;
    private @Mock RecycledViewPool mPool;

    private Context mContext;

    private OmniboxSuggestionsDropdown mDropdown;
    private OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener mListener;
    private OmniboxAlignment mOmniboxAlignment;
    private ObservableSupplierImpl<OmniboxAlignment> mOmniboxAlignmentSupplier =
            new ObservableSupplierImpl<>();
    private boolean mIsTablet;
    private boolean mAttachedToWindow;
    private OmniboxSuggestionsDropdownEmbedder mEmbedder =
            new OmniboxSuggestionsDropdownEmbedder() {
                @NonNull
                @Override
                public WindowDelegate getWindowDelegate() {
                    return mWindowDelegate;
                }

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
            };

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mDropdown = new OmniboxSuggestionsDropdown(mContext, mPool);
        mDropdown.setAdapter(mAdapter);
        mListener = mDropdown.getLayoutScrollListener();
    }

    @After
    public void tearDown() {
        mListener.resetKeyboardShownState();
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    public void
    testBackgroundColor_withOmniboxModernizeVisualUpdateFlags() {
        assertEquals(mDropdown.getStandardBgColor(),
                ChromeColors.getSurfaceColor(
                        mContext, R.dimen.omnibox_suggestion_dropdown_bg_elevation));
        assertEquals(mDropdown.getIncognitoBgColor(),
                mContext.getColor(R.color.default_bg_color_dark_elev_1_gm3_baseline));
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @DisableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void testBackgroundColor_withoutOmniboxModernizeVisualUpdateFlags() {
        assertEquals(
                mDropdown.getStandardBgColor(), ChromeColors.getDefaultThemeColor(mContext, false));
        assertEquals(
                mDropdown.getIncognitoBgColor(), ChromeColors.getDefaultThemeColor(mContext, true));
    }

    @Test
    @SmallTest
    public void testScrollListener_keyboardShouldDismissOnScrollAttemptFromTop() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);

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
    @SmallTest
    public void testScrollListener_keyboardShouldDismissOnScrollAttemptFromScrolledList() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);

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
    @SmallTest
    public void testScrollListener_keyboardShouldShowOnScrollToTop() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        mDropdown.setSuggestionDropdownOverscrolledToTopListener(mDropdownScrollToTopListener);

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Pretend we scroll up, while keyboard is hidden.
        assertEquals(-5, mListener.updateKeyboardVisibilityAndScroll(-5, -5));
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Overscroll to top. Expect the keyboard to be called in.
        assertEquals(-5, mListener.updateKeyboardVisibilityAndScroll(-5, -10));
        verify(mDropdownScrollToTopListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollToTopListener);

        // Overscroll again. Make sure we don't call the keyboard up again.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, -5));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_notDismissingKeyboardWhenScrollDoesNotHappen() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);

        // Pretend we're scrolling down (delta=10) but there is no content to move to (scroll=0).
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, 10));
        // Confirm that we're not hiding the keyboard.
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Pretend we're scrolling up now (delta=-10) but we're already on top and can't move.
        assertEquals(0, mListener.updateKeyboardVisibilityAndScroll(0, -10));
        // Confirm that we're not trying to show the keyboard.
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_notDismissingKeyboardWhenTheListIsOnlyBarelyUnderTheKeyboard() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);

        // We want to scroll by 10px, but there's only 1px of content. Don't hide the keyboard.
        assertEquals(1, mListener.updateKeyboardVisibilityAndScroll(1, 10));
        verifyNoMoreInteractions(mDropdownScrollListener);

        // We want to scroll by 10px, but there's only 9px of content. Don't hide the keyboard.
        assertEquals(9, mListener.updateKeyboardVisibilityAndScroll(9, 10));
        verifyNoMoreInteractions(mDropdownScrollListener);

        // But then, if we scroll back up, we likely should not ask for keyboard to show.
        assertEquals(-9, mListener.updateKeyboardVisibilityAndScroll(-9, -10));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_reemitsKeyboardDismissOnReset() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);

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
    @SmallTest
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
    @SmallTest
    public void testAlignmentProvider_windowAttachment() {
        mDropdown.setEmbedder(mEmbedder);
        assertFalse(mAttachedToWindow);

        mDropdown.onAttachedToWindow();
        assertTrue(mAttachedToWindow);

        mDropdown.onDetachedFromWindow();
        assertFalse(mAttachedToWindow);
    }

    @Test
    @SmallTest
    public void testAlignmentProvider_paddingChange() {
        assertEquals(0, mDropdown.getMeasuredWidth());

        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onAttachedToWindow();
        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, 0, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);
        assertEquals(600, mDropdown.getMeasuredWidth());
        assertEquals(10, mDropdown.getPaddingLeft());
        assertEquals(10, mDropdown.getPaddingRight());

        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, 0, 50, 50);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        ShadowLooper.runUiThreadTasks();

        assertEquals(50, mDropdown.getPaddingLeft());
        assertEquals(50, mDropdown.getPaddingRight());
    }

    @Test
    @SmallTest
    public void testAlignmentProvider_widthChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onAttachedToWindow();
        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, 0, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);
        assertEquals(600, mDropdown.getMeasuredWidth());

        mOmniboxAlignment = new OmniboxAlignment(0, 100, 400, 0, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mDropdown.isLayoutRequested());

        layoutDropdown(600, 800);
        assertEquals(400, mDropdown.getMeasuredWidth());
        assertFalse(mDropdown.isLayoutRequested());
    }

    @Test
    @SmallTest
    public void testAlignmentProvider_topChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onAttachedToWindow();
        mDropdown.setLayoutParams(new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, 0, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        MarginLayoutParams layoutParams = (MarginLayoutParams) mDropdown.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(100, layoutParams.topMargin);
        assertEquals(800 - 100, mDropdown.getMeasuredHeight());

        mOmniboxAlignment = new OmniboxAlignment(0, 54, 600, 0, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        layoutParams = (MarginLayoutParams) mDropdown.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(54, layoutParams.topMargin);
        assertEquals(800 - 54, mDropdown.getMeasuredHeight());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_CONSUMERS_IME_INSETS})
    public void testAlignmentProvider_heightChange() {
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onAttachedToWindow();
        mDropdown.setLayoutParams(new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int height = 400;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, height, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        assertEquals(height, mDropdown.getMeasuredHeight());

        height = 300;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, height, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        assertEquals(height, mDropdown.getMeasuredHeight());
    }

    @Test
    @SmallTest
    @LooperMode(Mode.PAUSED)
    public void testAlignmentProvider_changeDuringlayout() {
        mDropdown = Mockito.spy(new OmniboxSuggestionsDropdown(mContext, mPool));
        mDropdown.setAdapter(mAdapter);
        mDropdown.setEmbedder(mEmbedder);
        mDropdown.onAttachedToWindow();

        doReturn(true).when(mDropdown).isInLayout();
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 400, 600, 10, 10);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        mDropdown.layout(0, 0, 600, 800);
        assertFalse(mDropdown.isLayoutRequested());

        // The posted task should re-request layout.
        Mockito.clearInvocations(mDropdown);
        ShadowLooper.runUiThreadTasks();
        verify(mDropdown).requestLayout();
    }

    private void layoutDropdown(int width, int height) {
        doAnswer((invocation) -> {
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
}
