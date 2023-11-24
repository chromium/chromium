// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Common util methods for multi-instance UI. */
class UiUtils {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.

    private final Context mContext;
    private final int mMinIconSizeDp;
    private final int mDisplayedIconSize;
    private final Drawable mIncognitoFavicon;
    private final Drawable mGlobeFavicon;
    private LargeIconBridge mLargeIconBridge;
    private final RoundedIconGenerator mIconGenerator;

    UiUtils(Context context, LargeIconBridge iconBridge) {
        mContext = context;
        mLargeIconBridge = iconBridge;
        Resources res = context.getResources();
        mMinIconSizeDp = (int) res.getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = res.getDimensionPixelSize(R.dimen.default_favicon_size);
        mIncognitoFavicon = getTintedIcon(R.drawable.incognito_simple);
        mGlobeFavicon = getTintedIcon(R.drawable.ic_globe_24dp);
        mIconGenerator = FaviconUtils.createRoundedRectangleIconGenerator(context);
    }

    Drawable getTintedIcon(@DrawableRes int drawableId) {
        return org.chromium.ui.UiUtils.getTintedDrawable(
                mContext, drawableId, R.color.default_icon_color_tint_list);
    }

    /**
     * @param item {@link InstanceInfo} to get a title string for.
     * @return Text string for a given instance.
     */
    String getItemTitle(InstanceInfo item) {
        // We do not restore incognito tabs in an instance if its task got killed. Treat it as if it
        // did not have any incognito tabs.
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        int totalTabCount = totalTabCount(item);
        String title;
        Resources res = mContext.getResources();
        if (totalTabCount == 0 || isBeforeFirstTabLoad(item, totalTabCount)) {
            title = res.getString(R.string.instance_switcher_entry_empty_window);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            // Show 'incognito tab' only when we have any restorable incognito tabs.
            title = res.getString(R.string.notification_incognito_tab);
        } else {
            title = item.title;
        }
        return title;
    }

    /**
     * @param item {@link InstanceInfo} to get a description string for.
     * @return Text string containing additional description for a given instance.
     */
    String getItemDesc(InstanceInfo item) {
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        int totalTabCount = totalTabCount(item);
        String desc;
        Resources res = mContext.getResources();
        if (item.type == InstanceInfo.Type.CURRENT) {
            desc = res.getString(R.string.instance_switcher_current_window);
        } else if (item.type == InstanceInfo.Type.ADJACENT) {
            desc = res.getString(R.string.instance_switcher_adjacent_window);
        } else if (totalTabCount == 0) { // <ex>No tabs</ex>
            desc = res.getString(R.string.instance_switcher_tab_count_zero);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            if (item.tabCount == 0) { // <ex>2 incognito tabs</ex>
                desc =
                        res.getQuantityString(
                                R.plurals.instance_switcher_desc_incognito,
                                incognitoTabCount,
                                incognitoTabCount);
            } else { // <ex>5 tabs, 2 incognito</ex>
                desc =
                        res.getQuantityString(
                                R.plurals.instance_switcher_desc_mixed,
                                totalTabCount,
                                incognitoTabCount,
                                totalTabCount,
                                incognitoTabCount);
            }
        } else if (incognitoTabCount == 0) { // <ex>3 tabs</ex>
            desc =
                    res.getQuantityString(
                            R.plurals.instance_switcher_tab_count_nonzero,
                            item.tabCount,
                            item.tabCount);
        } else { // <ex>5 tabs, 2 incognito</ex>
            desc =
                    res.getQuantityString(
                            R.plurals.instance_switcher_desc_mixed,
                            totalTabCount,
                            incognitoTabCount,
                            totalTabCount,
                            incognitoTabCount);
        }
        return desc;
    }

    /**
     * @param item {@link InstanceInfo} to get a confirmation message for.
     * @return Confirmation message for closing a given instance.
     */
    String getConfirmationMessage(InstanceInfo item) {
        String title = item.title;
        int totalTabCount = totalTabCount(item);
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        Resources res = mContext.getResources();
        String msg;
        if (item.isIncognitoSelected && incognitoTabCount > 0) {
            if (item.tabCount == 0) { // 2 incognito tabs will be closed
                msg =
                        res.getQuantityString(
                                R.plurals.instance_switcher_close_confirm_deleted_incognito,
                                incognitoTabCount,
                                incognitoTabCount);
            } else { // 1 incognito and 3 more tabs will be closed
                msg =
                        res.getQuantityString(
                                R.plurals.instance_switcher_close_confirm_deleted_incognito_mixed,
                                item.tabCount,
                                incognitoTabCount,
                                item.tabCount,
                                incognitoTabCount);
            }
        } else if (totalTabCount == 0) { // The window will be closed
            msg = res.getString(R.string.instance_switcher_close_confirm_deleted_tabs_zero);
        } else if (totalTabCount == 1) { // The tab YouTube will be closed
            msg = res.getString(R.string.instance_switcher_close_confirm_deleted_tabs_one, title);
        } else { // YouTube and 3 more tabs will be closed
            msg =
                    res.getQuantityString(
                            R.plurals.instance_switcher_close_confirm_deleted_tabs_many,
                            totalTabCount - 1,
                            title,
                            totalTabCount - 1,
                            title);
        }
        return msg;
    }

    /**
     * Set the favicon for the given instance.
     * @param model {@link PropertyModel} that represents the instance entry.
     * @param faviconKey Property key for favicon item in the model.
     * @param item {@link InstanceInfo} object for the given instance.
     */
    void setFavicon(
            PropertyModel model,
            PropertyModel.WritableObjectPropertyKey<Drawable> faviconKey,
            InstanceInfo item) {
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        int totalTabCount = totalTabCount(item);
        if (totalTabCount == 0 || isBeforeFirstTabLoad(item, totalTabCount)) {
            model.set(faviconKey, mGlobeFavicon);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            model.set(faviconKey, mIncognitoFavicon);
        } else {
            GURL url = new GURL(item.url);
            mLargeIconBridge.getLargeIconForUrl(
                    url,
                    mMinIconSizeDp,
                    (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                        model.set(faviconKey, createIconDrawable(item.url, icon, fallbackColor));
                    });
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static int recoverableIncognitoTabCount(InstanceInfo item) {
        return item.taskId == INVALID_TASK_ID ? 0 : item.incognitoTabCount;
    }

    static int totalTabCount(InstanceInfo item) {
        return item.tabCount + recoverableIncognitoTabCount(item);
    }

    private Drawable createIconDrawable(String url, Bitmap icon, int fallbackColor) {
        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(url);
        } else {
            icon = Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, true);
        }
        return new BitmapDrawable(mContext.getResources(), icon);
    }

    /**
     * @return Whether a new Chrome instance has not yet started loading a URL on its tab.
     */
    private boolean isBeforeFirstTabLoad(InstanceInfo item, int totalTabCount) {
        return totalTabCount == 1 && TextUtils.isEmpty(item.url) && TextUtils.isEmpty(item.title);
    }

    static void closeOpenDialogs() {
        if (InstanceSwitcherCoordinator.sPrevInstance != null) {
            InstanceSwitcherCoordinator.sPrevInstance.dismissDialog(
                    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        }
        if (TargetSelectorCoordinator.sPrevInstance != null) {
            TargetSelectorCoordinator.sPrevInstance.dismissDialog(
                    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        }
    }
}
