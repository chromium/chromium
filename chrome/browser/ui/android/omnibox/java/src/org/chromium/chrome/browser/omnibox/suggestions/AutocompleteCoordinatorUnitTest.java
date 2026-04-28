// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.preloading.PreloadingFeatureMap;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Collections;
import java.util.function.Supplier;

/** Unit tests for {@link AutocompleteCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutocompleteCoordinator mAutocompleteCoordinator;
    private final SettableNonNullObservableSupplier<@ControlsPosition Integer>
            mControlsPositionSupplier = ObservableSuppliers.createNonNull(ControlsPosition.TOP);
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final MonotonicObservableSupplier<Profile> mProfileObservableSupplier =
            ObservableSuppliers.alwaysNull();

    @Mock private AutocompleteDelegate mAutocompleteDelegate;
    @Mock private OmniboxSuggestionsDropdownEmbedder mDropdownEmbedder;
    @Mock private UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private Supplier<Tab> mActivityTabSupplier;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private Callback<String> mBringToForegroundCallback;
    @Mock private BasicSuggestionProcessor.BookmarkState mBookmarkState;
    @Mock private OmniboxActionDelegateImpl mOmniboxActionDelegate;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DeferredIMEWindowInsetApplicationCallback mDeferredImeInsetCb;
    @Mock private FuseboxCoordinator mFuseboxCoordinator;
    @Mock private OmniboxSuggestionsContainer mSuggestionsContainer;
    @Mock private ViewGroup mParentView;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private PreloadingFeatureMap mPreloadingFeatureMap;
    @Mock private FuseboxSessionState mFuseboxSessionState;
    @Mock private Profile mProfile;
    @Mock private AutocompleteController mAutocompleteController;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        lenient().when(mParentView.getContext()).thenReturn(context);
        lenient()
                .when(mLocationBarDataProvider.getToolbarPositionSupplier())
                .thenReturn(mControlsPositionSupplier);

        lenient()
                .doReturn(mFuseboxStateSupplier)
                .when(mFuseboxCoordinator)
                .getFuseboxStateSupplier();

        // Stub getTextWithoutAutocomplete to return an empty string by default. This prevents
        // NullPointerException when triggerSiteSearch() is called (which checks .isEmpty() on it).
        lenient().doReturn("").when(mUrlBarEditingTextProvider).getTextWithoutAutocomplete();

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

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
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

    @Test
    public void testHandleKeyEvent_TabTriggersSiteSearch_WhenMatching() {
        doReturn(true).when(mSuggestionsContainer).isShown();

        var match =
                new AutocompleteMatchBuilder()
                        .setDisplayText("test query")
                        .setFillIntoEdit("test query")
                        .setAssociatedKeyword("bing")
                        .build();
        doReturn("bing").when(mUrlBarEditingTextProvider).getTextWithoutAutocomplete();
        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn("bing").when(mTemplateUrl).getKeyword();
        doReturn("Bing").when(mTemplateUrl).getShortName();
        doReturn(mTemplateUrl).when(mAutocompleteController).getTemplateUrlForText("bing");

        // Mock PreloadingFeatureMap to prevent crash during onSuggestionsReceived.
        PreloadingFeatureMap.setInstanceForTesting(mPreloadingFeatureMap);
        doReturn(false).when(mPreloadingFeatureMap).shouldPrewarmOnAutocomplete();

        // Create an input session to make the coordinator active and accept suggestions.
        AutocompleteInput mockInput = new AutocompleteInput();
        mockInput.setPageClassification(PageClassification.BLANK_VALUE);
        doReturn(mockInput).when(mFuseboxSessionState).getAutocompleteInput();
        doReturn(mProfile).when(mFuseboxSessionState).getProfile();
        doReturn(mAutocompleteController).when(mFuseboxSessionState).getAutocompleteController();

        mAutocompleteCoordinator.beginInput(mFuseboxSessionState);

        // Inject the suggestions result.
        mAutocompleteCoordinator
                .getSuggestionsReceivedListenerForTest()
                .onSuggestionsReceived(
                        AutocompleteResult.fromCache(Collections.singletonList(match), null),
                        false);

        // Action: Simulate TAB key press.
        assertTrue(
                mAutocompleteCoordinator.handleKeyEvent(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));

        // Verification: Verify it intercepts the event and doesn't fall through to default
        // navigation (which would call onKeyDown on the container).
        verify(mSuggestionsContainer, never()).onKeyDown(eq(KeyEvent.KEYCODE_TAB), any());
    }
}
