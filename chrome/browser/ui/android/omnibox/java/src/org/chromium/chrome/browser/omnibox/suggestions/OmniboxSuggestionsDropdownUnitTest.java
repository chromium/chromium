// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static junit.framework.Assert.assertEquals;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * Unit tests for {@link OmniboxSuggestionsDropdown}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownUnitTest {
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Runnable mDropdownScrollListener;
    private @Mock Runnable mDropdownScrollToTopListener;

    private Context mContext;

    private OmniboxSuggestionsDropdown mDropdown;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mDropdown = new OmniboxSuggestionsDropdown(mContext);
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
                ChromeColors.getSurfaceColor(mContext,
                        org.chromium.chrome.browser.omnibox.R.dimen
                                .omnibox_suggestion_dropdown_bg_elevation));
        assertEquals(mDropdown.getIncognitoBgColor(),
                mContext.getColor(
                        org.chromium.chrome.browser.omnibox.R.color.omnibox_dropdown_bg_incognito));
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
        var listener = mDropdown.getLayoutScrollListener();
        // Don't flake if the list was previously scrolled by another test.
        listener.resetKeyboardShowState();

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Subsequent scroll events should pass through.
        // Keyboard should not be dismissed again.
        assertEquals(5, listener.updateKeyboardVisibilityAndScroll(5, 10));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_keyboardShouldDismissOnScrollAttemptFromScrolledList() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        var listener = mDropdown.getLayoutScrollListener();
        // Don't flake if the list was previously scrolled by another test.
        listener.resetKeyboardShowState();

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(10, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Subsequent scroll events should pass through.
        // Keyboard should not be dismissed again.
        assertEquals(5, listener.updateKeyboardVisibilityAndScroll(5, 10));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_keyboardShouldShowOnScrollToTop() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        mDropdown.setSuggestionDropdownOverscrolledToTopListener(mDropdownScrollToTopListener);
        var listener = mDropdown.getLayoutScrollListener();
        // Don't flake if the list was previously scrolled by another test.
        listener.resetKeyboardShowState();

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Pretend we scroll up, while keyboard is hidden.
        assertEquals(5, listener.updateKeyboardVisibilityAndScroll(5, -5));
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Overscroll to top. Expect the keyboard to be called in.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, -5));
        verify(mDropdownScrollToTopListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollToTopListener);

        // Overscroll again. Make sure we don't call the keyboard up again.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, -5));
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_reemitsKeyboardDismissOnReset() {
        mDropdown.setSuggestionDropdownScrollListener(mDropdownScrollListener);
        var listener = mDropdown.getLayoutScrollListener();
        // Don't flake if the list was previously scrolled by another test.
        listener.resetKeyboardShowState();

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, 10));
        verify(mDropdownScrollListener, times(1)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);

        // Simulate lists being shown again.
        listener.resetKeyboardShowState();

        // Scroll attempt should suppress the scroll and emit keyboard dismiss.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, 10));
        verify(mDropdownScrollListener, times(2)).run();
        verifyNoMoreInteractions(mDropdownScrollListener);
    }

    @Test
    @SmallTest
    public void testScrollListener_inactiveWhenObserverNotEquipped() {
        // Note: do not equip the listeners (no calls to setSuggestionDropdownScrollListener() and
        // setSuggestionDropdownOverscrolledToTopListener).
        var listener = mDropdown.getLayoutScrollListener();
        listener.resetKeyboardShowState();

        // None of the calls below should invoke listeners (and crash).
        // Scroll down from top.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, 10));
        // Scroll down from the middle. Confirm new scroll position is accepted.
        assertEquals(10, listener.updateKeyboardVisibilityAndScroll(10, 10));
        // Overscroll to top.
        assertEquals(0, listener.updateKeyboardVisibilityAndScroll(0, -10));
        verifyNoMoreInteractions(mDropdownScrollListener);
        verifyNoMoreInteractions(mDropdownScrollToTopListener);
    }
}
