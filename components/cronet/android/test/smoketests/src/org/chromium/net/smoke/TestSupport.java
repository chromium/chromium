// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.content.Context;

import org.json.JSONObject;

import org.chromium.net.ExperimentalCronetEngine;

import java.io.File;

/**
 * Provides support for tests, so they can be run in different environments against different
 * servers.
 */
public interface TestSupport {
    enum Protocol {
        HTTP1,
        HTTP2,
        QUIC,
    }

    /**
     * Creates a new test server that supports a given {@code protocol}.
     *
     * @param context context.
     * @param protocol protocol that should be supported by the server.
     * @return an instance of the server.
     *
     * @throws UnsupportedOperationException if the implementation of this interface
     *                                       does not support a given {@code protocol}.
     */
    TestServer createTestServer(Context context, Protocol protocol);

    /**
     * This method is called at the end of a test run if the netlog is available. An implementer
     * of {@link TestSupport} can use it to process the result netlog; e.g., to copy the netlog
     * to a directory where all test logs are collected. This method is optional and can be no-op.
     *
     * @param file the netlog file.
     */
    void processNetLog(Context context, File file);

    /**
     * Adds host resolver rules to a given experimental option JSON file.
     * This method is optional.
     *
     * @param experimentalOptionsJson experimental options.
     */
    void addHostResolverRules(JSONObject experimentalOptionsJson);

    /**
     * Installs mock certificate verifier for a given {@code builder}.
     * This method is optional.
     *
     * @param builder that should have the verifier installed.
     */
    void installMockCertVerifierForTesting(ExperimentalCronetEngine.Builder builder);

    /** Loads a native library that is required for testing if any required. */
    void loadTestNativeLibrary();

    /** A test server. */
    interface TestServer {
        /**
         * Starts the server.
         *
         * @return true if the server started successfully.
         */
        boolean start();

        /** Shuts down the server. */
        void shutdown();

        /**
         * Return a URL that can be used by the test code to receive a successful response.
         *
         * @return the URL as a string.
         */
        String getSuccessURL();
    }
}
