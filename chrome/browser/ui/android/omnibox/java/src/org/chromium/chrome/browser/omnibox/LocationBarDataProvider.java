// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.ColorStateList;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.url.GURL;

/** Interface defining a provider for data needed by the {@link LocationBar}. */
// TODO(crbug.com/40154848): Refine split between LocationBar properties and sub-component
// properties, e.g. security state, which is only used by the status icon.
public interface LocationBarDataProvider {
    /**
     * Observer interface for consumers who wish to subscribe to updates of LocationBarData. Since
     * LocationBarDataProvider data is typically calculated lazily, individual observer methods
     * don't directly supply the updated value. Instead, the expectation is that the consumer will
     * query the data it cares about.
     */
    interface Observer {
        default void onIncognitoStateChanged() {}

        default void onNtpStartedLoading() {}

        /**
         * Notifies about a possible change of the value of {@link #getPrimaryColor()}, or {@link
         * #isUsingBrandColor()}.
         */
        default void onPrimaryColorChanged() {}

        /** Notifies about possible changes to values affecting the status icon. */
        default void onSecurityStateChanged() {}

        /** Notifies when the page stopped loading. */
        default void onPageLoadStopped() {}

        default void onTitleChanged() {}

        default void onUrlChanged() {}

        default void hintZeroSuggestRefresh() {}

        /** Notifies when the tab crashes. */
        default void onTabCrashed() {}
    }

    /** Adds an observer of changes to LocationBarDataProvider's data. */
    void addObserver(Observer observer);

    /** Removes an observer of changes to LocationBarDataProvider's data. */
    void removeObserver(Observer observer);

    /**
     * Returns the url of the current tab, represented as a GURL. Returns an empty GURL when there
     * is no tab.
     */
    @NonNull
    GURL getCurrentGurl();

    /** Returns the delegate for the NewTabPage shown for the current tab. */
    @NonNull
    NewTabPageDelegate getNewTabPageDelegate();

    /** Returns whether the currently active page is loading. */
    default boolean isLoading() {
        Tab tab = getTab();
        return tab != null && tab.isLoading();
    }

    /**
     * TODO(crbug.com/350654700): clean up usages and remove isIncognito.
     *
     * <p>Returns whether the current page is in an incognito browser context.
     *
     * @deprecated Use {@link #isIncognitoBranded()} or {@link #isOffTheRecord()}.
     */
    @Deprecated
    boolean isIncognito();

    /**
     * Returns whether the current page is in an incognito branded browser context.
     *
     * @see {@link Profile#isIncognitoBranded()}
     */
    boolean isIncognitoBranded();

    /**
     * Returns whether the current page is in an off the record browser context.
     *
     * @see {@link Profile#isOffTheRecord()}
     */
    boolean isOffTheRecord();

    /** Returns the currently active tab, if there is one. */
    @Nullable
    Tab getTab();

    /** Returns whether the LocationBarDataProvider currently has an active tab. */
    boolean hasTab();

    /** Returns the contents of the {@link UrlBar}. */
    UrlBarData getUrlBarData();

    /** Returns the title of the current page, or the empty string if there is currently no tab. */
    @NonNull
    String getTitle();

    /** Returns the primary color to use for the background. */
    int getPrimaryColor();

    /** Returns whether the current primary color is a brand color. */
    boolean isUsingBrandColor();

    /** Returns whether the page currently shown is an offline page. */
    boolean isOfflinePage();

    /** Returns whether the page currently shown is a paint preview. */
    default boolean isPaintPreview() {
        return false;
    }

    /** Returns the current {@link ConnectionSecurityLevel}. */
    @ConnectionSecurityLevel
    int getSecurityLevel();

    /**
     * Returns the current page classification.
     *
     * @param isPrefetch If the page classification for prefetching is requested.
     * @return Integer value representing the {@code OmniboxEventProto.PageClassification}.
     */
    int getPageClassification(boolean isPrefetch);

    /**
     * Returns the resource ID of the icon that should be displayed or 0 if no icon should be shown.
     *
     * @param isTablet Whether or not the display context of the icon is a tablet.
     */
    @DrawableRes
    int getSecurityIconResource(boolean isTablet);

    /** Returns The {@link ColorStateList} to use to tint the security state icon. */
    @ColorRes
    int getSecurityIconColorStateList();

    /** Returns the resource ID of the content description for the security icon. */
    @StringRes
    int getSecurityIconContentDescriptionResourceId();
}
