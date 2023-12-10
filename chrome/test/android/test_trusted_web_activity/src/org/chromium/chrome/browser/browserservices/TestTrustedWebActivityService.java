// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.trusted.Token;
import androidx.browser.trusted.TokenStore;
import androidx.browser.trusted.TrustedWebActivityCallbackRemote;
import androidx.browser.trusted.TrustedWebActivityService;

/** A TrustedWebActivityService to be used in TrustedWebActivityClientTest. */
public class TestTrustedWebActivityService extends TrustedWebActivityService {
    public static final String COMMAND_SET_RESPONSE = "setResponse";
    public static final String SET_RESPONSE_NAME = "setResponse.name";
    public static final String SET_RESPONSE_BUNDLE = "setResponse.bundle";
    public static final String SET_RESPONSE_RESPONSE = "setResponse.response";

    // TODO(peconn): Add an image resource to chrome_public_test_support, supply that in
    // getSmallIconId and verify it is used in notifyNotificationWithChannel.
    public static final int SMALL_ICON_ID = 42;

    private static final String CHECK_LOCATION_PERMISSION_COMMAND_NAME =
            "checkAndroidLocationPermission";
    private static final String LOCATION_PERMISSION_RESULT = "locationPermissionResult";
    private static final String START_LOCATION_COMMAND_NAME = "startLocation";
    private static final String STOP_LOCATION_COMMAND_NAME = "stopLocation";
    private static final String EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK = "onNewLocationAvailable";
    private static final String EXTRA_COMMAND_SUCCESS = "success";

    private final TokenStore mTokenStore = new InMemoryStore();
    private String mResponseName;
    private Bundle mResponseBundle;

    @Override
    public void onCreate() {
        super.onCreate();

        Token chromeTestToken = Token.create("org.chromium.chrome.tests", getPackageManager());
        mTokenStore.store(chromeTestToken);
    }

    @NonNull
    @Override
    public TokenStore getTokenStore() {
        return mTokenStore;
    }

    @Override
    public boolean onNotifyNotificationWithChannel(
            String platformTag, int platformId, Notification notification, String channelName) {
        MessengerService.sMessageHandler.recordNotifyNotification(
                platformTag, platformId, channelName);
        return true;
    }

    @Override
    public boolean onAreNotificationsEnabled(@NonNull String channelName) {
        // Pretend notifications are enabled, even on Android T where they'll be disabled by
        // default.
        return true;
    }

    @Override
    public void onCancelNotification(String platformTag, int platformId) {
        MessengerService.sMessageHandler.recordCancelNotification(platformTag, platformId);
    }

    @Override
    public int onGetSmallIconId() {
        MessengerService.sMessageHandler.recordGetSmallIconId();
        return SMALL_ICON_ID;
    }

    @Nullable
    @Override
    public Bundle onExtraCommand(
            String commandName, Bundle args, @Nullable TrustedWebActivityCallbackRemote callback) {
        Bundle executionResult = new Bundle();
        executionResult.putBoolean(EXTRA_COMMAND_SUCCESS, true);

        switch (commandName) {
            case CHECK_LOCATION_PERMISSION_COMMAND_NAME:
                if (callback == null) break;

                Bundle permission = new Bundle();
                permission.putBoolean(LOCATION_PERMISSION_RESULT, true);
                runCallback(callback, CHECK_LOCATION_PERMISSION_COMMAND_NAME, permission);
                break;
            case START_LOCATION_COMMAND_NAME:
                if (callback == null) break;

                Bundle locationResult = new Bundle();
                locationResult.putDouble("latitude", 1.0);
                locationResult.putDouble("longitude", -2.0);
                locationResult.putDouble("accuracy", 0.5);
                locationResult.putLong("timeStamp", System.currentTimeMillis());
                runCallback(callback, EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK, locationResult);
                break;
            case STOP_LOCATION_COMMAND_NAME:
                break;
            case COMMAND_SET_RESPONSE:
                mResponseName = args.getString(SET_RESPONSE_NAME);
                mResponseBundle = args.getBundle(SET_RESPONSE_BUNDLE);
                runCallback(callback, SET_RESPONSE_RESPONSE, null);
                break;
            default:
                if (mResponseBundle != null) {
                    runCallback(callback, mResponseName, mResponseBundle);
                } else {
                    executionResult.putBoolean(EXTRA_COMMAND_SUCCESS, false);
                }
        }
        return executionResult;
    }

    private static void runCallback(
            TrustedWebActivityCallbackRemote callback, String name, Bundle args) {
        if (callback == null) return;
        try {
            callback.runExtraCallback(name, args);
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    private static class InMemoryStore implements TokenStore {
        private Token mToken;

        @Override
        public void store(@Nullable Token token) {
            mToken = token;
        }

        @Nullable
        @Override
        public Token load() {
            return mToken;
        }
    }
}
