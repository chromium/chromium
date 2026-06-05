// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link BottomBarButtonManager}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarButtonManagerUnitTest {
    private static final int HOME = ActionId.HOME_BUTTON;
    private static final int GLIC = ActionId.GLIC;
    private static final int NEW_TAB = ActionId.NEW_TAB;
    private static final int TAB_SWITCHER = ActionId.TAB_SWITCHER;
    private static final int APP_MENU = ActionId.APP_MENU;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> mBinder;
    @Mock private BottomBarButtonManager.Listener mListener;
    @Mock private BottomBarButtonContainer mContainerHome;
    @Mock private BottomBarButtonContainer mContainerGlic;
    @Mock private BottomBarButtonContainer mContainerNewTab;
    @Mock private BottomBarButtonContainer mContainerTabSwitcher;
    @Mock private BottomBarButtonContainer mContainerAppMenu;
    @Mock private ActionRegistry mActionRegistry;

    private SettableNullableObservableSupplier<PropertyModel> mSupplierHome;
    private SettableNullableObservableSupplier<PropertyModel> mSupplierGlic;
    private SettableNullableObservableSupplier<PropertyModel> mSupplierNewTab;
    private SettableNullableObservableSupplier<PropertyModel> mSupplierTabSwitcher;
    private SettableNullableObservableSupplier<PropertyModel> mSupplierAppMenu;
    private @Nullable BottomBarButtonManager mManager;
    private PropertyModel mBottomBarModel;

    @Before
    public void setUp() {
        mBottomBarModel = new PropertyModel(BottomBarProperties.ALL_KEYS);

        mSupplierHome = ObservableSuppliers.createNullable();
        mSupplierGlic = ObservableSuppliers.createNullable();
        mSupplierNewTab = ObservableSuppliers.createNullable();
        mSupplierTabSwitcher = ObservableSuppliers.createNullable();
        mSupplierAppMenu = ObservableSuppliers.createNullable();

        when(mActionRegistry.get(HOME)).thenReturn(mSupplierHome);
        when(mActionRegistry.get(GLIC)).thenReturn(mSupplierGlic);
        when(mActionRegistry.get(NEW_TAB)).thenReturn(mSupplierNewTab);
        when(mActionRegistry.get(TAB_SWITCHER)).thenReturn(mSupplierTabSwitcher);
        when(mActionRegistry.get(APP_MENU)).thenReturn(mSupplierAppMenu);
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }
    }

    @Test
    public void testOnModelChanged_WhenModelSupplied_AddsObserverAndNotifiesListener() {
        mManager = initManager(HOME, HOME);
        clearInvocations(mListener);

        PropertyModel actionModel = new PropertyModel();
        mSupplierHome.set(actionModel);

        verify(mListener).onButtonVisibilityChanged(HOME, true);
        verify(mListener).onBottomBarStateChanged(/* visibilityChanged= */ true);
        assertTrue(
                "Home button should be visible",
                mBottomBarModel.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));
    }

    @Test
    public void testIsCentered_CalculatesBalanceBasedOnVisibility() {
        mManager = initManager(GLIC, HOME, GLIC, NEW_TAB);

        PropertyModel modelHome = new PropertyModel();
        PropertyModel modelGlic = new PropertyModel();
        PropertyModel modelNewTab = new PropertyModel();
        mSupplierHome.set(modelHome);
        mSupplierGlic.set(modelGlic);
        mSupplierNewTab.set(modelNewTab);

        // Order: [HOME, GLIC, NEW_TAB]
        // Center is GLIC.

        assertTrue("Should be centered", mManager.hasCenteredButton());

        // Hide HOME.
        mSupplierHome.set(null);
        assertFalse(
                "Should not be centered when left has fewer elements",
                mManager.hasCenteredButton());
        assertFalse(
                "Home button should be gone",
                mBottomBarModel.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));

        // Hide NEW_TAB.
        mSupplierNewTab.set(null);
        assertTrue("Should be centered back with equal counts", mManager.hasCenteredButton());
        assertFalse(
                "New Tab button should be gone",
                mBottomBarModel.get(BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE));
    }

    @Test
    public void testIsCentered_WithMultipleButtons_CalculatesBalanceCorrectly() {
        mManager = initManager(NEW_TAB, HOME, GLIC, NEW_TAB, TAB_SWITCHER, APP_MENU);

        PropertyModel modelHome = new PropertyModel();
        PropertyModel modelGlic = new PropertyModel();
        PropertyModel modelNewTab = new PropertyModel();
        PropertyModel modelTabSwitcher = new PropertyModel();
        PropertyModel modelAppMenu = new PropertyModel();
        mSupplierHome.set(modelHome);
        mSupplierGlic.set(modelGlic);
        mSupplierNewTab.set(modelNewTab);
        mSupplierTabSwitcher.set(modelTabSwitcher);
        mSupplierAppMenu.set(modelAppMenu);

        // Order: [HOME, GLIC, NEW_TAB, TAB_SWITCHER, APP_MENU].
        // Center is NEW_TAB.
        assertTrue("Should be centered with 2 on each side", mManager.hasCenteredButton());

        // Hide HOME.
        mSupplierHome.set(null);
        assertFalse("Should not be centered", mManager.hasCenteredButton());

        // Hide APP_MENU.
        mSupplierAppMenu.set(null);
        assertTrue("Should be centered after hiding one on right", mManager.hasCenteredButton());
    }

    @Test(expected = AssertionError.class)
    public void testInitialize_ThrowsIfDuplicateActions() {
        initManager(HOME, HOME, HOME);
    }

    @Test
    public void testDestroy_CleansUpObservers() {
        mManager = initManager(HOME, HOME);
        clearInvocations(mListener);

        PropertyModel model1 = new PropertyModel(BottomBarProperties.ALL_KEYS);

        // Verify property change triggers listener before destroy.
        mSupplierHome.set(model1);
        verify(mListener, times(1)).onButtonVisibilityChanged(HOME, true);
        verify(mListener, times(1)).onBottomBarStateChanged(/* visibilityChanged= */ true);

        clearInvocations(mListener);
        model1.set(BottomBarProperties.IS_VISIBLE, true);
        verify(mListener, times(1)).onBottomBarStateChanged(/* visibilityChanged= */ false);

        mManager.destroy();

        // Reset history to verify exactly zero new calls happen after destroy.
        clearInvocations(mListener);

        // 1. Mutating the old model should NOT trigger the listener anymore.
        model1.set(BottomBarProperties.IS_VISIBLE, false);

        // 2. Supplying a new model should NOT trigger the listener anymore.
        PropertyModel model2 = new PropertyModel(BottomBarProperties.ALL_KEYS);
        mSupplierHome.set(model2);

        // 3. Unsetting the model should NOT trigger the listener anymore.
        mSupplierHome.set(null);

        // 4. Mutating the new model should NOT trigger the listener anymore.
        model2.set(BottomBarProperties.IS_VISIBLE, false);

        // Verify NO calls were made to the listener after destroy().
        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testOnModelChanged_WhenVisibilityDoesNotChange_ListenerNotifiedWithStateChanged() {
        mManager = initManager(HOME, HOME);
        clearInvocations(mListener);

        // Set initial model (null -> non-null). Visibility changes to true.
        PropertyModel model1 = new PropertyModel(BottomBarProperties.ALL_KEYS);
        mSupplierHome.set(model1);
        verify(mListener).onButtonVisibilityChanged(HOME, true);
        verify(mListener).onBottomBarStateChanged(/* visibilityChanged= */ true);

        clearInvocations(mListener);

        // Set a different model (non-null -> non-null). Button remains visible. Only properties
        // change.
        PropertyModel model2 = new PropertyModel(BottomBarProperties.ALL_KEYS);
        mSupplierHome.set(model2);
        verify(mListener).onBottomBarStateChanged(/* visibilityChanged= */ false);

        // Reset history to verify exactly zero new calls happen.
        clearInvocations(mListener);

        // Verify model1 (old model) no longer triggers listener.
        model1.set(BottomBarProperties.IS_VISIBLE, true);
        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testOnModelChanged_WhenMediatorSetVisibilityFalse_DoesNotOverride() {
        mManager = initManager(GLIC, HOME, GLIC);

        PropertyModel modelHome = new PropertyModel();
        mSupplierHome.set(modelHome);

        // Order: [HOME, GLIC]. Center is GLIC.
        assertFalse("Should not be centered", mManager.hasCenteredButton());
        assertTrue(
                "Home button should be visible",
                mBottomBarModel.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));

        // Mediator hides HOME through setButtonVisibility().
        mManager.setButtonVisibility(HOME, false);
        assertTrue(
                "Should be centered after mediator hides HOME button",
                mManager.hasCenteredButton());
        assertFalse(
                "Home button should be hidden",
                mBottomBarModel.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));

        // Trigger model change.
        mSupplierHome.set(new PropertyModel());

        // Verify it STILL is centered (counts didn't change).
        assertTrue(
                "Should still be centered, model change should not override the mediator",
                mManager.hasCenteredButton());
        assertFalse(
                "Home button should still be hidden",
                mBottomBarModel.get(BottomBarProperties.IS_HOME_BUTTON_VISIBLE));
    }

    @Test(expected = AssertionError.class)
    public void testInitialize_ThrowsIfCenterActionIdNotFound() {
        mManager = initManager(GLIC, HOME);
    }

    private BottomBarButtonManager initManager(int centerActionId, int... actions) {
        List<BottomBarButtonManager.ActionConfig> configs = createConfigs(actions);
        BottomBarButtonManager manager =
                new BottomBarButtonManager(
                        configs, mActionRegistry, mBottomBarModel, centerActionId);
        manager.setListener(mListener);
        return manager;
    }

    private List<BottomBarButtonManager.ActionConfig> createConfigs(int... actions) {
        List<BottomBarButtonManager.ActionConfig> configs = new ArrayList<>();
        for (int action : actions) {
            configs.add(
                    new BottomBarButtonManager.ActionConfig(
                            action,
                            getContainerForAction(action),
                            mBinder,
                            getPropertyKeyForAction(action)));
        }
        return configs;
    }

    private BottomBarButtonContainer getContainerForAction(int action) {
        switch (action) {
            case HOME:
                return mContainerHome;
            case GLIC:
                return mContainerGlic;
            case NEW_TAB:
                return mContainerNewTab;
            case TAB_SWITCHER:
                return mContainerTabSwitcher;
            case APP_MENU:
                return mContainerAppMenu;
            default:
                throw new IllegalArgumentException("Unknown action: " + action);
        }
    }

    private PropertyModel.WritableBooleanPropertyKey getPropertyKeyForAction(int action) {
        switch (action) {
            case HOME:
                return BottomBarProperties.IS_HOME_BUTTON_VISIBLE;
            case GLIC:
                return BottomBarProperties.IS_GLIC_BUTTON_VISIBLE;
            case NEW_TAB:
                return BottomBarProperties.IS_NEW_TAB_BUTTON_VISIBLE;
            case TAB_SWITCHER:
                return BottomBarProperties.IS_TAB_SWITCHER_BUTTON_VISIBLE;
            case APP_MENU:
                return BottomBarProperties.IS_APP_MENU_BUTTON_VISIBLE;
            default:
                throw new IllegalArgumentException("Unknown action: " + action);
        }
    }
}
