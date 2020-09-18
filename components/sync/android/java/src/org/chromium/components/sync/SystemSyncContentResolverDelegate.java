// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.SyncStatusObserver;
import android.os.Bundle;

/**
 * A SyncContentResolverDelegate that simply forwards calls to ContentResolver.
 * Note that SyncContentResolverDelegate is not an Android concept. In particular,
 * it's not this class that will notify observers, Android will directly do that.
 */
public class SystemSyncContentResolverDelegate implements SyncContentResolverDelegate {
    @Override
    public Object addStatusChangeListener(int mask, SyncStatusObserver callback) {
        return ContentResolver.addStatusChangeListener(mask, callback);
    }

    @Override
    public void removeStatusChangeListener(Object handle) {
        ContentResolver.removeStatusChangeListener(handle);
    }

    @Override
    public void setMasterSyncAutomatically(boolean sync) {
        ContentResolver.setMasterSyncAutomatically(sync);
    }

    @Override
    public boolean getMasterSyncAutomatically() {
        return ContentResolver.getMasterSyncAutomatically();
    }

    @Override
    public boolean getSyncAutomatically(Account account, String authority) {
        return ContentResolver.getSyncAutomatically(account, authority);
    }

    @Override
    public void setSyncAutomatically(Account account, String authority, boolean sync) {
        ContentResolver.setSyncAutomatically(account, authority, sync);
    }

    @Override
    public void setIsSyncable(Account account, String authority, int syncable) {
        ContentResolver.setIsSyncable(account, authority, syncable);
    }

    @Override
    public int getIsSyncable(Account account, String authority) {
        return ContentResolver.getIsSyncable(account, authority);
    }

    @Override
    public void removePeriodicSync(Account account, String authority, Bundle extras) {
        ContentResolver.removePeriodicSync(account, authority, extras);
    }
}
