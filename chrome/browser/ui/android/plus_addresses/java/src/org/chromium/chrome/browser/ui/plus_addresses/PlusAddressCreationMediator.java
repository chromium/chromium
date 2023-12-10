// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/**
 * Mediator class for the plus address creation UI.
 *
 * <p>This component shows a bottom sheet to let the user create a plus address.
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiShown, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class PlusAddressCreationMediator extends EmptyBottomSheetObserver
        implements PlusAddressCreationDelegate, TabModelObserver, LayoutStateObserver {
    private final PlusAddressCreationBottomSheetContent mBottomSheetContent;
    private final BottomSheetController mBottomSheetController;
    private final LayoutStateProvider mLayoutStateProvider;
    private final TabModelSelector mTabModelSelector;
    private final TabModel mTabModel;
    private final PlusAddressCreationViewBridge mBridge;

    /**
     * Creates the mediator.
     *
     * @param bottomSheetContent The bottom sheet content to be shown.
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     * @param tabModelSelector The TabModelSelector for tabModel
     * @param bridge The bridge to signal UI flow events (onConfirmed, onCanceled, etc.) to.
     */
    PlusAddressCreationMediator(
            PlusAddressCreationBottomSheetContent bottomSheetContent,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            TabModelSelector tabModelSelector,
            PlusAddressCreationViewBridge bridge) {
        mBottomSheetContent = bottomSheetContent;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProvider = layoutStateProvider;
        mTabModel = tabModel;
        mTabModelSelector = tabModelSelector;
        mBridge = bridge;

        mBottomSheetContent.setDelegate(this);
        mBottomSheetController.addObserver(this);
        mLayoutStateProvider.addObserver(this);
        mTabModel.addObserver(this);
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    void updateProposedPlusAddress(String plusAddress) {
        mBottomSheetContent.setProposedPlusAddress(plusAddress);
    }

    void showError(String message) {
        mBottomSheetContent.showError(message);
    }

    /** Hide the bottom sheet (if showing) and clean up observers. */
    void destroy() {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
        mBottomSheetController.removeObserver(this);
        mLayoutStateProvider.removeObserver(this);
        mTabModel.removeObserver(this);
    }

    // PlusAddressCreationDelegate implementation:
    @Override
    public void onConfirmRequested() {
        mBridge.onConfirmRequested();
    }

    @Override
    public void onConfirmFinished() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void onCanceled() {
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        mBridge.onCanceled();
    }

    @Override
    public void onPromptDismissed() {
        mBridge.onPromptDismissed();
    }

    @Override
    public void openManagementPage(GURL url) {
        mTabModelSelector.openNewTab(
                new LoadUrlParams(url.getSpec()),
                TabLaunchType.FROM_LINK,
                mTabModelSelector.getCurrentTab(),
                /* incognito= */ false);
    }

    // EmptyBottomSheetObserver overridden methods follow:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        this.onPromptDismissed();
    }

    // TabModelObserver
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        // While the bottom sheet scrim covers the omnibox UI, a new tab can be created in other
        // ways such as by opening a link from another app. In this case we want to hide the bottom
        // sheet rather than keeping the bottom sheet open while this tab loads behind the scrim.
        if (lastId != tab.getId()) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
        }
    }

    // LayoutStateObserver
    @Override
    public void onStartedShowing(@LayoutType int layoutType) {
        // When the browser layout changes away from browsing to say the tab switcher, then the
        // bottom sheet must be hidden.
        if (layoutType != LayoutType.BROWSING) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
        }
    }
}
