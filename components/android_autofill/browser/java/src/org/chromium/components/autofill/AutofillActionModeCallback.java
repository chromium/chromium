// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

/**
 * The class to implement autofill context menu. To match the Android native view behavior, the
 * autofill context menu only appears when there is no text selected.
 */
public class AutofillActionModeCallback implements ActionMode.Callback {
    private final Context mContext;
    private final AutofillProvider mAutofillProvider;
    private final int mAutofillMenuItemTitle;
    private final int mAutofillMenuItem;

    public AutofillActionModeCallback(Context context, AutofillProvider autofillProvider) {
        mContext = context;
        mAutofillProvider = autofillProvider;
        // TODO(michaelbai): Uses the resource directly after sdk roll to Android O MR1.
        // crbug.com/740628
        mAutofillMenuItemTitle =
                mContext.getResources().getIdentifier("autofill", "string", "android");
        mAutofillMenuItem = mContext.getResources().getIdentifier("autofill", "id", "android");
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        return mAutofillMenuItemTitle != 0 && mAutofillMenuItem != 0;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        if (mAutofillMenuItemTitle != 0 && mAutofillProvider.shouldQueryAutofillSuggestion()) {
            MenuItem item = menu.add(
                    Menu.NONE, mAutofillMenuItem, Menu.CATEGORY_SECONDARY, mAutofillMenuItemTitle);
            item.setShowAsActionFlags(
                    MenuItem.SHOW_AS_ACTION_NEVER | MenuItem.SHOW_AS_ACTION_WITH_TEXT);
        }
        return true;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (item.getItemId() == mAutofillMenuItem) {
            mAutofillProvider.queryAutofillSuggestion();
            mode.finish();
            return true;
        }
        return false;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {}
}
