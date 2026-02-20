// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.animation.Animator;
import android.content.Context;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;

import java.util.Collection;

/** The implementation of {@link ExtensionToolbarCoordinator}. */
@NullMarked
@ServiceImpl(ExtensionToolbarCoordinator.class)
public class ExtensionToolbarCoordinatorImpl implements ExtensionToolbarCoordinator {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    // TODO(crbug.com/473396591): Remove once {link ExtensionActionsBridge} is deprecated.
    private ExtensionActionsBridge mBridge;

    private LinearLayout mContainer;
    private ExtensionsToolbarBridge mExtensionsToolbarBridge;
    private ExtensionActionListCoordinator mExtensionActionListCoordinator;
    private ExtensionsMenuAndAccessControlButtonCoordinator
            mExtensionsMenuAndAccessControlButtonCoordinator;

    private final MenuButtonWidthConsumer mMenuButtonWidthConsumer = new MenuButtonWidthConsumer();
    private final ActionListWidthConsumer mActionListWidthConsumer = new ActionListWidthConsumer();

    @Override
    public void initializeWithNative(
            Context context,
            ViewStub extensionToolbarStub,
            WindowAndroid windowAndroid,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            TabCreator tabCreator,
            ThemeColorProvider themeColorProvider,
            ViewGroup rootView) {
        mBridge = new ExtensionActionsBridge(task, profile);

        extensionToolbarStub.setLayoutResource(R.layout.extension_toolbar_container);
        mContainer = (LinearLayout) extensionToolbarStub.inflate();

        mExtensionsToolbarBridge = new ExtensionsToolbarBridge(task, profile);

        mExtensionActionListCoordinator =
                new ExtensionActionListCoordinator(
                        context,
                        mContainer.findViewById(R.id.extension_action_list),
                        windowAndroid,
                        task,
                        profile,
                        currentTabSupplier,
                        mExtensionsToolbarBridge,
                        rootView);
        mExtensionsMenuAndAccessControlButtonCoordinator =
                new ExtensionsMenuAndAccessControlButtonCoordinator(
                        context,
                        mContainer.findViewById(R.id.extensions_menu_button),
                        themeColorProvider,
                        task,
                        profile,
                        currentTabSupplier,
                        tabCreator,
                        mExtensionsToolbarBridge,
                        mContainer.findViewById(R.id.extensions_request_access_button));
    }

    @Override
    public void destroy() {
        mExtensionsMenuAndAccessControlButtonCoordinator.destroy();
        mExtensionActionListCoordinator.destroy();
        mExtensionsToolbarBridge.destroy();
        mBridge.destroy();

        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Filter out events we are not interested in before calling into JNI.
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() > 0) {
            return false;
        }

        ExtensionActionsBridge.HandleKeyEventResult result = mBridge.handleKeyDownEvent(event);
        if (result.handled) {
            return true;
        }
        if (result.actionId.isEmpty()) {
            return false;
        }

        mExtensionActionListCoordinator.click(result.actionId);
        return true;
    }

    @Override
    public void updateMenuButtonBackground(int backgroundResource) {
        mExtensionsMenuAndAccessControlButtonCoordinator.updateButtonBackground(backgroundResource);
    }

    @Override
    public ToolbarWidthConsumer getMenuButtonWidthConsumer() {
        return mMenuButtonWidthConsumer;
    }

    @Override
    public ToolbarWidthConsumer getActionListWidthConsumer() {
        return mActionListWidthConsumer;
    }

    private class MenuButtonWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            ListMenuButton menuButton = mContainer.findViewById(R.id.extensions_menu_button);
            return menuButton.getVisibility() == View.VISIBLE;
        }

        private void setHasSpaceToShow(boolean hasSpaceToShow) {
            int visibility = hasSpaceToShow ? View.VISIBLE : View.GONE;
            mContainer.findViewById(R.id.extensions_menu_button).setVisibility(visibility);
            mContainer.findViewById(R.id.extensions_divider).setVisibility(visibility);
        }

        @Override
        public int updateVisibility(int availableWidth) {
            int puzzleButtonWidth =
                    mContainer
                            .getResources()
                            .getDimensionPixelSize(
                                    org.chromium.chrome.browser.toolbar.R.dimen
                                            .toolbar_button_width);
            int toolbarDividerWidth =
                    mContainer
                            .getResources()
                            .getDimensionPixelSize(
                                    org.chromium.chrome.browser.toolbar.R.dimen
                                            .toolbar_divider_width);
            int totalWidth = puzzleButtonWidth + toolbarDividerWidth;

            setHasSpaceToShow(totalWidth <= availableWidth);
            return Math.min(availableWidth, totalWidth);
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    private class ActionListWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return mContainer.findViewById(R.id.extension_action_list).getVisibility()
                    == View.VISIBLE;
        }

        @Override
        public int updateVisibility(int availableWidth) {
            return mExtensionActionListCoordinator.fitActionsWithinWidth(availableWidth);
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }
}
