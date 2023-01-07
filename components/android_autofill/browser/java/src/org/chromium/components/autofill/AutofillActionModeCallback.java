// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.os.Build;
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

    // using getIdentifier to work around not-exposed framework resource ID
    @SuppressWarnings("DiscouragedApi")
    public AutofillActionModeCallback(Context context, AutofillProvider autofillProvider) {
        mContext = context;
        mAutofillProvider = autofillProvider;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            mAutofillMenuItemTitle = android.R.string.autofill;
        } else {
            // The string resource was not made public until O MR1, so on O we look it up by name.
            mAutofillMenuItemTitle =
                    mContext.getResources().getIdentifier("autofill", "string", "android");
        }
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        return mAutofillMenuItemTitle != 0;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        if (mAutofillMenuItemTitle != 0 && mAutofillProvider.shouldQueryAutofillSuggestion()) {
            MenuItem item = menu.add(Menu.NONE, android.R.id.autofill, Menu.CATEGORY_SECONDARY,
                    mAutofillMenuItemTitle);
            item.setShowAsActionFlags(
                    MenuItem.SHOW_AS_ACTION_NEVER | MenuItem.SHOW_AS_ACTION_WITH_TEXT);
        }
        return true;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (item.getItemId() == android.R.id.autofill) {
            mAutofillProvider.queryAutofillSuggestion();
            mode.finish();
            return true;
        }
        return false;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {}
}
