// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.app.Application;

import androidx.annotation.OptIn;

import org.chromium.net.ConnectionMigrationOptions;
import org.chromium.net.CronetEngine;
import org.chromium.net.DnsOptions;
import org.chromium.net.QuicOptions;

/** Application for managing the Cronet Sample. */
public class CronetSampleApplication extends Application {
    private CronetEngine mCronetEngine;

    @Override
    public void onCreate() {
        super.onCreate();
        CronetEngine.Builder myBuilder = new CronetEngine.Builder(this);
        myBuilder
                .enableHttpCache(CronetEngine.Builder.HTTP_CACHE_IN_MEMORY, 100 * 1024)
                .enableHttp2(true)
                .enableQuic(true);
        mCronetEngine = myBuilder.build();
    }

    public CronetEngine getCronetEngine() {
        return mCronetEngine;
    }

    @OptIn(
            markerClass = {
                org.chromium.net.QuicOptions.Experimental.class,
                org.chromium.net.ConnectionMigrationOptions.Experimental.class
            })
    public void restartCronetEngine() {
        mCronetEngine.shutdown();
        CronetEngine.Builder myBuilder = new CronetEngine.Builder(this);
        ConnectionMigrationOptions.Builder connecMigrationBuilder =
                ConnectionMigrationOptions.builder();
        QuicOptions.Builder quicOptionsBuilder = QuicOptions.builder();
        DnsOptions.Builder dnsOptionsBuilder = DnsOptions.builder();
        ActionData actionData =
                new ActionData.Builder()
                        .setCronetEngineBuilder(myBuilder)
                        .setMigrationBuilder(connecMigrationBuilder)
                        .setQuicBuilder(quicOptionsBuilder)
                        .setDnsBuilder(dnsOptionsBuilder)
                        .build();
        for (Options.Option<?> option : Options.getOptions()) {
            option.configure(actionData);
        }
        mCronetEngine =
                myBuilder
                        .setConnectionMigrationOptions(connecMigrationBuilder)
                        .setQuicOptions(quicOptionsBuilder)
                        .setDnsOptions(dnsOptionsBuilder)
                        .enableHttpCache(CronetEngine.Builder.HTTP_CACHE_IN_MEMORY, 100 * 1024)
                        .enableHttp2(true)
                        .enableQuic(true)
                        .build();
    }
}
