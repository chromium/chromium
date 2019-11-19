// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync.test.util;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.SyncStatusObserver;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.sync.SyncContentResolverDelegate;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Mock implementation of the {@link SyncContentResolverDelegate}.
 *
 * This implementation only supports status change listeners for the type
 * SYNC_OBSERVER_TYPE_SETTINGS.
 */
public class MockSyncContentResolverDelegate implements SyncContentResolverDelegate {
    private final Set<String> mSyncAutomaticallySet;
    private final Map<String, Boolean> mIsSyncableMap;
    private final Object mSyncableMapLock = new Object();

    private final Set<AsyncSyncStatusObserver> mObservers;

    private boolean mMasterSyncAutomatically;
    private boolean mDisableObserverNotifications;

    private Semaphore mPendingObserverCount;

    public MockSyncContentResolverDelegate() {
        mSyncAutomaticallySet = new HashSet<String>();
        mIsSyncableMap = new HashMap<String, Boolean>();
        mObservers = new HashSet<AsyncSyncStatusObserver>();
    }

    @Override
    public Object addStatusChangeListener(int mask, SyncStatusObserver callback) {
        if (mask != ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS) {
            throw new IllegalArgumentException("This implementation only supports "
                    + "ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS as the mask");
        }
        AsyncSyncStatusObserver asyncSyncStatusObserver = new AsyncSyncStatusObserver(callback);
        synchronized (mObservers) {
            mObservers.add(asyncSyncStatusObserver);
        }
        return asyncSyncStatusObserver;
    }

    @Override
    public void removeStatusChangeListener(Object handle) {
        synchronized (mObservers) {
            mObservers.remove(handle);
        }
    }

    @Override
    @VisibleForTesting
    public void setMasterSyncAutomatically(boolean sync) {
        if (mMasterSyncAutomatically == sync) return;

        mMasterSyncAutomatically = sync;
        notifyObservers();
    }

    @Override
    public boolean getMasterSyncAutomatically() {
        return mMasterSyncAutomatically;
    }

    @Override
    public boolean getSyncAutomatically(Account account, String authority) {
        String key = createKey(account, authority);
        synchronized (mSyncableMapLock) {
            return mSyncAutomaticallySet.contains(key);
        }
    }

    @Override
    public void setSyncAutomatically(Account account, String authority, boolean sync) {
        String key = createKey(account, authority);
        synchronized (mSyncableMapLock) {
            if (!mIsSyncableMap.containsKey(key) || !mIsSyncableMap.get(key)) {
                throw new IllegalArgumentException("Account " + account
                        + " is not syncable for authority " + authority
                        + ". Can not set sync state to " + sync);
            }
            if (sync) {
                mSyncAutomaticallySet.add(key);
            } else if (mSyncAutomaticallySet.contains(key)) {
                mSyncAutomaticallySet.remove(key);
            }
        }
        notifyObservers();
    }

    @Override
    public void setIsSyncable(Account account, String authority, int syncable) {
        String key = createKey(account, authority);

        synchronized (mSyncableMapLock) {
            switch (syncable) {
                case 0:
                    mIsSyncableMap.put(key, false);
                    break;
                case 1:
                    mIsSyncableMap.put(key, true);
                    break;
                case -1:
                    if (mIsSyncableMap.containsKey(key)) {
                        mIsSyncableMap.remove(key);
                    }
                    break;
                default:
                    throw new IllegalArgumentException(
                            "Unable to understand syncable argument: " + syncable);
            }
        }
        notifyObservers();
    }

    @Override
    public int getIsSyncable(Account account, String authority) {
        String key = createKey(account, authority);
        synchronized (mSyncableMapLock) {
            if (mIsSyncableMap.containsKey(key)) {
                return mIsSyncableMap.get(key) ? 1 : 0;
            } else {
                return -1;
            }
        }
    }

    @Override
    public void removePeriodicSync(Account account, String authority, Bundle extras) {}

    private static String createKey(Account account, String authority) {
        return account.name + "@@@" + account.type + "@@@" + authority;
    }

    private void notifyObservers() {
        if (mDisableObserverNotifications) return;
        synchronized (mObservers) {
            mPendingObserverCount = new Semaphore(1 - mObservers.size());
            for (AsyncSyncStatusObserver observer : mObservers) {
                observer.notifyObserverAsync(mPendingObserverCount);
            }
        }
    }

    /**
     * Blocks until the last notification has been issued to all registered observers.
     * Note that if an observer is removed while a notification is being handled this can
     * fail to return correctly.
     *
     * @throws InterruptedException
     */
    @VisibleForTesting
    public void waitForLastNotificationCompleted() throws InterruptedException {
        Assert.assertTrue("Timed out waiting for notifications to complete.",
                mPendingObserverCount.tryAcquire(5, TimeUnit.SECONDS));
    }

    public void disableObserverNotifications() {
        mDisableObserverNotifications = true;
    }

    /**
      * Simulate an account rename, which copies settings to the new account.
      */
    public void renameAccounts(Account oldAccount, Account newAccount, String authority) {
        int oldIsSyncable = getIsSyncable(oldAccount, authority);
        setIsSyncable(newAccount, authority, oldIsSyncable);
        if (oldIsSyncable == 1) {
            setSyncAutomatically(
                    newAccount, authority, getSyncAutomatically(oldAccount, authority));
        }
    }

    private static class AsyncSyncStatusObserver {
        private final SyncStatusObserver mSyncStatusObserver;

        private AsyncSyncStatusObserver(SyncStatusObserver syncStatusObserver) {
            mSyncStatusObserver = syncStatusObserver;
        }

        private void notifyObserverAsync(final Semaphore pendingObserverCount) {
            if (ThreadUtils.runningOnUiThread()) {
                new AsyncTask<Void>() {
                    @Override
                    protected Void doInBackground() {
                        mSyncStatusObserver.onStatusChanged(
                                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS);
                        return null;
                    }

                    @Override
                    protected void onPostExecute(Void result) {
                        pendingObserverCount.release();
                    }
                }
                        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            } else {
                mSyncStatusObserver.onStatusChanged(ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS);
                pendingObserverCount.release();
            }
        }
    }
}
