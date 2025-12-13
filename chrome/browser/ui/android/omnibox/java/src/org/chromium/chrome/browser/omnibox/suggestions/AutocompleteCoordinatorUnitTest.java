// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;

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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Unit tests for {@link AutocompleteCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private final ObservableSupplierImpl<@ControlsPosition Integer> mControlsPositionSupplier =
            new ObservableSupplierImpl<>(ControlsPosition.TOP);
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier =
                    new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);

    @Mock private AutocompleteDelegate mAutocompleteDelegate;
    @Mock private OmniboxSuggestionsDropdownEmbedder mDropdownEmbedder;
    @Mock private UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private Supplier<Tab> mActivityTabSupplier;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private ObservableSupplier<Profile> mProfileObservableSupplier;
    @Mock private Callback<String> mBringToForegroundCallback;
    @Mock private BasicSuggestionProcessor.BookmarkState mBookmarkState;
    @Mock private OmniboxActionDelegate mOmniboxActionDelegate;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DeferredIMEWindowInsetApplicationCallback mDeferredImeInsetCb;
    @Mock private FuseboxCoordinator mFuseboxCoordinator;
    @Mock private OmniboxSuggestionsContainer mSuggestionsContainer;
    @Mock private ViewGroup mParentView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        lenient().when(mParentView.getContext()).thenReturn(mContext);
        lenient()
                .when(mLocationBarDataProvider.getToolbarPositionSupplier())
                .thenReturn(mControlsPositionSupplier);

        lenient()
                .doReturn(mAutocompleteRequestTypeSupplier)
                .when(mFuseboxCoordinator)
                .getAutocompleteRequestTypeSupplier();

        mAutocompleteCoordinator =
                new AutocompleteCoordinator(
                        mParentView,
                        mAutocompleteDelegate,
                        mDropdownEmbedder,
                        mUrlBarEditingTextProvider,
                        mModalDialogManagerSupplier,
                        mActivityTabSupplier,
                        mShareDelegateSupplier,
                        mLocationBarDataProvider,
                        mProfileObservableSupplier,
                        mBringToForegroundCallback,
                        mBookmarkState,
                        mOmniboxActionDelegate,
                        null,
                        mLifecycleDispatcher,
                        false,
                        mWindowAndroid,
                        mDeferredImeInsetCb,
                        mFuseboxCoordinator);

        mAutocompleteCoordinator.setSuggestionsContainerForTest(mSuggestionsContainer);
    }

    @Test
    public void testHandleKeyEvent() {
        // Suggestions are shown.
        doReturn(true).when(mSuggestionsContainer).isShown();

        // Tab navigation is handled.
        assertTrue(
                mAutocompleteCoordinator.handleKeyEvent(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));

        // Shift+Tab navigation is handled.
        assertTrue(
                mAutocompleteCoordinator.handleKeyEvent(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));

        // Ctrl+Tab is not handled.
        assertFalse(
                mAutocompleteCoordinator.handleKeyEvent(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_CTRL_ON)));

        // Alt+Tab is not handled.
        assertFalse(
                mAutocompleteCoordinator.handleKeyEvent(
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
