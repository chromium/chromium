// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for setting up and coordinating everything related to the
 * add-to-homescreen UI component.
 *
 * The {@link #showForAppMenu} method is used to show the add-to-homescreen UI when the user
 * chooses the "Add to Home screen" option from the app menu.
 */
@JNINamespace("webapps")
public class AddToHomescreenCoordinator {
    private Context mActivityContext;
    private ModalDialogManager mModalDialogManager;
    private WindowAndroid mWindowAndroid;
    // May be null during tests.
    private WebContents mWebContents;

    @VisibleForTesting
    public AddToHomescreenCoordinator(WebContents webContents, Context activityContext,
            WindowAndroid windowAndroid, ModalDialogManager modalDialogManager) {
        mActivityContext = activityContext;
        mWindowAndroid = windowAndroid;
        mModalDialogManager = modalDialogManager;
        mWebContents = webContents;
    }

    /**
     * Starts and shows the add-to-homescreen UI component for the given {@link WebContents}.
     * @return whether add-to-homescreen UI was started successfully.
     */
    public static void showForAppMenu(Context activityContext, WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager, WebContents webContents, Bundle menuItemData) {
        @StringRes
        int titleId = menuItemData.getInt(AppBannerManager.MENU_TITLE_KEY);
        new AddToHomescreenCoordinator(
                webContents, activityContext, windowAndroid, modalDialogManager)
                .showForAppMenu(titleId);
    }

    @VisibleForTesting
    public boolean showForAppMenu(@StringRes int titleId) {
        // Don't start if there is no visible URL to add.
        if (mWebContents == null || mWebContents.getVisibleUrl().isEmpty()) {
            return false;
        }

        buildMediatorAndShowDialog().startForAppMenu(mWebContents, titleId);
        return true;
    }

    /**
     * Constructs all MVC components on request from the C++ side.
     * @param webContents The {@link WebContents} that initiated the add to homescreen request. Used
     *         for accessing activity {@link Context} and {@link ModalDialogManager}.
     * @return A C++ pointer to the associated add_to_homescreen_mediator.cc object. This will be
     * used by add_to_homescreen_coordinator.cc to complete the initialization of the mediator.
     */
    @CalledByNative
    private static long initMvcAndReturnMediator(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return 0;

        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();

        if (modalDialogManager == null) return 0;

        AddToHomescreenCoordinator coordinator = new AddToHomescreenCoordinator(
                webContents, windowAndroid.getContext().get(), windowAndroid, modalDialogManager);
        return coordinator.buildMediatorAndShowDialog().getNativeMediator();
    }

    /**
     * Constructs all MVC components. {@link AddToHomescreenDialogView} is shown as soon as it's
     * constructed.
     * @return The instance of {@link AddToHomescreenMediator} that was constructed.
     */
    private AddToHomescreenMediator buildMediatorAndShowDialog() {
        PropertyModel model = new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(model, mWindowAndroid);
        PropertyModelChangeProcessor.create(model,
                initView(AppBannerManager.getHomescreenLanguageOption(mWebContents),
                        addToHomescreenMediator),
                AddToHomescreenViewBinder::bind);
        return addToHomescreenMediator;
    }

    /**
     * Creates and returns a {@link AddToHomescreenDialogView}.
     * Extracted into a separate method for easier testing.
     */
    @VisibleForTesting
    protected AddToHomescreenDialogView initView(AppBannerManager.InstallStringPair installStrings,
            AddToHomescreenViewDelegate delegate) {
        return new AddToHomescreenDialogView(
                mActivityContext, mModalDialogManager, installStrings, delegate);
    }

    protected ModalDialogManager getModalDialogManagerForTests() {
        return mModalDialogManager;
    }

    protected Context getContextForTests() {
        return mActivityContext;
    }
}
