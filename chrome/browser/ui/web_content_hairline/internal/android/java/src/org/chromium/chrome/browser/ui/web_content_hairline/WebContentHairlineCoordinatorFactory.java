// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_content_hairline;

import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;

/** Factory for creating a {@link WebContentHairlineCoordinator}. */
@NullMarked
public final class WebContentHairlineCoordinatorFactory {
    private WebContentHairlineCoordinatorFactory() {}

    /**
     * Creates a {@link WebContentHairlineCoordinator}.
     *
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} to observe top
     *     controls changes.
     * @param sideUiStateProvider The {@link SideUiStateProvider} to observe side UI changes.
     * @param incognitoStateProvider The {@link IncognitoStateProvider} to observe incognito state.
     * @param topControlsStacker The {@link TopControlsStacker} to query top controls layer state.
     * @param webContentHairlineContainerStub The {@link ViewStub} for the WebContents hairline
     *     container.
     * @return The newly-created {@link WebContentHairlineCoordinator}.
     */
    public static WebContentHairlineCoordinator create(
            BrowserControlsStateProvider browserControlsStateProvider,
            SideUiStateProvider sideUiStateProvider,
            IncognitoStateProvider incognitoStateProvider,
            TopControlsStacker topControlsStacker,
            ViewStub webContentHairlineContainerStub) {
        return new WebContentHairlineCoordinatorImpl(
                browserControlsStateProvider,
                sideUiStateProvider,
                incognitoStateProvider,
                topControlsStacker,
                webContentHairlineContainerStub);
    }
}
