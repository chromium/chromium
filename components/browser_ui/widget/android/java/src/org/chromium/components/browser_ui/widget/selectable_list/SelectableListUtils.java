// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.R;

/** A set of utils to be used alongside SelectableListLayout. */
public class SelectableListUtils {
    @IntDef({ContentDescriptionSource.MENU_BUTTON, ContentDescriptionSource.REMOVE_BUTTON})
    public @interface ContentDescriptionSource {
        int MENU_BUTTON = 0;
        int REMOVE_BUTTON = 1;
    }

    /**
     * Text that represents the item this menu button is related to.  This will affect the content
     * description of the view {@see #setContentDescription(CharSequence)}.
     *
     * @param context The current Android context.
     * @param view The view to set the accessibility description on.
     * @param accessibilityContext The string representation of the list item this button
     *         represents.
     * @param source The description source which indicates what kind of accessibility string to
     *         add.
     */
    public static void setContentDescriptionContext(
            @NonNull Context context,
            @NonNull View view,
            @Nullable String accessibilityContext,
            @ContentDescriptionSource int source) {
        boolean missingContext = TextUtils.isEmpty(accessibilityContext);
        int accessibilityResource = -1;
        switch (source) {
            case ContentDescriptionSource.MENU_BUTTON:
                accessibilityResource =
                        missingContext ? R.string.remove : R.string.accessibility_list_menu_button;
                break;
            case ContentDescriptionSource.REMOVE_BUTTON:
                accessibilityResource =
                        missingContext
                                ? R.string.accessibility_toolbar_btn_menu
                                : R.string.accessibility_list_remove_button;
                break;
            default:
                assert false : "Unsupported ContentDescriptionSource detected!";
                return;
        }
        view.setContentDescription(
                missingContext
                        ? context.getResources().getString(accessibilityResource)
                        : context.getResources()
                                .getString(accessibilityResource, accessibilityContext));
    }
}
