// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.pm.ResolveInfo;

import androidx.annotation.NonNull;

import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;

import java.util.ArrayList;
import java.util.List;

/**
 * Base autofill delegate customizing text selection menu items in {@link SelectionPopupController}.
 * It ensures that a client using Android Autofill has access to the fallback entry in the
 * long-press selection menu.
 */
public class AutofillSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
    private AutofillSelectionMenuItemHelper mAutofillSelectionMenuItemHelper;

    public AutofillSelectionActionMenuDelegate() {}

    @Override
    public void modifyDefaultMenuItems(
            List<SelectionMenuItem.Builder> menuItemBuilders,
            boolean isSelectionPassword,
            @NonNull String selectedText) {}

    @Override
    public List<ResolveInfo> filterTextProcessingActivities(List<ResolveInfo> activities) {
        return activities;
    }

    @NonNull
    @Override
    public List<SelectionMenuItem> getAdditionalNonSelectionItems() {
        if (mAutofillSelectionMenuItemHelper != null) {
            return mAutofillSelectionMenuItemHelper.getAdditionalItems();
        }
        return new ArrayList<>();
    }

    @NonNull
    @Override
    public List<SelectionMenuItem> getAdditionalTextProcessingItems() {
        return new ArrayList<>();
    }

    public void setAutofillSelectionMenuItemHelper(AutofillSelectionMenuItemHelper provider) {
        mAutofillSelectionMenuItemHelper = provider;
    }
}
