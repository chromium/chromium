// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;

/** A fake implementation of AconfigFlaggedApiDelegate for testing the system contacts picker. */
@NullMarked
@ServiceImpl(AconfigFlaggedApiDelegate.class)
public class FakeAconfigFlaggedApiDelegate implements AconfigFlaggedApiDelegate {
    private boolean mSystemContactsPickerEnabled;

    public static final String ACTION_PICK_CONTACTS = "org.chromium.test.ACTION_PICK_CONTACTS";
    private static final String EXTRA_FIELDS = "org.chromium.test.EXTRA_FIELDS";

    public void setSystemContactsPickerEnabled(boolean enabled) {
        mSystemContactsPickerEnabled = enabled;
    }

    @Override
    public boolean isSystemContactsPickerEnabled() {
        return mSystemContactsPickerEnabled;
    }

    @Override
    public @Nullable String getSystemContactsPickerAction() {
        return mSystemContactsPickerEnabled ? ACTION_PICK_CONTACTS : null;
    }

    @Override
    public @Nullable String getSystemContactsPickerExtraRequestedDataFields() {
        return mSystemContactsPickerEnabled ? EXTRA_FIELDS : null;
    }

    @Override
    public @Nullable String getSystemContactsPickerAuthority() {
        return mSystemContactsPickerEnabled ? "org.chromium.test.contacts" : null;
    }
}
