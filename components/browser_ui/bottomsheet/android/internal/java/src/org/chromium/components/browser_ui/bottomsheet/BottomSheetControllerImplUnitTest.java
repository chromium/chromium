// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** Unit tests for {@link BottomSheetControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetControllerImplUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;

    @Mock private ScrimCoordinator mScrimCoordinator;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private ViewGroup mRoot;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private BottomSheet mBottomSheet;

    private BottomSheetControllerImpl mController;
    private final OneshotSupplierImpl<ScrimCoordinator> mScrimCoordinatorSupplier =
            new OneshotSupplierImpl<>();
    private final Callback<View> mInitializedCallback = view -> {};
    private Window mWindow;
    private final OneshotSupplierImpl<ViewGroup> mRootSupplier = new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mEdgeToEdgeBottomInsetSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        Activity activity = buildActivity(Activity.class).setup().get();
        mWindow = activity.getWindow();
        when(mRoot.getContext()).thenReturn(activity);
        when(mRoot.findViewById(R.id.bottom_sheet)).thenReturn(mBottomSheet);
        mScrimCoordinatorSupplier.set(mScrimCoordinator);
        mRootSupplier.set(mRoot);
        mController =
                new BottomSheetControllerImpl(
                        mScrimCoordinatorSupplier,
                        mInitializedCallback,
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        mRootSupplier,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        mDesktopWindowStateProvider);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_SheetNotInitialized() {
        // Trigger app header state change before first sheet is triggered.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        mController.onAppHeaderStateChanged(mAppHeaderState);

        // Simulate sheet initialization, this should kick off
        // BottomSheet#onAppHeaderHeightChange().
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet)
                .init(
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        APP_HEADER_HEIGHT);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_SheetInitialized() {
        // Simulate sheet initialization.
        mController.runSheetInitializerForTesting();

        // Trigger app header state change after sheet is initialized.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        mController.onAppHeaderStateChanged(mAppHeaderState);
        verify(mBottomSheet).onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_NoHeaderHeightChange() {
        // Simulate sheet initialization.
        mController.runSheetInitializerForTesting();

        // Trigger multiple app header state changes with no height change, after sheet is
        // initialized.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(150);
        mController.onAppHeaderStateChanged(mAppHeaderState);
        var newAppHeaderState = mock(AppHeaderState.class);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(100);
        mController.onAppHeaderStateChanged(newAppHeaderState);
        verify(mBottomSheet, times(1)).onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
    }
}
