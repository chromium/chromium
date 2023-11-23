// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;

/**
 * Dialog subclass that ensures that dismiss() is called, even if the dialog is implicitly dismissed
 * due to its activity being destroyed. This is important if the dialog's dismiss() method releases
 * references to memory or performs other crucial cleanup. See http://crbug.com/507748.
 * DialogFragments ensure that dismiss() is called as well.
 */
public class AlwaysDismissedDialog extends ChromeDialog
        implements ApplicationStatus.ActivityStateListener {
    public AlwaysDismissedDialog(Activity ownerActivity, int theme) {
        super(ownerActivity, theme);
        ApplicationStatus.registerStateListenerForActivity(this, ownerActivity);

        setOwnerActivity(ownerActivity);
    }

    @Override
    public void dismiss() {
        super.dismiss();
        ApplicationStatus.unregisterActivityStateListener(this);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.DESTROYED) dismiss();
    }
}
