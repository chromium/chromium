// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.ui.UiUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuHost;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;
import java.util.function.Supplier;

/** A popup that handles displaying the navigation history for a given tab. */
@NullMarked
public class NavigationPopup implements AdapterView.OnItemClickListener {
    private static final int MAXIMUM_HISTORY_ITEMS = 8;
    private static final int FULL_HISTORY_ENTRY_INDEX = -1;

    /** Specifies the type of navigation popup being shown */
    @IntDef({Type.TABLET_BACK, Type.TABLET_FORWARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int TABLET_BACK = 0;
        int TABLET_FORWARD = 1;
    }

    /** Delegate to display navigation history. */
    public interface HistoryDelegate {
        /**
         * Show navigation history.
         *
         * @param tab The tab whose navigation history is to used.
         */
        void show(Tab tab);
    }

    private final Profile mProfile;
    private final Context mContext;
    private final ListPopupWindow mPopup;
    private final NavigationController mNavigationController;
    private final NavigationHistory mHistory;
    private final NavigationAdapter mAdapter;
    private final @Type int mType;
    private final int mFaviconSize;
    private final Supplier<@Nullable Tab> mCurrentTabSupplier;
    private final HistoryDelegate mHistoryDelegate;
    private final ModelList mListItems = new ModelList();

    private @Nullable ListMenuHost mListMenuHost;

    private DefaultFaviconHelper mDefaultFaviconHelper;

    /** Loads the favicons asynchronously. */
    private FaviconHelper mFaviconHelper;

    private @Nullable Runnable mOnDismissCallback;

    private boolean mInitialized;

    /**
     * Constructs a new popup with the given history information.
     *
     * @param profile The profile used for fetching favicons.
     * @param context The context used for building the popup.
     * @param navigationController The controller which takes care of page navigations.
     * @param type The type of navigation popup being triggered.
     * @param currentTabSupplier Supplies the current tab.
     * @param historyDelegate Delegate used to display navigation history.
     */
    @SuppressWarnings("NullAway")
    public NavigationPopup(
            Profile profile,
            Context context,
            @Nullable NavigationController navigationController,
            @Type int type,
            Supplier<@Nullable Tab> currentTabSupplier,
            HistoryDelegate historyDelegate) {
        mProfile = profile;
        mContext = context;
        Resources resources = mContext.getResources();
        mNavigationController = navigationController;
        mType = type;
        mCurrentTabSupplier = currentTabSupplier;
        mHistoryDelegate = historyDelegate;

        boolean isForward = type == Type.TABLET_FORWARD;

        mHistory =
                mNavigationController.getDirectedNavigationHistory(
                        isForward, MAXIMUM_HISTORY_ITEMS);
        if (!shouldUseIncognitoResources()) {
            UrlConstantResolver urlConstantResolver =
                    UrlConstantResolverFactory.getForProfile(profile);
            mHistory.addEntry(
                    new NavigationEntry(
                            FULL_HISTORY_ENTRY_INDEX,
                            new GURL(urlConstantResolver.getHistoryPageUrl()),
                            GURL.emptyGURL(),
                            GURL.emptyGURL(),
                            resources.getString(R.string.show_full_history),
                            null,
                            0,
                            0,
                            /* isInitialEntry= */ false));
        }

        mAdapter = new NavigationAdapter();

        mPopup = new ListPopupWindow(context, null, 0, R.style.NavigationPopupDialog);
        mPopup.setOnDismissListener(this::onDismiss);
        mPopup.setBackgroundDrawable(
                AppCompatResources.getDrawable(context, R.drawable.menu_bg_tinted));
        mPopup.setModal(true);
        mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        mPopup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopup.setOnItemClickListener(this);
        mPopup.setAdapter(mAdapter);

        if (ToolbarFeatures.isNavigationListMenuEnabled()) {
            mFaviconSize = AttrUtils.getDimensionPixelSize(mContext, R.attr.listItemIconSize);
        } else {
            mFaviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        }
    }

    ListPopupWindow getPopupForTesting() {
        return mPopup;
    }

    ModelList getListItemsForTesting() {
        return mListItems;
    }

    void handleItemClickForTesting(int index, int position) {
        handleItemClick(index, position);
        if (mListMenuHost != null) mListMenuHost.dismiss();
    }

    private String buildComputedAction(String action) {
        return (mType == Type.TABLET_FORWARD ? "ForwardMenu_" : "BackMenu_") + action;
    }

    /** Shows the popup attached to the specified anchor view. */
    public void show(View anchorView) {
        if (!mInitialized) initialize();
        if (ToolbarFeatures.isNavigationListMenuEnabled()) {
            showListMenu(anchorView);
            return;
        }
        showListPopupWindow(anchorView);
    }

    private void showListPopupWindow(View anchorView) {
        if (!mPopup.isShowing()) RecordUserAction.record(buildComputedAction("Popup"));
        mPopup.setAnchorView(anchorView);
        Resources resources = mContext.getResources();
        int contentWidth = UiUtils.computeListAdapterContentDimensions(mAdapter, null)[0];
        int minWidth = resources.getDimensionPixelSize(R.dimen.navigation_popup_tablet_min_width);
        int maxWidth =
                // Take the smaller of...
                Math.min(
                        // ... a fixed upper bound, and...
                        resources.getDimensionPixelSize(R.dimen.navigation_popup_tablet_max_width),
                        // ... the width of the screen minus a margin.
                        resources.getDisplayMetrics().widthPixels
                                - resources.getDimensionPixelSize(
                                        R.dimen.navigation_popup_tablet_width_margin));
        mPopup.setWidth(MathUtils.clamp(contentWidth, minWidth, maxWidth));
        mPopup.show();

        // Set clipToOutline to true to contain the mouse hover effect inside the
        // popup's outline. Also set the background of the list view to popup_bg_shape
        // to make its shape the same as the popup.
        assumeNonNull(mPopup.getListView());
        TypedValue typedValue = new TypedValue();
        @DrawableRes
        int bgResId =
                mContext.getTheme().resolveAttribute(R.attr.popupBgShape, typedValue, true)
                        ? typedValue.resourceId
                        : 0;
        assert bgResId != 0;
        mPopup.getListView().setBackgroundResource(bgResId);
        mPopup.getListView().setClipToOutline(true);
    }

    /** Dismisses the popup. */
    public void dismiss() {
        if (mListMenuHost != null) {
            mListMenuHost.dismiss();
            return;
        }
        mPopup.dismiss();
    }

    /**
     * Sets the callback to be notified when the popup has been dismissed.
     * @param onDismiss The callback to be notified.
     */
    public void setOnDismissCallback(Runnable onDismiss) {
        mOnDismissCallback = onDismiss;
    }

    private void showListMenu(View anchorView) {
        if (mListMenuHost != null && mListMenuHost.isMenuShowing()) {
            return;
        }

        RecordUserAction.record(buildComputedAction("Popup"));
        mListItems.clear();

        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            PropertyModel model =
                    new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                            .with(ListMenuItemProperties.TITLE, getTitleForNavEntry(entry))
                            .with(ListMenuItemProperties.ENABLED, true)
                            .with(ListMenuItemProperties.MENU_ITEM_ID, entry.getIndex())
                            .with(ListMenuItemProperties.ORDER, i)
                            .with(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN, true)
                            .with(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END, true)
                            .build();
            updateIconForModel(model, entry.getFavicon(), entry.getIndex());
            if (entry.getIndex() == FULL_HISTORY_ENTRY_INDEX) {
                mListItems.add(BasicListMenu.buildMenuDivider(shouldUseIncognitoResources()));
            }
            mListItems.add(new ListItem(ListItemType.MENU_ITEM, model));
        }
        initListMenuHost(anchorView);
        assumeNonNull(mListMenuHost).showMenu();
    }

    private String getTitleForNavEntry(NavigationEntry entry) {
        String title = entry.getTitle();
        if (!TextUtils.isEmpty(title)) return title;
        String virtualUrl = entry.getVirtualUrl().getSpec();
        return !TextUtils.isEmpty(virtualUrl) ? virtualUrl : entry.getUrl().getSpec();
    }

    private void updateIconForModel(PropertyModel model, @Nullable Bitmap favicon, int index) {
        if (index == FULL_HISTORY_ENTRY_INDEX) {
            model.set(ListMenuItemProperties.START_ICON_ID, R.drawable.ic_history_24dp);
            model.set(
                    ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                    R.color.default_icon_color_accent1_tint_list);
        } else if (favicon != null) {
            model.set(ListMenuItemProperties.START_ICON_BITMAP, favicon);
        }
    }

    private void initListMenuHost(View anchorView) {
        mListMenuHost = new ListMenuHost(anchorView, null);
        mListMenuHost.setMenuMaxWidth(calculateMaxWidthPx(anchorView.getRootView()));
        mListMenuHost.tryToFitLargestItem(true);
        mListMenuHost.setDelegate(createListMenuDelegate(), false);
        mListMenuHost.addPopupListener(
                new ListMenuHost.PopupMenuShownListener() {
                    @Override
                    public void onPopupMenuShown() {}

                    @Override
                    public void onPopupMenuDismissed() {
                        onDismiss();
                    }
                });
    }

    private int calculateMaxWidthPx(View rootView) {
        Resources res = mContext.getResources();
        int viewportWidthPx = rootView.getWidth();
        int maxWidthPx = res.getDimensionPixelSize(R.dimen.navigation_popup_tablet_max_width);
        int gutterPx = res.getDimensionPixelSize(R.dimen.navigation_popup_tablet_width_margin);
        return Math.min(viewportWidthPx - 2 * gutterPx, maxWidthPx);
    }

    private ListMenuDelegate createListMenuDelegate() {
        return new ListMenuDelegate() {
            @Override
            public ListMenu getListMenu() {
                return BrowserUiListMenuUtils.getBasicListMenu(
                        mContext,
                        mListItems,
                        (item, view) -> {
                            int index = item.get(ListMenuItemProperties.MENU_ITEM_ID);
                            int position = item.get(ListMenuItemProperties.ORDER);
                            handleItemClick(index, position);
                            dismiss();
                        });
            }

            @Override
            public RectProvider getRectProvider(View view) {
                return new ViewRectProvider(view);
            }
        };
    }

    private void onDismiss() {
        if (mInitialized) mFaviconHelper.destroy();
        mInitialized = false;
        if (mDefaultFaviconHelper != null) mDefaultFaviconHelper.clearCache();
        if (mOnDismissCallback != null) mOnDismissCallback.run();
    }

    private void initialize() {
        ThreadUtils.assertOnUiThread();
        mInitialized = true;
        mFaviconHelper = new FaviconHelper();

        Set<GURL> requestedUrls = new HashSet<>();
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (entry.getFavicon() != null) continue;
            final GURL pageUrl = entry.getUrl();
            if (!requestedUrls.contains(pageUrl)) {
                FaviconImageCallback imageCallback =
                        (bitmap, iconUrl) ->
                                NavigationPopup.this.onFaviconAvailable(pageUrl, bitmap);
                mFaviconHelper.getLocalFaviconImageForURL(
                        mProfile, pageUrl, mFaviconSize, /* fallbackToHost= */ true, imageCallback);
                requestedUrls.add(pageUrl);
            }
        }
    }

    /**
     * Called when favicon data requested by {@link #initialize()} is retrieved.
     * @param pageUrl the page for which the favicon was retrieved.
     * @param favicon the favicon data.
     */
    private void onFaviconAvailable(GURL pageUrl, Bitmap favicon) {
        if (favicon == null) {
            if (mDefaultFaviconHelper == null) mDefaultFaviconHelper = new DefaultFaviconHelper();
            favicon =
                    mDefaultFaviconHelper.getDefaultFaviconBitmap(
                            mContext,
                            pageUrl,
                            /* useDarkIcon= */ true,
                            /* useIncognitoNtpIcon= */ false);
        }
        if (UrlUtilities.isNtpUrl(pageUrl) && shouldUseIncognitoResources()) {
            favicon =
                    mDefaultFaviconHelper.getThemifiedBitmap(
                            mContext, R.drawable.incognito_small, true);
        }
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (pageUrl.equals(entry.getUrl())) {
                entry.updateFavicon(favicon);

                if (!mListItems.isEmpty()
                        && i < mListItems.size()
                        && entry.getIndex() != FULL_HISTORY_ENTRY_INDEX) {
                    PropertyModel model = mListItems.get(i).model;
                    updateIconForModel(model, favicon, entry.getIndex());
                }
            }
        }

        if (mListItems.isEmpty()) {
            mAdapter.notifyDataSetChanged();
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        NavigationEntry entry = (NavigationEntry) parent.getItemAtPosition(position);
        handleItemClick(entry.getIndex(), position);
        mPopup.dismiss();
    }

    private void handleItemClick(int index, int position) {
        if (index == FULL_HISTORY_ENTRY_INDEX) {
            RecordUserAction.record(buildComputedAction("ShowFullHistory"));
            Tab currentTab = mCurrentTabSupplier.get();
            assert currentTab != null;
            mHistoryDelegate.show(currentTab);
            return;
        }
        // 1-based index to keep in line with Desktop implementation.
        RecordUserAction.record(buildComputedAction("HistoryClick" + (position + 1)));
        mNavigationController.goToNavigationIndex(index);
    }

    private class NavigationAdapter extends BaseAdapter {

        @Override
        public int getCount() {
            return mHistory.getEntryCount();
        }

        @Override
        public Object getItem(int position) {
            return mHistory.getEntryAtIndex(position);
        }

        @Override
        public long getItemId(int position) {
            return ((NavigationEntry) getItem(position)).getIndex();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            EntryViewHolder viewHolder;
            if (convertView == null) {
                LayoutInflater inflater = LayoutInflater.from(mContext);
                convertView = inflater.inflate(R.layout.navigation_popup_item, parent, false);
                viewHolder =
                        new EntryViewHolder(
                                /* imageView= */ convertView.findViewById(R.id.favicon_img),
                                /* textView= */ convertView.findViewById(R.id.entry_title));
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (EntryViewHolder) convertView.getTag();
            }

            NavigationEntry entry = (NavigationEntry) getItem(position);
            setViewText(entry, viewHolder.mTextView);
            viewHolder.mImageView.setImageBitmap(entry.getFavicon());

            if (entry.getIndex() == FULL_HISTORY_ENTRY_INDEX) {
                ImageViewCompat.setImageTintList(
                        viewHolder.mImageView,
                        mContext.getColorStateList(R.color.default_icon_color_accent1_tint_list));
            } else {
                ImageViewCompat.setImageTintList(viewHolder.mImageView, null);
            }

            return convertView;
        }

        private void setViewText(NavigationEntry entry, TextView view) {
            view.setText(getTitleForNavEntry(entry));
        }
    }

    private boolean shouldUseIncognitoResources() {
        return mProfile.isOffTheRecord();
    }

    private static class EntryViewHolder {
        private EntryViewHolder(ImageView imageView, TextView textView) {
            mImageView = imageView;
            mTextView = textView;
        }

        final ImageView mImageView;
        final TextView mTextView;
    }
}
