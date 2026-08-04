// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Unit tests for {@link AutocompleteCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutocompleteCoordinator mAutocompleteCoordinator;
    private final MonotonicObservableSupplier<Profile> mProfileObservableSupplier =
            ObservableSuppliers.alwaysNull();

    @Mock private AutocompleteMediator mAutocompleteMediator;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private LocationBarEmbedder mLocationBarEmbedder;
    @Mock private OmniboxSuggestionsContainer mSuggestionsContainer;
    @Mock private ViewGroup mParentView;
    @Mock private OmniboxResourceProvider mResourceProvider;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        lenient().when(mParentView.getContext()).thenReturn(context);

        mAutocompleteCoordinator =
                new AutocompleteCoordinator(
                        mParentView,
                        mAutocompleteMediator,
                        mProfileObservableSupplier,
                        mLocationBarEmbedder,
                        mModalDialogManagerSupplier,
                        mResourceProvider);

        mAutocompleteCoordinator.setSuggestionsContainerForTest(mSuggestionsContainer);
    }

    @Test
    public void testHandleKeyEvent() {
        // Suggestions are shown.
        doReturn(true).when(mSuggestionsContainer).isShown();
        doReturn(true)
                .when(mSuggestionsContainer)
                .onKeyDown(eq(KeyEvent.KEYCODE_TAB), any(KeyEvent.class));

        // Tab navigation is handled.
        assertTrue(sendKeyDownEvent(KeyEvent.KEYCODE_TAB, 0));

        // Shift+Tab navigation is handled.
        assertTrue(sendKeyDownEvent(KeyEvent.KEYCODE_TAB, KeyEvent.META_SHIFT_ON));

        // Ctrl+Tab is not handled.
        assertFalse(sendKeyDownEvent(KeyEvent.KEYCODE_TAB, KeyEvent.META_CTRL_ON));

        // Alt+Tab is not handled.
        assertFalse(sendKeyDownEvent(KeyEvent.KEYCODE_TAB, KeyEvent.META_ALT_ON));
    }

    @Test
    public void testHandleKeyEvent_enter_delegatesToContainer() {
        doReturn(true).when(mSuggestionsContainer).isShown();

        // Container handles Enter.
        doReturn(true).when(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());
        assertTrue(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, 0));
        verify(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());

        // Container does not handle Enter.
        doReturn(false).when(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());
        assertFalse(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, 0));
    }

    @Test
    public void testHandleKeyEvent_altEnter_delegatesToContainer() {
        doReturn(true).when(mSuggestionsContainer).isShown();
        doReturn(true).when(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());
        assertTrue(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, KeyEvent.META_ALT_ON));
        verify(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());
    }

    @Test
    public void testHandleKeyEvent_shiftEnter_delegatesToContainer() {
        doReturn(true).when(mSuggestionsContainer).isShown();
        doReturn(false).when(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());
        assertFalse(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, KeyEvent.META_SHIFT_ON));
        verify(mSuggestionsContainer).onKeyDown(eq(KeyEvent.KEYCODE_ENTER), any());
    }

    private boolean sendKeyDownEvent(int keyCode, int metaState) {
        return mAutocompleteCoordinator.handleKeyEvent(
                keyCode, new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, keyCode, 0, metaState));
    }
}
