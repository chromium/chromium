// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
public class AssistantCoordinator {
    private final AssistantModel mModel;
    private AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    public AssistantCoordinator(Activity activity, BottomSheetController controller,
            @Nullable AssistantTabObscuringUtil tabObscuringUtil,
            @Nullable AssistantOverlayCoordinator overlayCoordinator,
            AssistantKeyboardCoordinator.Delegate keyboardCoordinatorDelegate,
            @NonNull KeyboardVisibilityDelegate keyboardDelegate, @NonNull View rootView,
            ViewGroup rootViewGroup,
            @NonNull AssistantBrowserControlsFactory browserControlsFactory,
            @NonNull ApplicationViewportInsetSupplier applicationBottomInsetProvider,
            AccessibilityUtil accessibilityUtil, AssistantInfoPageUtil infoPageUtil,
            @Nullable AssistantProfileImageUtil profileImageUtil, ImageFetcher imageFetcher,
            @Nullable AssistantEditorFactory editorFactory, WindowAndroid windowAndroid,
            AssistantSettingsUtil settingsUtil) {
        if (overlayCoordinator != null) {
            mModel = new AssistantModel(overlayCoordinator.getModel());
            mOverlayCoordinator = overlayCoordinator;
        } else {
            mModel = new AssistantModel();
            mOverlayCoordinator = new AssistantOverlayCoordinator(activity, browserControlsFactory,
                    rootView, controller.getScrimCoordinator(), mModel.getOverlayModel(),
                    accessibilityUtil);
        }

        mBottomBarCoordinator = new AssistantBottomBarCoordinator(activity, mModel,
                mOverlayCoordinator, controller, applicationBottomInsetProvider, tabObscuringUtil,
                rootViewGroup, browserControlsFactory, accessibilityUtil, infoPageUtil,
                profileImageUtil, imageFetcher, editorFactory, windowAndroid, settingsUtil);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity, keyboardDelegate,
                rootView, mModel, keyboardCoordinatorDelegate, controller);
    }

    /** Detaches and destroys the view. */
    public void destroy() {
        mModel.setVisible(false);
        mBottomBarCoordinator.destroy();
        mBottomBarCoordinator = null;
        mOverlayCoordinator.destroy();
    }

    /**
     * Get the model representing the current state of the UI.
     */

    public AssistantModel getModel() {
        return mModel;
    }

    // Getters to retrieve the sub coordinators.

    public AssistantBottomBarCoordinator getBottomBarCoordinator() {
        return mBottomBarCoordinator;
    }

    AssistantKeyboardCoordinator getKeyboardCoordinator() {
        return mKeyboardCoordinator;
    }

    public void show() {
        // Simulates native's initialization.
        mModel.setVisible(true);
        mBottomBarCoordinator.restoreState(SheetState.HALF);
    }
}
