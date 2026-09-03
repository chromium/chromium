// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.ColorStateList;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.security_state.ConnectionMaliciousContentStatus;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Central read-only data provider interface supplying contextual tab, page, and toolbar data to the
 * LocationBar / Omnibox and its sub-components (such as LocationBarMediator, StatusMediator,
 * AutocompleteMediator, HintTextUpdater, VoiceRecognitionHandler, and FuseboxSessionState).
 *
 * <p>Exposes properties such as the current URL ({@link GURL}), page title, connection security
 * level & malicious content status, page classification, primary and brand theme colors, incognito
 * / off-the-record state, offline / paint preview status, toolbar position supplier, {@link
 * NewTabPageDelegate}, and fusebox session state.
 *
 * <p>Provides an {@link Observer} mechanism to notify subscribers of state transitions (e.g. {@code
 * onTabChanged}, {@code onUrlChanged}, {@code onPrimaryColorChanged}, {@code
 * onSecurityStateChanged}, {@code onPageLoadStopped}, {@code onTitleChanged}, {@code
 * onIncognitoStateChanged}, {@code onTabCrashed}). Because data is typically computed lazily or on
 * demand, observer methods pass signals rather than data payloads; consumers are expected to pull
 * the specific data they require.
 *
 * <p><b>Strict Immutability:</b> This interface is intentionally immutable and read-only, and
 * <b>must remain immutable</b>. Do NOT add setters or mutator methods to {@code
 * LocationBarDataProvider}. Consumers of this interface should only observe and read state, never
 * modify it directly. All state modifications belong in concrete implementors such as {@code
 * LocationBarModel}.
 */
// TODO(crbug.com/40154848): Refine split between LocationBar properties and sub-component
// properties, e.g. security state, which is only used by the status icon.
@NullMarked
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

        /**
         * Notifies when the tab changed. This is guaranteed to be called before onUrlChanged().
         *
         * @param previousTab The tab that was active before this change. May be null if there was
         *     no previously selected tab.
         */
        default void onTabChanged(@Nullable Tab previousTab) {}

        /**
         * Notifies when the URL changed.
         *
         * @param isTabChanging whether this URL change event was caused by a tab change.
         */
        default void onUrlChanged(boolean isTabChanging) {}

        default void hintZeroSuggestRefresh() {}

        /** Notifies when the tab crashes. */
        default void onTabCrashed() {}

        /** Notifies when whether the current URL has an installed app might have changed. */
        default void onAppInstallationStateChanged() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        AppInstallState.NOT_INSTALLED,
        AppInstallState.INSTALLED,
        AppInstallState.PENDING_INSTALL,
    })
    @interface AppInstallState {
        int NOT_INSTALLED = 0;
        int INSTALLED = 1;
        int PENDING_INSTALL = 2;
    }

    /** Delegate to resolve the app install state for a tab. */
    interface AppInstalledDelegate {
        @AppInstallState
        int getAppInstallState(@Nullable Tab tab);

        default void addObserver(Runnable observer) {}

        default void removeObserver(Runnable observer) {}
    }

    /** Adds an observer of changes to LocationBarDataProvider's data. */
    void addObserver(Observer observer);

    /** Removes an observer of changes to LocationBarDataProvider's data. */
    void removeObserver(Observer observer);

    /**
     * Returns the url of the current tab, represented as a GURL. Returns an empty GURL when there
     * is no tab.
     */
    GURL getCurrentGurl();

    /** Returns the delegate for the NewTabPage shown for the current tab. */
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
    @Nullable Tab getTab();

    /**
     * Returns the FuseboxSessionState linked to the current tab (if present) or context
     * (otherwise).
     */
    @Nullable FuseboxSessionState getFuseboxSessionState();

    /** Returns whether the LocationBarDataProvider currently has an active tab. */
    boolean hasTab();

    /** Returns the active WebContents. */
    default @Nullable WebContents getWebContents() {
        return null;
    }

    /** Returns the contents of the {@link UrlBar}. */
    UrlBarData getUrlBarData();

    /** Returns the title of the current page, or the empty string if there is currently no tab. */
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

    /** Returns the current {@link ConnectionMaliciousContentStatus}. */
    @ConnectionMaliciousContentStatus
    int getMaliciousContentStatus();

    /**
     * Returns the current page classification.
     *
     * @param prefetch whether retrieving page class in prefetch context.
     * @return Integer value representing the {@code PageClassification}.
     */
    @PageClassification
    int getPageClassification(boolean prefetch);

    /**
     * Returns the resource ID of the icon that should be displayed or 0 if no icon should be shown.
     */
    @DrawableRes
    int getSecurityIconResource(boolean isTablet);

    /** Returns The {@link ColorStateList} to use to tint the security state icon. */
    @ColorRes
    int getSecurityIconColorStateList();

    /** Returns the resource ID of the content description for the security icon. */
    @StringRes
    int getSecurityIconContentDescriptionResourceId();

    /** Returns the user-selected placement of the Toolbar. */
    NonNullObservableSupplier<@ControlsPosition Integer> getToolbarPositionSupplier();

    /** Returns the app install state for the current tab. */
    default @AppInstallState int getAppInstallState() {
        return AppInstallState.NOT_INSTALLED;
    }
}
