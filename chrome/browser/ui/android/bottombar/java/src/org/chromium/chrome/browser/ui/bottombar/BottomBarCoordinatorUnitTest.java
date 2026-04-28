// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link BottomBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ActionRegistry mActionRegistry;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mHomeActionSupplier =
            ObservableSuppliers.createNullable();

    private Activity mActivity;
    private FrameLayout mParent;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private BottomBarCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mActionRegistry.get(ActionId.NEW_TAB)).thenReturn(mActionSupplier);
        when(mActionRegistry.get(ActionId.HOME_BUTTON)).thenReturn(mHomeActionSupplier);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mParent = new FrameLayout(mActivity);
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(true);
        mCoordinator =
                new BottomBarCoordinator(
                        mParent,
                        mActionRegistry,
                        mThemeColorProvider,
                        mTabSupplier,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate);
    }

    @Test
    public void testInitialization_bindsAction() {
        assertNotNull(mCoordinator);
        verify(mActionRegistry).get(ActionId.NEW_TAB);
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
        verify(mActionRegistry).get(ActionId.HOME_BUTTON);

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
}
