// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Unit tests for {@link AutocompleteCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocompleteCoordinatorUnitTest {
    private static class TestOmniboxSuggestionsContainer extends OmniboxSuggestionsContainer {
        private boolean mIsShown;
        private boolean mHandleKeyDown;
        private int mLastKeyDownKeyCode;

        TestOmniboxSuggestionsContainer(Context context) {
            super(context, null);
        }

        @Override
        public boolean isShown() {
            return mIsShown;
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            mLastKeyDownKeyCode = keyCode;
            return mHandleKeyDown;
        }
    }

    private static class TestOmniboxSuggestionsDropdown extends OmniboxSuggestionsDropdown {
        private boolean mIsShown = true;

        TestOmniboxSuggestionsDropdown(Context context) {
            super(context, null);
        }

        @Override
        public boolean isShown() {
            return mIsShown;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Context mContext;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private final MonotonicObservableSupplier<Profile> mProfileObservableSupplier =
            ObservableSuppliers.alwaysNull();

    @Mock private AutocompleteMediator mAutocompleteMediator;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private LocationBarEmbedder mLocationBarEmbedder;
    @Mock private OmniboxResourceProvider mResourceProvider;
    @Mock private Profile mProfile;

    private TestOmniboxSuggestionsContainer mSuggestionsContainer;
    private TestOmniboxSuggestionsDropdown mSuggestionsDropdown;
    private ViewGroup mParentView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mParentView = new FrameLayout(mContext);
        mSuggestionsContainer = new TestOmniboxSuggestionsContainer(mContext);
        mSuggestionsDropdown = new TestOmniboxSuggestionsDropdown(mContext);

        mAutocompleteCoordinator =
                new AutocompleteCoordinator(
                        mParentView,
                        mAutocompleteMediator,
                        mProfileObservableSupplier,
                        mLocationBarEmbedder,
                        mModalDialogManagerSupplier,
                        mResourceProvider);

        mAutocompleteCoordinator.setSuggestionsContainerForTest(mSuggestionsContainer);
        mAutocompleteCoordinator.setSuggestionsDropdownForTest(mSuggestionsDropdown);
    }

    @Test
    public void testHandleKeyEvent() {
        // Suggestions are shown.
        mSuggestionsContainer.mIsShown = true;
        mSuggestionsContainer.mHandleKeyDown = true;

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
        mSuggestionsContainer.mIsShown = true;

        // Container handles Enter.
        mSuggestionsContainer.mHandleKeyDown = true;
        assertTrue(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, 0));
        assertEquals(KeyEvent.KEYCODE_ENTER, mSuggestionsContainer.mLastKeyDownKeyCode);

        // Container does not handle Enter.
        mSuggestionsContainer.mHandleKeyDown = false;
        assertFalse(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, 0));
    }

    @Test
    public void testHandleKeyEvent_altEnter_delegatesToContainer() {
        mSuggestionsContainer.mIsShown = true;
        mSuggestionsContainer.mHandleKeyDown = true;
        assertTrue(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, KeyEvent.META_ALT_ON));
        assertEquals(KeyEvent.KEYCODE_ENTER, mSuggestionsContainer.mLastKeyDownKeyCode);
    }

    @Test
    public void testHandleKeyEvent_shiftEnter_delegatesToContainer() {
        mSuggestionsContainer.mIsShown = true;
        mSuggestionsContainer.mHandleKeyDown = false;
        assertFalse(sendKeyDownEvent(KeyEvent.KEYCODE_ENTER, KeyEvent.META_SHIFT_ON));
        assertEquals(KeyEvent.KEYCODE_ENTER, mSuggestionsContainer.mLastKeyDownKeyCode);
    }

    @Test
    public void testProfileObserver_synchronousBindingWhenNonNull() {
        SettableMonotonicObservableSupplier<Profile> profileSupplier =
                ObservableSuppliers.createMonotonic();
        profileSupplier.set(mProfile);

        new AutocompleteCoordinator(
                mParentView,
                mAutocompleteMediator,
                profileSupplier,
                mLocationBarEmbedder,
                mModalDialogManagerSupplier,
                mResourceProvider);

        verify(mAutocompleteMediator).setAutocompleteProfile(mProfile);
    }

    @Test
    public void testIsServingSuggestions_requiresContainerAndDropdownShown() {
        doReturn(true).when(mAutocompleteMediator).isInInputSession();
        doReturn(2).when(mAutocompleteMediator).getSuggestionCount();
        mSuggestionsContainer.mIsShown = true;

        mSuggestionsDropdown.mIsShown = false;
        assertFalse(mAutocompleteCoordinator.isServingSuggestions());

        mSuggestionsDropdown.mIsShown = true;
        assertTrue(mAutocompleteCoordinator.isServingSuggestions());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_PREFETCH_SELECTED_SUGGESTIONS_OMT_ANDROID)
    public void testOmtOverlay_notCreatedWhenFeatureDisabled() {
        AutocompleteCoordinator coordinator = createCoordinatorWithSuggestionsStub();

        OmniboxSuggestionsContainer container = coordinator.getSuggestionsContainer();
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(container);
        OmniboxSuggestionsDropdown dropdown =
                (OmniboxSuggestionsDropdown) coordinator.getSuggestionsDropdown();
        assertNotNull(dropdown);
        assertNull(dropdown.getOmtOverlayViewForTesting());
        assertEquals(1, container.getChildCount());
        assertEquals(dropdown, container.getChildAt(0));

        coordinator.destroy();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_PREFETCH_SELECTED_SUGGESTIONS_OMT_ANDROID)
    public void testOmtOverlay_createdAndDestroyedWhenFeatureEnabled() throws Exception {
        AutocompleteCoordinator coordinator = createCoordinatorWithSuggestionsStub();

        OmniboxSuggestionsContainer container = coordinator.getSuggestionsContainer();
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(container);
        OmniboxSuggestionsDropdown dropdown =
                (OmniboxSuggestionsDropdown) coordinator.getSuggestionsDropdown();
        assertNotNull(dropdown);
        SuggestionsOmtOverlayView overlay = dropdown.getOmtOverlayViewForTesting();
        assertNotNull(overlay);
        assertEquals(container, overlay.getParent());
        assertEquals(2, container.getChildCount());
        assertTrue(overlay.getInputThreadForTesting().isAlive());

        coordinator.destroy();

        assertNull(coordinator.getSuggestionsContainerForTest());
        assertNull(dropdown.getOmtOverlayViewForTesting());
        assertNull(overlay.getParent());
        overlay.getInputThreadForTesting().join();
        assertFalse(overlay.getInputThreadForTesting().isAlive());
    }

    private AutocompleteCoordinator createCoordinatorWithSuggestionsStub() {
        int inflatedId = View.generateViewId();
        AsyncViewStub stub = new AsyncViewStub(mContext, null);
        stub.setLayoutResource(R.layout.suggestions_result_container);
        stub.setInflatedId(inflatedId);
        mParentView.addView(stub);
        when(mLocationBarEmbedder.getSuggestionsContainerStub()).thenReturn(stub);
        when(mLocationBarEmbedder.getSuggestionsContainerInflatedViewId()).thenReturn(inflatedId);
        return new AutocompleteCoordinator(
                mParentView,
                mAutocompleteMediator,
                mProfileObservableSupplier,
                mLocationBarEmbedder,
                mModalDialogManagerSupplier,
                mResourceProvider);
    }

    private boolean sendKeyDownEvent(int keyCode, int metaState) {
        return mAutocompleteCoordinator.handleKeyEvent(
                keyCode, new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, keyCode, 0, metaState));
    }
}
