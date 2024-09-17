// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/** Defines the data that is exposed to properly render the Toolbar. */
public interface ToolbarDataProvider {
    /** Observer interface for consumers who wish to subscribe to updates of ToolbarData. */
    interface Observer {
        default void onIncognitoStateChanged() {}
    }

    /** Adds an observer of changes to ToolbarDataProvider's data. */
    void addToolbarDataProviderObserver(Observer observer);

    /** Removes an observer of changes to ToolbarDataProvider's data. */
    void removeToolbarDataProviderObserver(Observer observer);

    /**
     * @return The tab that contains the information currently displayed in the toolbar.
     */
    @Nullable
    Tab getTab();

    /** Returns The url of the current tab. Returns empty string when there is no tab. */
    @NonNull
    String getCurrentUrl();

    /**
     * Returns The url of the current tab, represented as a GURL. Returns an empty GURL when there
     * is no tab.
     */
    @NonNull
    GURL getCurrentGurl();

    /** Returns the delegate for the NewTabPage shown for the current tab. */
    @NonNull
    NewTabPageDelegate getNewTabPageDelegate();

    /**
     * TODO(crbug.com/350654700): clean up usages and remove isIncognito.
     *
     * @return Whether the toolbar is currently being displayed for incognito.
     * @deprecated Use {@link #isIncognitoBranded()} or {@link #isOffTheRecord()}.
     */
    @Deprecated
    boolean isIncognito();

    /**
     * @return Whether the toolbar is currently being displayed for an incognito branded browser
     *     context.
     * @see {@link Profile#isIncognitoBranded()}
     */
    boolean isIncognitoBranded();

    /**
     * @return Whether the toolbar is currently being displayed for an off the record browser
     *     context.
     * @see {@link Profile#isOffTheRecord()}
     */
    boolean isOffTheRecord();

    /**
     * @return The current {@link Profile}.
     */
    Profile getProfile();

    /**
     * @return The contents of the {@link org.chromium.chrome.browser.omnibox.UrlBar}.
     */
    UrlBarData getUrlBarData();

    /**
     * @return The primary color to use for the background drawable.
     */
    @ColorInt
    int getPrimaryColor();

    /**
     * @return Whether the current primary color is a brand color.
     */
    boolean isUsingBrandColor();

    /**
     * Returns the resource ID of the icon that should be displayed or 0 if no icon should be shown.
     *
     * @param isTablet Whether or not the display context of the icon is a tablet.
     */
    @DrawableRes
    int getSecurityIconResource(boolean isTablet);

    /** Returns whether the page currently shown is a paint preview. */
    boolean isPaintPreview();
}
