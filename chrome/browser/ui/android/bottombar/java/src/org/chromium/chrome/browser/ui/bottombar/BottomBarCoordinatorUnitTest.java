// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link BottomBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GLIC)
public class BottomBarCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActionRegistry mActionRegistry;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;
    @Mock private Profile mProfile;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tracker mTracker;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mHomeActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mMenuActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mTabSwitcherActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mGlicActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createNullable();

    private Activity mActivity;
    private FrameLayout mParent;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private SettableNonNullObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private BottomBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);
        when(mActionRegistry.get(ActionId.NEW_TAB)).thenReturn(mActionSupplier);
        when(mActionRegistry.get(ActionId.HOME_BUTTON)).thenReturn(mHomeActionSupplier);
        when(mActionRegistry.get(ActionId.APP_MENU)).thenReturn(mMenuActionSupplier);
        when(mActionRegistry.get(ActionId.TAB_SWITCHER)).thenReturn(mTabSwitcherActionSupplier);
        when(mActionRegistry.get(ActionId.GLIC)).thenReturn(mGlicActionSupplier);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mParent = new FrameLayout(mActivity);
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(true);
        mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
        mModalDialogManagerSupplier = ObservableSuppliers.createNonNull(mModalDialogManager);
        mProfileSupplier.set(mProfile);
        mCoordinator =
                new BottomBarCoordinator(
                        mParent,
                        mActionRegistry,
                        mThemeColorProvider,
                        mTabSupplier,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier,
                        mModalDialogManagerSupplier);
    }

    @Test
    public void testInitialization_bindsAction() {
        assertNotNull(mCoordinator);
        verify(mActionRegistry, times(2)).get(ActionId.NEW_TAB);
        verify(mActionRegistry, times(1)).get(ActionId.TAB_SWITCHER);
    }

    @Test
    public void testActionBinding_setsClickListener() {
        AtomicBoolean clicked = new AtomicBoolean(false);
        Callback<View> onPressCallback = (v) -> clicked.set(true);
        PropertyModel actionModel = new PropertyModel.Builder(ActionProperties.BASE_KEYS).build();

        mActionSupplier.set(actionModel);

        // Verify the button is initialized.
        View newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);

        // No callback is assigned, so clicking does nothing.
        newTabButton.performClick();
        assertFalse(clicked.get());

        // Assign the callback and test again.
        actionModel.set(ActionProperties.ON_PRESS_CALLBACK, onPressCallback);
        newTabButton.performClick();
        assertTrue(clicked.get());
    }

    @Test
    public void testDestroy() {
        assertTrue(mActionSupplier.hasObservers());
        mCoordinator.destroy();
        assertFalse(mActionSupplier.hasObservers());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/false")
    public void testInitialization_withHomeButton_bindsHomeButton() {
        verify(mActionRegistry, times(1)).get(ActionId.HOME_BUTTON);

        View homeButton = mCoordinator.getView().findViewById(R.id.home_button);
        assertNotNull(homeButton);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/true")
    public void testInitialization_withoutHomeButton_doesNotBindHomeButton() {
        verify(mActionRegistry, never()).get(ActionId.HOME_BUTTON);

        View homeButton = mCoordinator.getView().findViewById(R.id.home_button);
        assertNull(homeButton);

        View homeStub = mCoordinator.getView().findViewById(R.id.home_stub);
        assertNotNull(homeStub);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false")
    public void testInitialization_withAppMenu_bindsAppMenu() {
        verify(mActionRegistry, times(1)).get(ActionId.APP_MENU);

        View menuButton = mCoordinator.getView().findViewById(R.id.app_menu_button);
        assertNotNull(menuButton);
        assertEquals(true, menuButton.getTag(R.id.is_bottom_bar_menu_anchor));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true")
    public void testInitialization_withoutAppMenu_doesNotBindAppMenu() {
        verify(mActionRegistry, never()).get(ActionId.APP_MENU);

        View menuButton = mCoordinator.getView().findViewById(R.id.app_menu_button);
        assertNull(menuButton);
    }

    @Test
    public void testGetBackgroundColor() {
        int expectedColor =
                BottomBarUtils.getBottomBarBackgroundColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT);
        assertEquals(expectedColor, mCoordinator.getBackgroundColor());
    }

    @Test
    public void testOmniboxFocusHidesBottomBar() {
        // Initially not focused, should be visible.
        verify(mVisibilityDelegate).onVisibilityChanged(true);

        // Focus omnibox, should hide.
        mOmniboxFocusStateSupplier.set(true);
        verify(mVisibilityDelegate).onVisibilityChanged(false);

        // Unfocus omnibox, should show again.
        mOmniboxFocusStateSupplier.set(false);
        verify(mVisibilityDelegate, times(2)).onVisibilityChanged(true);
    }
}
