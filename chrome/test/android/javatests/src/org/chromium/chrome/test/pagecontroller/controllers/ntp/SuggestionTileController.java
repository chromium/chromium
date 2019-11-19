// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import static org.chromium.chrome.test.pagecontroller.utils.Ui2Locators.withText;

import android.support.test.uiautomator.UiObject2;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.ElementController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

import java.util.List;

/**
 * Suggestions Tile (below the Omnibox) Element Controller.
 */
public class SuggestionTileController extends ElementController {
    /**
     * Represents a single tile, can be used by the NewTabPageController
     * to perform actions.
     */
    public static class Info {
        private String mTitle;

        public Info(String title) {
            mTitle = title;
        }

        public String getTitle() {
            return mTitle;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof Info)) return false;
            Info info = (Info) o;
            return mTitle.equals(info.mTitle);
        }

        @Override
        public int hashCode() {
            return mTitle.hashCode();
        }

        @Override
        public String toString() {
            return "Info{"
                    + "mTitle='" + mTitle + '\'' + '}';
        }
    }

    private static final IUi2Locator LOCATOR_TILE_TITLES =
            Ui2Locators.withPath(Ui2Locators.withAnyResEntry(R.id.tile_grid_layout),
                    Ui2Locators.withAnyResEntry(R.id.tile_view_title));
    private static final IUi2Locator LOCATOR_TILE_TITLE_TEXT = Ui2Locators.withTextRegex(".+");

    private static final SuggestionTileController sInstance = new SuggestionTileController();
    private SuggestionTileController() {}
    public static SuggestionTileController getInstance() {
        return sInstance;
    }

    /**
     * Get list of tiles.
     * @return  List of Infos representing each tile on the screen, or empty list if non are found.
     */
    public List<Info> parseScreen() {
        List<String> titles = mLocatorHelper.getAllTexts(LOCATOR_TILE_TITLES);
        List<Info> infos = mLocatorHelper.getCustomElements(
                LOCATOR_TILE_TITLES, new UiLocatorHelper.CustomElementMaker<Info>() {
                    @Override
                    public Info makeElement(UiObject2 root, boolean isLastAttempt) {
                        String title =
                                mLocatorHelper.getOneTextImmediate(LOCATOR_TILE_TITLE_TEXT, root);
                        if (title != null) {
                            return new Info(title);
                        } else if (!isLastAttempt) {
                            // Not the last attempt yet, so makeElement will be
                            // called again if we throw an exception.  This
                            // gives a chance for the title UI of the tile to
                            // load.
                            throw new UiLocationException(
                                    "Title not found.", LOCATOR_TILE_TITLES, root);
                        } else {
                            // This is the last attempt.  It is possible that
                            // no complete tiles are found on the screen, just
                            // return null to signal that no tile was found.
                            return null;
                        }
                    }
                });

        return infos;
    }

    public IUi2Locator getLocator(Info tileInfo) {
        return Ui2Locators.withPath(LOCATOR_TILE_TITLES, withText(tileInfo.getTitle()));
    }
}
