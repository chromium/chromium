// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Generic dependencies interface. The concrete implementation will depend on the browser framework,
 * i.e., WebLayer vs. Chrome.
 *
 * WebContents should not be returned in this interface as objects should stay valid when
 * WebContents change.
 */
@JNINamespace("autofill_assistant")
public interface AssistantDependencies extends AssistantStaticDependencies {
    /**
     * Updates dependencies that are tied to the activity.
     * @return Whether a new activity could be found.
     */
    boolean maybeUpdateDependencies(Activity activity);

    boolean maybeUpdateDependencies(WebContents webContents);

    Activity getActivity();

    WindowAndroid getWindowAndroid();

    BottomSheetController getBottomSheetController();

    KeyboardVisibilityDelegate getKeyboardVisibilityDelegate();

    ApplicationViewportInsetSupplier getBottomInsetProvider();

    // TODO(b/224910639): Due to legacy reasons root view and root view group are not the same
    // object here (compositor view vs coordinator view). However, this can most likely be
    // reconciled.
    View getRootView();

    ViewGroup getRootViewGroup();

    AssistantSnackbarFactory getSnackbarFactory();

    AssistantBrowserControlsFactory createBrowserControlsFactory();

    /**
     * Observes tab changes.
     * @return The destroyer that must be called to unregister the internal observer.
     */
    Destroyable observeTabChanges(AssistantTabChangeObserver tabChangeObserver);

    // Only called by native to guarantee future type safety.
    @CalledByNative
    default AssistantStaticDependencies getStaticDependencies() {
        return this;
    }
}
