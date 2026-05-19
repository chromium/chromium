// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.textfield.TextInputEditText;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;

import java.util.Objects;

/** Common util methods for multi-instance UI. */
@NullMarked
public class UiUtils {
    @VisibleForTesting
    static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.

    @IntDef({
        NameWindowDialogSource.WINDOW_MANAGER,
        NameWindowDialogSource.TAB_STRIP,
        NameWindowDialogSource.APP_MENU,
    })
    public @interface NameWindowDialogSource {
        int WINDOW_MANAGER = 0;
        int TAB_STRIP = 1;
        int APP_MENU = 2;
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
        MultiWindowMetricsUtils.recordNameWindowUserAction(source);

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
                        MultiWindowMetricsUtils.recordSaveWindowNameUserAction(source);
                        if (!newTitle.equals(currentTitle)) {
                            MultiWindowMetricsUtils.recordChangeWindowNameUserAction(source);
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

    /**
     * Gets the display title for an instance.
     *
     * @param context The {@link Context} to retrieve string resources.
     * @param item {@link InstanceInfo} to get a title string for.
     * @return Text string for a given instance.
     */
    public static String getItemTitle(Context context, InstanceInfo item) {
        // We do not restore incognito tabs in an instance if its task got killed. Treat it as if it
        // did not have any incognito tabs.
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        int totalTabCount = totalTabCount(item);
        String title;
        Resources res = context.getResources();
        if (!TextUtils.isEmpty(item.customTitle)) {
            title = item.customTitle;
        } else if (totalTabCount == 0 || isInitialNonIncognitoWindow(item, totalTabCount)) {
            title = res.getString(R.string.instance_switcher_entry_empty_window);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            // Show 'incognito tab' only when we have any restorable incognito tabs.
            title =
                    IncognitoUtils.shouldOpenIncognitoAsWindow()
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
    /* package */ static String getItemDesc(Context context, InstanceInfo item) {
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        int totalTabCount = totalTabCount(item);
        String desc;
        Resources res = context.getResources();
        if (totalTabCount == 0) { // <ex>No tabs</ex>
            desc = res.getString(R.string.instance_switcher_tab_count_zero);
        } else if (item.isIncognitoSelected && incognitoTabCount > 0) {
            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) { // <ex>2 tabs</ex>
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
    /* package */ static String getConfirmationMessage(Context context, InstanceInfo item) {
        String title = item.title;
        int totalTabCount = totalTabCount(item);
        int incognitoTabCount = recoverableIncognitoTabCount(item);
        Resources res = context.getResources();
        String msg;
        if (item.isIncognitoSelected && incognitoTabCount > 0) {
            if (item.tabCount == 0) { // 2 incognito tabs will be closed
                msg =
                        res.getQuantityString(
                                R.plurals.instance_switcher_close_confirm_deleted_incognito,
                                incognitoTabCount,
                                incognitoTabCount);
            } else { // 1 incognito and 3 other tabs will be closed
                msg =
                        res.getQuantityString(
                                R.plurals
                                        .instance_switcher_close_confirm_deleted_incognito_mixed_v2,
                                item.tabCount,
                                incognitoTabCount,
                                item.tabCount,
                                incognitoTabCount);
            }
        } else if (totalTabCount == 0) { // The window will be closed
            msg = res.getString(R.string.instance_switcher_close_confirm_deleted_tabs_zero);
        } else if (totalTabCount == 1) { // YouTube will be closed.
            msg =
                    res.getString(
                            R.string.instance_switcher_close_confirm_deleted_tabs_one_v2, title);
        } else { // YouTube and 3 other tabs will be closed
            msg =
                    res.getQuantityString(
                            R.plurals.instance_switcher_close_confirm_deleted_tabs_many_v2,
                            totalTabCount - 1,
                            title,
                            totalTabCount - 1,
                            title);
        }
        return msg;
    }

    /* package */ static int recoverableIncognitoTabCount(InstanceInfo item) {
        return item.taskId == INVALID_TASK_ID ? 0 : item.incognitoTabCount;
    }

    /* package */ static int totalTabCount(InstanceInfo item) {
        return item.tabCount + recoverableIncognitoTabCount(item);
    }

    /**
     * @return Whether a new Chrome instance has not yet started loading a URL on its tab.
     */
    /* package */ static boolean isInitialNonIncognitoWindow(InstanceInfo item, int totalTabCount) {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            return !item.isIncognitoSelected
                    && totalTabCount == 1
                    && TextUtils.isEmpty(item.url)
                    && TextUtils.isEmpty(item.title);
        }
        return totalTabCount == 1 && TextUtils.isEmpty(item.url) && TextUtils.isEmpty(item.title);
    }

    /* package */ static void closeOpenDialogs() {
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
