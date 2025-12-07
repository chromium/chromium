// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.pm.ResolveInfo;
import android.view.View;

import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;

import java.util.List;

/**
 * Base autofill delegate customizing text selection menu items in {@link SelectionPopupController}.
 * It ensures that a client using Android Autofill has access to the fallback entry in the
 * long-press selection menu.
 */
@NullMarked
public class AutofillSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
    private @Nullable AutofillSelectionMenuItemHelper mAutofillSelectionMenuItemHelper;

    public AutofillSelectionActionMenuDelegate() {}

    @Override
    public List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        if (selectedText.isEmpty() && mAutofillSelectionMenuItemHelper != null) {
            return mAutofillSelectionMenuItemHelper.getAdditionalItems();
        }
        return List.of();
    }

    @Override
    public List<ResolveInfo> filterTextProcessingActivities(
            @MenuType int menuType, List<ResolveInfo> activities) {
        return activities;
    }

    @Override
    public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
        return true;
    }

    public void setAutofillSelectionMenuItemHelper(AutofillSelectionMenuItemHelper provider) {
        mAutofillSelectionMenuItemHelper = provider;
    }

    @Override
    public boolean handleMenuItemClick(
            SelectionMenuItem item, WebContents webContents, @Nullable View containerView) {
        return mAutofillSelectionMenuItemHelper != null
                && mAutofillSelectionMenuItemHelper.handleMenuItemClick(item);
    }
}
