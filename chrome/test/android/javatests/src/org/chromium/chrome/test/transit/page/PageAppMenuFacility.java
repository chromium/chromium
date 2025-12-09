// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.junit.Assert.assertNotNull;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabbed_mode.TabbedAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.test.transit.CtaAppMenuFacility;
import org.chromium.chrome.test.transit.bookmarks.BookmarksPhoneStation;
import org.chromium.chrome.test.transit.bookmarks.BookmarksTabletStation;
import org.chromium.chrome.test.transit.hub.TabGroupListBottomSheetFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.Set;

/**
 * The app menu shown when pressing ("...") in a Tab.
 *
 * <p>Use subclasses to access menu items not shared between all {@link CtaPageStation} types:
 *
 * <ul>
 *   <li>{@link RegularNewTabPageAppMenuFacility}
 *   <li>{@link IncognitoNewTabPageAppMenuFacility}
 *   <li>{@link RegularWebPageAppMenuFacility}
 *   <li>{@link IncognitoWebPageAppMenuFacility}
 * </ul>
 *
 * @param <HostPageStationT> the type of host {@link CtaPageStation} where this app menu is opened.
 */
public class PageAppMenuFacility<HostPageStationT extends CtaPageStation>
        extends CtaAppMenuFacility<HostPageStationT> {

    protected @Nullable Item mNewTab;
    protected @Nullable Item mNewIncognitoTab;
    protected @Nullable Item mNewIncognitoWindow;
    protected @Nullable Item mAddToGroup;
    protected @Nullable Item mReaderMode;
    protected Item mNewWindow;
    protected Item mBookmarks;
    protected Item mSettings;

    @Override
    protected void declareItems(ItemsBuilder items) {
        // TODO: Declare top buttons (forward, reload, bookmark, etc.).
        // TODO: Declare more common menu items

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            if (!mHostStation.isIncognito()) {
                mNewTab = declareMenuItem(items, NEW_TAB_ID);
            } else {
                mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);
            }
        } else {
            mNewTab = declareMenuItem(items, NEW_TAB_ID);
            mNewIncognitoTab = declareMenuItem(items, NEW_INCOGNITO_TAB_ID);
        }

        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            mAddToGroup = declareMenuItem(items, ADD_TO_GROUP_ID);
        }
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            mNewWindow = declareMenuItem(items, NEW_WINDOW_ID);
            mNewIncognitoWindow = declareMenuItem(items, NEW_INCOGNITO_WINDOW_ID);
        }
        mSettings = declareMenuItem(items, SETTINGS_ID);
    }

    /** Select "New tab" from the app menu. If this doesn't exist, select "New window". */
    public RegularNewTabPageStation openNewTab() {
        if (mNewTab != null) {
            return mNewTab.scrollToAndSelectTo().arriveAt(createNewTabPageStation());
        } else {
            assert mNewWindow != null : "App menu does not have 'New tab' or 'New window'";
            return openNewWindow();
        }
    }

    /**
     * Select "New Incognito tab" from the app menu. If this doesn't exist, select "New Incognito
     * window".
     */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        if (mNewIncognitoTab != null) {
            return mNewIncognitoTab
                    .scrollToAndSelectTo()
                    .arriveAt(createNewIncognitoTabPageStation());
        } else {
            assert mNewIncognitoWindow != null
                    : "App menu does not have 'New Incognito tab' or 'New Incognito window'";
            return openNewIncognitoWindow();
        }
    }

    /** Select "New window" from the app menu. */
    public RegularNewTabPageStation openNewWindow() {
        TabbedAppMenuPropertiesDelegate delegate = getTabbedAppMenuPropertiesDelegate();
        assert delegate.shouldShowNewWindow() : "App menu is not expected to show 'New window'";
        return mNewWindow.scrollToAndSelectTo().inNewTask().arriveAt(createNewWindowStation());
    }

    /** Select "New Incognito window" from the app menu. */
    public IncognitoNewTabPageStation openNewIncognitoWindow() {
        TabbedAppMenuPropertiesDelegate delegate = getTabbedAppMenuPropertiesDelegate();
        assert delegate.shouldShowNewIncognitoWindow()
                : "App menu is not expected to show 'New Incognito window'";
        assert mNewIncognitoWindow != null;
        return mNewIncognitoWindow
                .scrollToAndSelectTo()
                .inNewTask()
                .arriveAt(createNewIncognitoWindowStation());
    }

    /**
     * Select "Add to group" from the app menu. This opens a bottom sheet to add the current tab to
     * a tab group.
     */
    public TabGroupListBottomSheetFacility<HostPageStationT> selectAddToGroupWithBottomSheet() {
        assertNotNull(mAddToGroup);

        TabGroupModelFilter tabGroupModelFilter = mHostStation.getTabGroupModelFilter();
        Set<Token> tabGroupIds =
                ThreadUtils.runOnUiThreadBlocking(() -> tabGroupModelFilter.getAllTabGroupIds());
        return mAddToGroup
                .scrollToAndSelectTo()
                .enterFacility(
                        new TabGroupListBottomSheetFacility<>(
                                new ArrayList<>(tabGroupIds), /* isNewTabGroupRowVisible= */ true));
    }

    private TabbedAppMenuPropertiesDelegate getTabbedAppMenuPropertiesDelegate() {
        return (TabbedAppMenuPropertiesDelegate)
                mHostStation
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getAppMenuCoordinatorForTesting()
                        .getAppMenuPropertiesDelegate();
    }

    /** Select "Bookmarks" from the app menu in tablets. */
    public BookmarksTabletStation openBookmarksTablet() {
        assert DeviceFormFactor.isNonMultiDisplayContextOnTablet(mHostStation.getActivity());
        return mBookmarks
                .scrollToAndSelectTo()
                .arriveAt(BookmarksTabletStation.newBuilder().initOpeningNewTab().build());
    }

    /** Select "Bookmarks" from the app menu in phones. */
    public BookmarksPhoneStation openBookmarksPhone() {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mHostStation.getActivity());
        return mBookmarks.scrollToAndSelectTo().arriveAt(new BookmarksPhoneStation());
    }

    /** Select "Settings" from the app menu. */
    public SettingsStation openSettings() {
        return mSettings.scrollToAndSelectTo().arriveAt(createSettingsStation());
    }

    /** Select "Show Reading Mode" from the app menu. */
    public WebPageStation enterReaderMode() {
        return mReaderMode
                .scrollToAndSelectTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initForLoadingUrlOnSameTab("chrome-distiller://", mHostStation)
                                .build());
    }
}
