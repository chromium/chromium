// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.mock_service;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.ip_protection_auth.common.IErrorCode;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetProxyConfigCallback;

import java.util.ArrayList;
import java.util.List;

/** Mock IP Protection auth service which deliberately crashes at configurable times. */
public abstract class CrashingService extends Service {
    private static final String TAG = "CrashingService";

    // Number of requests needed to trigger a crash.
    protected abstract int getRequestLimit();

    // Whether to call callback.reportError or just leave it hanging.
    protected abstract boolean isResponsive();

    // Whether to crash synchronously in the binder handler or crash async on the UI thread.
    //
    // Prefer testing with sync crashes as these are more deterministic.
    protected abstract boolean isSynchronous();

    void maybeSynchronous(Runnable r) {
        if (isSynchronous()) {
            r.run();
        } else {
            // Post with a slight delay to make it unlikely to beat the binder call's return.
            // This still doesn't strictly guarantee anything.
            ThreadUtils.postOnUiThreadDelayed(r, 100);
        }
    }

    void crash() {
        int pid = Process.myPid();
        Log.i(TAG, "killing own process (PID %d) to mimick service crash", pid);
        Process.killProcess(pid);
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate for %s", this.getClass().getName());
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy for %s", this.getClass().getName());
    }

    @Override
    public IBinder onBind(Intent intent) {
        final String className = this.getClass().getName();
        Log.i(TAG, "returning %s binding for %s", className, intent.toString());
        if (getRequestLimit() <= 0) {
            throw new AssertionError("request limit must be > 0");
        }
        return new IIpProtectionAuthService.Stub() {
            int mRequestsRemaining = getRequestLimit();
            // Make things well-defined by avoiding garbage collection of unresolved callbacks.
            final List<Object> mNotGarbage = new ArrayList<>();

            @Override
            public synchronized void getInitialData(
                    byte[] bytes, IIpProtectionGetInitialDataCallback callback) {
                maybeSynchronous(
                        () -> {
                            mRequestsRemaining--;
                            Log.i(
                                    TAG,
                                    "got getInitialData request for %s, %d requests left before"
                                            + " planned crash",
                                    className,
                                    mRequestsRemaining);
                            if (isResponsive()) {
                                try {
                                    callback.reportError(
                                            IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
                                } catch (RemoteException e) {
                                    throw new RuntimeException(e);
                                }
                            } else {
                                mNotGarbage.add(callback);
                            }
                            maybeCrash();
                        });
                // Binder call not strictly guaranteed to return before any UI thread work runs.
            }

            @Override
            public synchronized void authAndSign(
                    byte[] bytes, IIpProtectionAuthAndSignCallback callback) {
                maybeSynchronous(
                        () -> {
                            mRequestsRemaining--;
                            Log.i(
                                    TAG,
                                    "got authAndSign request for %s, %d requests left before"
                                            + " planned crash",
                                    className,
                                    mRequestsRemaining);
                            if (isResponsive()) {
                                try {
                                    callback.reportError(
                                            IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
                                } catch (RemoteException e) {
                                    throw new RuntimeException(e);
                                }
                            } else {
                                mNotGarbage.add(callback);
                            }
                            maybeCrash();
                        });
                // Binder call not strictly guaranteed to return before any UI thread work runs.
            }

            @Override
            public synchronized void getProxyConfig(
                    byte[] bytes, IIpProtectionGetProxyConfigCallback callback) {
                maybeSynchronous(
                        () -> {
                            mRequestsRemaining--;
                            Log.i(
                                    TAG,
                                    "got getProxyConfig request for %s, %d requests left before"
                                            + " planned crash",
                                    className,
                                    mRequestsRemaining);
                            if (isResponsive()) {
                                try {
                                    callback.reportError(
                                            IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
                                } catch (RemoteException e) {
                                    throw new RuntimeException(e);
                                }
                            } else {
                                mNotGarbage.add(callback);
                            }
                            maybeCrash();
                        });
                // Binder call not strictly guaranteed to return before any UI thread work runs.
            }

            void maybeCrash() {
                if (mRequestsRemaining <= 0) {
                    crash();
                }
            }
        };
    }

    public static class CrashOnRequestSyncWithoutResponse extends CrashingService {
        @Override
        protected int getRequestLimit() {
            return 1;
        }

        @Override
        protected boolean isResponsive() {
            return false;
        }

        @Override
        protected boolean isSynchronous() {
            return true;
        }
    }

    public static class CrashOnRequestAsyncWithoutResponse extends CrashingService {
        @Override
        protected int getRequestLimit() {
            return 1;
        }

        @Override
        protected boolean isResponsive() {
            return false;
        }

        @Override
        protected boolean isSynchronous() {
            return false;
        }
    }

    public static class CrashOnRequestSyncWithResponse extends CrashingService {
        @Override
        protected int getRequestLimit() {
            return 1;
        }

        @Override
        protected boolean isResponsive() {
            return true;
        }

        @Override
        protected boolean isSynchronous() {
            return true;
        }
    }

    public static class CrashAfterTwoRequestsSyncWithoutResponses extends CrashingService {
        @Override
        protected int getRequestLimit() {
            return 2;
        }

        @Override
        protected boolean isResponsive() {
            return false;
        }

        @Override
        protected boolean isSynchronous() {
            return true;
        }
    }
}
