// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

/**
 * Interface implemented by services provide device logs
 *
 * WARNING: Please read before updating this file.
 *  Rules for updating this file:
 *  - DO NOT change or remove methods that are already defined.
 *  - Methods must be added to the bottom of this file.
 *  - When adding a method, please increment the VERSION field
 *    defined in implementors.
 *  - The presence of new methods must be verified in the service
 *    by checking the API version via getApiVersion().
 *
 * API CHANGE LOG (Update when methods are added):
 * VERSION 1: Initial API version.
 */
interface IDeviceLogsProvider {
    /**
     * The current API version. Whenever a method is added to this file,
     * please ++ this value and update the change log at the top.
     */
    const int VERSION = 1;

    /**
     * Filename of location of logs provided from remote service.
     */
    String getLogs();
}