// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Interface to get language profile data for device. */
public interface LanguageProfileDelegate {
    /**
     * @param accountName Account to get profile or null if the default profile should be returned.
     * @param timeoutInSeconds Seconds to wait before timing out on call to device.
     * @return A list of language tags ordered by preference for |accountName|
     */
    public List<String> getLanguagePreferences(String accountName, int timeoutInSeconds)
            throws ExecutionException, InterruptedException, TimeoutException;
}
