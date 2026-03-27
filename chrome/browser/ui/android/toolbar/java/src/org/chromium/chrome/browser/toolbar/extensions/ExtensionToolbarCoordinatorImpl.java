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
import android.widget.TextView;

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
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;
    private ExtensionAccessControlButtonCoordinator mExtensionAccessControlButtonCoordinator;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMenuButtonChangeProcessor;

    private final MenuButtonWidthConsumer mMenuButtonWidthConsumer = new MenuButtonWidthConsumer();
    private final RequestAccessButtonWidthConsumer mRequestAccessButtonWidthConsumer =
            new RequestAccessButtonWidthConsumer();
    private final ActionListWidthConsumer mActionListWidthConsumer = new ActionListWidthConsumer();
    private final PoppedOutActionWidthConsumer mPoppedOutActionWidthConsumer =
            new PoppedOutActionWidthConsumer();

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
            ViewGroup rootView,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate) {
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
                        rootView,
                        contextMenuPopulatorFactory,
                        selectionDropdownMenuDelegate);
        mModel =
                new PropertyModel.Builder(
                                PropertyModel.concatKeys(
                                        ExtensionsMenuProperties.ALL_KEYS,
                                        ExtensionsToolbarProperties.ALL_KEYS))
                        .build();
        mMenuButtonChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        mContainer.findViewById(R.id.extensions_menu_button),
                        ExtensionsMenuButtonViewBinder::bind);

        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(
                        context,
                        mContainer.findViewById(R.id.extensions_menu_button),
                        themeColorProvider,
                        task,
                        profile,
                        currentTabSupplier,
                        tabCreator,
                        mExtensionsToolbarBridge,
                        mModel);
        mExtensionAccessControlButtonCoordinator =
                new ExtensionAccessControlButtonCoordinator(
                        mModel,
                        currentTabSupplier,
                        mExtensionsToolbarBridge,
                        (TextView) mContainer.findViewById(R.id.extensions_request_access_button),
                        (v) -> {});
    }

    @Override
    public void destroy() {
        mMenuButtonChangeProcessor.destroy();
        mExtensionAccessControlButtonCoordinator.destroy();
        mExtensionsMenuCoordinator.destroy();
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

        mExtensionActionListCoordinator.executeUserAction(
                result.actionId, InvocationSource.COMMAND);
        return true;
    }

    @Override
    public void updateMenuButtonBackground(int backgroundResource) {
        mModel.set(
                ExtensionsToolbarProperties.EXTENSIONS_MENU_BUTTON_DEFAULT_BACKGROUND,
                backgroundResource);
    }

    @Override
    public void showExtensionsMenu() {
        ListMenuButton extensionsMenuButton = mContainer.findViewById(R.id.extensions_menu_button);
        assert extensionsMenuButton != null;

        extensionsMenuButton.performClick();
    }

    @Override
    public PoppedOutActionWidthConsumer getPoppedOutActionWidthConsumer() {
        return mPoppedOutActionWidthConsumer;
    }

    @Override
    public ToolbarWidthConsumer getMenuButtonWidthConsumer() {
        return mMenuButtonWidthConsumer;
    }

    @Override
    public ToolbarWidthConsumer getRequestAccessButtonWidthConsumer() {
        return mRequestAccessButtonWidthConsumer;
    }

    @Override
    public ToolbarWidthConsumer getActionListWidthConsumer() {
        return mActionListWidthConsumer;
    }

    private class PoppedOutActionWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return mExtensionActionListCoordinator.hasPoppedOutAction();
        }

        @Override
        public int updateVisibility(int availableWidth) {
            // Do not update the UI here just yet. We will leave that to {@link
            // ActionListWidthConsumer}, which will be called but later because it has lower
            // priority.
            return mExtensionActionListCoordinator.setCanShowPoppedOutAction(availableWidth);
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
    }

    private class RequestAccessButtonWidthConsumer implements ToolbarWidthConsumer {
        @Override
        public boolean isVisible() {
            return mModel.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE);
        }

        private void setHasSpaceToShow(boolean hasSpaceToShow) {
            int visibility = hasSpaceToShow ? View.VISIBLE : View.GONE;
            mContainer
                    .findViewById(R.id.extensions_request_access_button)
                    .setVisibility(visibility);
        }

        @Override
        public int updateVisibility(int availableWidth) {
            if (!isVisible()) {
                setHasSpaceToShow(false);
                return 0;
            }

            TextView requestAccessButton =
                    mContainer.findViewById(R.id.extensions_request_access_button);

            requestAccessButton.measure(
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            int buttonWidth = requestAccessButton.getMeasuredWidth();

            boolean hasSpaceToShow = buttonWidth <= availableWidth;
            setHasSpaceToShow(hasSpaceToShow);

            // TODO(crbug.com/473396591): Add styling and width adjustments for Clank message which
            // appears where the access button should appear, but the menu puzzle icon is unpinned
            // as well as when the window size is compact and there isn't enough space to show the
            // button.
            return Math.min(availableWidth, buttonWidth);
        }

        @Override
        public int updateVisibilityWithAnimation(
                int availableWidth, Collection<Animator> animators) {
            return updateVisibility(availableWidth);
        }
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
