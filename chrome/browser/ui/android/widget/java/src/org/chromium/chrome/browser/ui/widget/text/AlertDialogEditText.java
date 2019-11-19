// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.text;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.v7.widget.AppCompatEditText;
import android.util.AttributeSet;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ui.widget.R;

/**
 * EditText to use in AlertDialog needed due to b/20882793 and b/122113958. This class should be
 * removed when we roll to AppCompat with a fix for both issues.
 *
 * Note that for password fields the hint text is expected to be set in XML so that it is available
 * during inflation. If the hint text or content description is changed programatically, consider
 * calling {@link ApiCompatibilityUtils#setPasswordEditTextContentDescription(EditText)} after
 * the change.
 */
public class AlertDialogEditText extends AppCompatEditText {
    public AlertDialogEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        ApiCompatibilityUtils.setPasswordEditTextContentDescription(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) return;

        setCustomSelectionActionModeCallback(new ActionMode.Callback() {
            @Override
            public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
                return true;
            }

            @Override
            public void onDestroyActionMode(ActionMode mode) {}

            @Override
            public boolean onCreateActionMode(ActionMode mode, Menu menu) {
                for (int i = 0; i < menu.size(); i++) {
                    MenuItem item = menu.getItem(i);
                    Drawable icon = item.getIcon();
                    if (icon == null) break;
                    icon.setColorFilter(ApiCompatibilityUtils.getColor(
                                                getResources(), R.color.default_icon_color),
                            PorterDuff.Mode.SRC_IN);
                    item.setIcon(icon);
                }
                return true;
            }

            @Override
            public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
                return false;
            }
        });
    }
}
