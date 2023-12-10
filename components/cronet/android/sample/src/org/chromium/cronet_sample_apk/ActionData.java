// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import org.chromium.net.ConnectionMigrationOptions;
import org.chromium.net.CronetEngine;
import org.chromium.net.DnsOptions;
import org.chromium.net.QuicOptions;

public class ActionData {
    private final CronetEngine.Builder mCronetEngineBuilder;
    private final ConnectionMigrationOptions.Builder mMigrationBuilder;
    private final QuicOptions.Builder mQuicBuilder;
    private final DnsOptions.Builder mDnsBuilder;

    private ActionData(ActionData.Builder builder) {
        this.mCronetEngineBuilder = builder.mCronetEngineBuilder;
        this.mMigrationBuilder = builder.mMigrationBuilder;
        this.mQuicBuilder = builder.mQuicBuilder;
        this.mDnsBuilder = builder.mDnsBuilder;
    }

    public CronetEngine.Builder getCronetEngineBuilder() {
        return mCronetEngineBuilder;
    }

    public DnsOptions.Builder getDnsBuilder() {
        return mDnsBuilder;
    }

    public ConnectionMigrationOptions.Builder getMigrationBuilder() {
        return mMigrationBuilder;
    }

    public QuicOptions.Builder getQuicBuilder() {
        return mQuicBuilder;
    }

    public static class Builder {
        private CronetEngine.Builder mCronetEngineBuilder;
        private ConnectionMigrationOptions.Builder mMigrationBuilder;
        private QuicOptions.Builder mQuicBuilder;
        private DnsOptions.Builder mDnsBuilder;

        public Builder setCronetEngineBuilder(CronetEngine.Builder cronetEngineBuilder) {
            this.mCronetEngineBuilder = cronetEngineBuilder;
            return this;
        }

        public Builder setDnsBuilder(DnsOptions.Builder dnsBuilder) {
            this.mDnsBuilder = dnsBuilder;
            return this;
        }

        public Builder setMigrationBuilder(ConnectionMigrationOptions.Builder migrationBuilder) {
            this.mMigrationBuilder = migrationBuilder;
            return this;
        }

        public Builder setQuicBuilder(QuicOptions.Builder quicBuilder) {
            this.mQuicBuilder = quicBuilder;
            return this;
        }

        public ActionData build() {
            return new ActionData(this);
        }
    }
}
