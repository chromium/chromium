// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.invalidation;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import org.chromium.base.test.util.AdvancedMockContext;

import java.util.ArrayList;
import java.util.List;

/** Mock context that saves all intents given to {@code startService}. */
public class IntentSavingContext extends AdvancedMockContext {
    private final List<Intent> mStartedIntents = new ArrayList<Intent>();

    public IntentSavingContext(Context targetContext) {
        super(targetContext);
    }

    @Override
    public ComponentName startService(Intent intent) {
        mStartedIntents.add(intent);
        return new ComponentName(this, getClass());
    }

    public int getNumStartedIntents() {
        return mStartedIntents.size();
    }

    public Intent getStartedIntent(int idx) {
        return mStartedIntents.get(idx);
    }

    @Override
    public PackageManager getPackageManager() {
        return getBaseContext().getPackageManager();
    }
}
