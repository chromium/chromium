// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.content.Context;
import android.content.Intent;

/**
 * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
 * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
 * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
 */
public interface CustomTabIntentHelper {
    /**
     * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
     */
    Intent createCustomTabActivityIntent(Context context, Intent intent);
}
