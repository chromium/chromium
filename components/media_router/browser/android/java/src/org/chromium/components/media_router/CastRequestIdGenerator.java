// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

/**
 * Returns a request id in a range that is considered fairly unique. These request ids are used to
 * communicate with the cast device and identify messages and their responses.
 */
public class CastRequestIdGenerator {
    private static final Object LOCK = new Object();
    private static CastRequestIdGenerator sInstance;

    private int mRequestId;

    /** Returns the next requestId in the range allocated to communicate with the device. */
    public static int getNextRequestId() {
        CastRequestIdGenerator instance = getInstance();

        // Return the current |mRequestId| then increment. Never return 0 because it is reserved.
        if (instance.mRequestId == 0) ++instance.mRequestId;
        return instance.mRequestId++;
    }

    /** Returns the Singleton instance of the CastRequestIdGenerator. */
    private static CastRequestIdGenerator getInstance() {
        synchronized (LOCK) {
            if (sInstance == null) sInstance = new CastRequestIdGenerator();
        }
        return sInstance;
    }

    private CastRequestIdGenerator() {
        mRequestId = (int) Math.floor(Math.random() * 100000.0) * 1000;
    }
}
