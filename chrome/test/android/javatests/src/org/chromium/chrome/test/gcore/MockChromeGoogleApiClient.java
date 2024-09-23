// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.gcore;

import static org.junit.Assert.assertEquals;

import org.chromium.chrome.browser.gcore.ChromeGoogleApiClient;

/** Mock of ChromeGoogleApiClient that tracks which methods are called. */
public class MockChromeGoogleApiClient implements ChromeGoogleApiClient {
    private final Object mLock = new Object();

    private boolean mConnectionResult;
    private boolean mIsGooglePlayServicesAvailable;

    private int mConnectWithTimeoutCount;
    private int mDisconnectCount;
    private int mIsGooglePlayServicesAvailableCount;

    @Override
    public boolean connectWithTimeout(long timeout) {
        synchronized (mLock) {
            mConnectWithTimeoutCount++;
            return mConnectionResult;
        }
    }

    @Override
    public void disconnect() {
        synchronized (mLock) {
            mDisconnectCount++;
        }
    }

    @Override
    public boolean isGooglePlayServicesAvailable() {
        synchronized (mLock) {
            mIsGooglePlayServicesAvailableCount++;
            return mIsGooglePlayServicesAvailable;
        }
    }

    public void setConnectionResult(boolean result) {
        synchronized (mLock) {
            mConnectionResult = result;
        }
    }

    public void setIsGooglePlayServicesAvailable(boolean available) {
        synchronized (mLock) {
            mIsGooglePlayServicesAvailable = available;
        }
    }

    public void assertConnectWithTimeoutCalled(int times) {
        synchronized (mLock) {
            assertEquals(times, mConnectWithTimeoutCount);
            mConnectWithTimeoutCount = 0;
        }
    }

    public void assertDisconnectCalled(int times) {
        synchronized (mLock) {
            assertEquals(times, mDisconnectCount);
            mDisconnectCount = 0;
        }
    }

    public void assertIsGooglePlayServicesAvailableCalled(int times) {
        synchronized (mLock) {
            assertEquals(times, mIsGooglePlayServicesAvailableCount);
            mIsGooglePlayServicesAvailableCount = 0;
        }
    }

    public void assertNoOtherMethodsCalled() {
        synchronized (mLock) {
            assertEquals(0, mConnectWithTimeoutCount);
            assertEquals(0, mDisconnectCount);
            assertEquals(0, mIsGooglePlayServicesAvailableCount);
        }
    }

    protected Object getLock() {
        return mLock;
    }
}
