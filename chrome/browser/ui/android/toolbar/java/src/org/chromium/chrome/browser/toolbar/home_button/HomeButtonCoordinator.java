// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.MenuBuilderHelper;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.util.function.BooleanSupplier;

/**
 * Root component for the {@link HomeButton} on the toolbar. Currently owns showing IPH and context
 * menu for the home button.
 */
// TODO(crbug.com/1056422): Fix the visibility bug on NTP.
public class HomeButtonCoordinator {
    private static final int ID_SETTINGS = 0;

    private final Context mContext;
    private final HomeButton mHomeButton;
    private final BooleanSupplier mIsFeedEnabled;
    private final UserEducationHelper mUserEducationHelper;
    private final BooleanSupplier mIsIncognitoSupplier;
    private final CurrentTabObserver mPageLoadObserver;
    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final Supplier<Boolean> mIsHomepageNonNtpSupplier;
    private final Supplier<Boolean> mIsHomeButtonMenuDisabled;

    private final Callback<Context> mOnMenuClickCallback;
    private MVCListAdapter.ModelList mMenuList;
    private @Nullable ListMenuButtonDelegate mListMenuButtonDelegate;

    /**
     * @param context The Android context used for various view operations.
     * @param homeButton The concrete {@link View} class for this MVC component.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param isIncognitoSupplier Supplier for whether the current tab is incognito.
     * @param promoShownOneshotSupplier Potentially delayed information about if a promo was shown.
     * @param isHomepageNonNtpSupplier Supplier for whether the current homepage is not NTP.
     * @param isFeedEnabled Supplier for whether feed is enabled.
     * @param tabSupplier Supplier of the activity tab.
     * @param onMenuClickCallback Callback when home button menu item is clicked.
     * @param isHomepageMenuDisabledSupplier Supplier for whether the home button menu is enabled.
     */
    public HomeButtonCoordinator(
            @NonNull Context context,
            @NonNull View homeButton,
            @NonNull UserEducationHelper userEducationHelper,
            @NonNull BooleanSupplier isIncognitoSupplier,
            @NonNull OneshotSupplier<Boolean> promoShownOneshotSupplier,
            @NonNull Supplier<Boolean> isHomepageNonNtpSupplier,
            @NonNull BooleanSupplier isFeedEnabled,
            @NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull Callback<Context> onMenuClickCallback,
            @NonNull Supplier<Boolean> isHomepageMenuDisabledSupplier) {
        mContext = context;
        mHomeButton = (HomeButton) homeButton;
        mUserEducationHelper = userEducationHelper;
        mIsIncognitoSupplier = isIncognitoSupplier;
        mPromoShownOneshotSupplier = promoShownOneshotSupplier;
        mIsHomepageNonNtpSupplier = isHomepageNonNtpSupplier;
        mIsFeedEnabled = isFeedEnabled;
        mPageLoadObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onPageLoadFinished(Tab tab, GURL url) {
                                // Part of scroll jank investigation http://crbug.com/1311003. Will
                                // remove
                                // TraceEvent after the investigation is complete.
                                try (TraceEvent te =
                                        TraceEvent.scoped(
                                                "HomeButtonCoordinator::onPageLoadFinished")) {
                                    handlePageLoadFinished(url);
                                }
                            }
                        },
                        /* swapCallback= */ null);

        mOnMenuClickCallback = onMenuClickCallback;
        mIsHomeButtonMenuDisabled = isHomepageMenuDisabledSupplier;
        mHomeButton.setOnLongClickListener(this::onLongClickHomeButton);
    }

    /** Cleans up observers. */
    public void destroy() {
        mPageLoadObserver.destroy();
    }

    /**
     * @param url The URL of the current page that was just loaded.
     */
    @VisibleForTesting
    void handlePageLoadFinished(GURL url) {
        if (!mHomeButton.isShown()) return;
        if (mIsIncognitoSupplier.getAsBoolean()) return;
        if (UrlUtilities.isNTPUrl(url)) return;
        if (mIsHomepageNonNtpSupplier.get()) return;
        if (mPromoShownOneshotSupplier.get() == null || mPromoShownOneshotSupplier.get()) return;

        boolean hasFeed = mIsFeedEnabled.getAsBoolean();
        int textId = hasFeed ? R.string.iph_ntp_with_feed_text : R.string.iph_ntp_without_feed_text;
        int accessibilityTextId =
                hasFeed
                        ? R.string.iph_ntp_with_feed_accessibility_text
                        : R.string.iph_ntp_without_feed_accessibility_text;

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
                                textId,
                                accessibilityTextId)
                        .setAnchorView(mHomeButton)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .build());
    }

    @VisibleForTesting
    boolean onLongClickHomeButton(View view) {
        if (view != mHomeButton || mIsHomeButtonMenuDisabled.get()) return false;

        if (mListMenuButtonDelegate == null) {
            RectProvider rectProvider = MenuBuilderHelper.getRectProvider(mHomeButton);
            mMenuList = new MVCListAdapter.ModelList();
            mMenuList.add(
                    buildMenuListItem(
                            R.string.options_homepage_edit_title,
                            ID_SETTINGS,
                            R.drawable.ic_edit_24dp));
            BasicListMenu listMenu =
                    new BasicListMenu(
                            mContext,
                            mMenuList,
                            (model) -> mOnMenuClickCallback.onResult(mContext));
            mListMenuButtonDelegate =
                    new ListMenuButtonDelegate() {
                        @Override
                        public ListMenu getListMenu() {
                            return listMenu;
                        }

                        @Override
                        public RectProvider getRectProvider(View listMenuButton) {
                            return rectProvider;
                        }
                    };
            mHomeButton.setDelegate(mListMenuButtonDelegate, false);
        }
        mHomeButton.showMenu();
        return true;
    }

    public MVCListAdapter.ModelList getMenuForTesting() {
        return mMenuList;
    }
}
