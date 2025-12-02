// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.textfield.TextInputEditText;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Objects;

/** Common util methods for multi-instance UI. */
@NullMarked
public class UiUtils {
    @VisibleForTesting
    static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.

    private final Context mContext;
    private final int mMinIconSizeDp;
    private final int mDisplayedIconSize;
    private final Drawable mIncognitoFavicon;
    private final Drawable mGlobeFavicon;
    private final LargeIconBridge mLargeIconBridge;
    private final RoundedIconGenerator mIconGenerator;

    @IntDef({
        NameWindowDialogSource.WINDOW_MANAGER,
        NameWindowDialogSource.TAB_STRIP,
    })
    public @interface NameWindowDialogSource {
        int WINDOW_MANAGER = 0;
        int TAB_STRIP = 1;
    }

    UiUtils(Context context, LargeIconBridge iconBridge) {
        mContext = context;
        mLargeIconBridge = iconBridge;
        Resources res = context.getResources();
        mMinIconSizeDp = (int) res.getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = res.getDimensionPixelSize(R.dimen.default_favicon_size);
        mIncognitoFavicon =
                isIncognitoAsWindowEnabled()
                        ? mContext.getResources()
                                .getDrawable(R.drawable.ic_incognito_24dp, mContext.getTheme())
                        : getTintedIcon(R.drawable.incognito_simple);
        mGlobeFavicon = getTintedIcon(R.drawable.ic_globe_24dp);
        mIconGenerator = FaviconUtils.createRoundedRectangleIconGenerator(context);
    }

    /**
     * Checks whether the Instance Switcher V2 feature is enabled.
     *
     * @return {@code true} if the Instance Switcher V2 feature is enabled, {@code false} otherwise.
     */
    public static boolean isInstanceSwitcherV2Enabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INSTANCE_SWITCHER_V2);
    }

    /**
     * Checks whether the Robust Window Management feature is enabled.
     *
     * @return {@code true} if the Robust Window Management feature is enabled, {@code false}
     *     otherwise.
     */
    public static boolean isRobustWindowManagementEnabled() {
        return ChromeFeatureList.sRobustWindowManagement.isEnabled();
    }

    /**
     * Checks whether the bulk close feature of Robust Window Management is enabled.
     *
     * @return {@code true} if bulk close is enabled, {@code false} otherwise.
     */
    public static boolean isRobustWindowManagementBulkCloseEnabled() {
        return ChromeFeatureList.sRobustWindowManagementBulkClose.getValue();
    }

    /**
     * Checks whether the Recently Closed Tabs and Windows feature is enabled.
     *
     * @return {@code true} if the Recently Closed Tabs and Windows feature is enabled, {@code
     *     false} otherwise.
     */
    public static boolean isRecentlyClosedTabsAndWindowsEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS);
    }

    /**
     * Checks whether the Android Open Incognito As Window feature is enabled.
     *
     * @deprecated Use {@link
     *     org.chromium.chrome.browser.incognito.IncognitoUtils#isIncognitoAsWindowEnabled()}
     *     instead.
     * @return {@code true} if the Android Open Incognito As Window feature is enabled, {@code
     *     false} otherwise.
     */
    // TODO(crbug.com/448671285): Shift away from isIncognitoAsWindowEnabled() to calling
    // IncognitoUtils function
    @Deprecated
    public static boolean isIncognitoAsWindowEnabled() {
        return ChromeFeatureList.sAndroidOpenIncognitoAsWindow.isEnabled();
    }

    /**
     * Shows a dialog for naming or renaming a window.
     *
     * @param context The {@link Context} to use for creating the dialog.
     * @param currentTitle The current title of the window, which will be pre-filled.
     * @param nameChangedCallback A {@link Callback} that will be invoked when a valid, new title is
     *     set.
     * @param source The {@link NameWindowDialogSource} that tracks the caller of this method.
     */
    public static void showNameWindowDialog(
            Context context,
            String currentTitle,
            Callback<String> nameChangedCallback,
            @NameWindowDialogSource int source) {
        recordNameWindowUserAction(source);

        int style = R.style.Theme_Chromium_Multiwindow_RenameWindowDialog;
        Dialog dialog = new Dialog(context, style);
        dialog.setCanceledOnTouchOutside(true);
        dialog.setContentView(R.layout.rename_window_dialog);

        Resources res = context.getResources();
        ((TextView) dialog.findViewById(R.id.title))
                .setText(res.getString(R.string.instance_switcher_name_window_confirm_header));

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(currentTitle);
        editText.requestFocus();
        Window window = assumeNonNull(dialog.getWindow());
        window.setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN
                        | WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);

        TextView positiveButton = dialog.findViewById(R.id.positive_button);
        positiveButton.setOnClickListener(
                v -> {
                    String newTitle = Objects.toString(editText.getText(), "").trim();
                    if (!TextUtils.isEmpty(newTitle)) {
                        recordSaveWindowNameUserAction(source);
                        if (!newTitle.equals(currentTitle)) {
                            recordChangeWindowNameUserAction(source);
                            nameChangedCallback.onResult(newTitle);
                        }
                    } else {
                        nameChangedCallback.onResult(newTitle);
                    }
                    dialog.dismiss();
                });

        TextView negativeButton = dialog.findViewById(R.id.negative_button);
        negativeButton.setOnClickListener(v -> dialog.dismiss());

        dialog.show();
    }

    private static void recordNameWindowUserAction(@NameWindowDialogSource int source) {
        switch (source) {
            case NameWindowDialogSource.WINDOW_MANAGER:
                RecordUserAction.record("Android.WindowManager.NameWindow");
                break;
            case NameWindowDialogSource.TAB_STRIP:
                RecordUserAction.record("Android.TabStripMenu.NameWindow");
                break;
            default:
                assert false : "Unexpected @NameWindowDialogSource.";
                break;
        }
    }

    private static void recordSaveWindowNameUserAction(@NameWindowDialogSource int source) {
        switch (source) {
            case NameWindowDialogSource.WINDOW_MANAGER:
                RecordUserAction.record("Android.WindowManager.SaveWindowName");
                break;
            case NameWindowDialogSource.TAB_STRIP:
                RecordUserAction.record("Android.TabStripMenu.SaveWindowName");
                break;
            default:
                assert false : "Unexpected @NameWindowDialogSource.";
                break;
        }
    }

    private static void recordChangeWindowNameUserAction(@NameWindowDialogSource int source) {
        switch (source) {
            case NameWindowDialogSource.WINDOW_MANAGER:
                RecordUserAction.record("Android.WindowManager.ChangeWindowName");
                break;
            case NameWindowDialogSource.TAB_STRIP:
                RecordUserAction.record("Android.TabStripMenu.ChangeWindowName");
                break;
            default:
                assert false : "Unexpected @NameWindowDialogSource.";
                break;
        }
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
        // TODO (crbug.com/441312171): Add "Incognito - " prefix for incognito instances.
        if (!TextUtils.isEmpty(item.customTitle)) {
            title = item.customTitle;
        } else if (totalTabCount == 0 || isInitialNonIncognitoWindow(item, totalTabCount)) {
            title = res.getString(R.string.instance_switcher_entry_empty_window);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            // Show 'incognito tab' only when we have any restorable incognito tabs.
            title =
                    isIncognitoAsWindowEnabled()
                            ? res.getString(R.string.instance_switcher_title_incognito_window)
                            : res.getString(R.string.notification_incognito_tab);
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
        if (item.type == InstanceInfo.Type.CURRENT && !isInstanceSwitcherV2Enabled()) {
            desc = res.getString(R.string.instance_switcher_current_window);
        } else if (item.type == InstanceInfo.Type.ADJACENT && !isInstanceSwitcherV2Enabled()) {
            desc = res.getString(R.string.instance_switcher_adjacent_window);
        } else if (totalTabCount == 0) { // <ex>No tabs</ex>
            desc = res.getString(R.string.instance_switcher_tab_count_zero);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            if (isIncognitoAsWindowEnabled()) { // <ex>2 tabs</ex>
                desc =
                        res.getQuantityString(
                                R.plurals.instance_switcher_tab_count_nonzero,
                                item.incognitoTabCount,
                                item.incognitoTabCount);
            } else if (item.tabCount == 0) { // <ex>2 incognito tabs</ex>
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
                                isInstanceSwitcherV2Enabled()
                                        ? R.plurals
                                                .instance_switcher_close_confirm_deleted_incognito_mixed_v2
                                        : R.plurals
                                                .instance_switcher_close_confirm_deleted_incognito_mixed,
                                item.tabCount,
                                incognitoTabCount,
                                item.tabCount,
                                incognitoTabCount);
            }
        } else if (totalTabCount == 0) { // The window will be closed
            msg = res.getString(R.string.instance_switcher_close_confirm_deleted_tabs_zero);
        } else if (totalTabCount == 1) {
            // V1. The tab YouTube will be closed. V2. YouTube will be closed.
            msg =
                    res.getString(
                            isInstanceSwitcherV2Enabled()
                                    ? R.string.instance_switcher_close_confirm_deleted_tabs_one_v2
                                    : R.string.instance_switcher_close_confirm_deleted_tabs_one,
                            title);
        } else { // YouTube and 3 more tabs will be closed
            msg =
                    res.getQuantityString(
                            isInstanceSwitcherV2Enabled()
                                    ? R.plurals.instance_switcher_close_confirm_deleted_tabs_many_v2
                                    : R.plurals.instance_switcher_close_confirm_deleted_tabs_many,
                            totalTabCount - 1,
                            title,
                            totalTabCount - 1,
                            title);
        }
        return msg;
    }

    /**
     * Set the favicon for the given instance.
     *
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
        if (totalTabCount == 0 || isInitialNonIncognitoWindow(item, totalTabCount)) {
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

    @VisibleForTesting
    static int recoverableIncognitoTabCount(InstanceInfo item) {
        return item.taskId == INVALID_TASK_ID ? 0 : item.incognitoTabCount;
    }

    static int totalTabCount(InstanceInfo item) {
        return item.tabCount + recoverableIncognitoTabCount(item);
    }

    private Drawable createIconDrawable(String url, @Nullable Bitmap icon, int fallbackColor) {
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
    private boolean isInitialNonIncognitoWindow(InstanceInfo item, int totalTabCount) {
        if (isIncognitoAsWindowEnabled()) {
            return !item.isIncognitoSelected
                    && totalTabCount == 1
                    && TextUtils.isEmpty(item.url)
                    && TextUtils.isEmpty(item.title);
        }
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
