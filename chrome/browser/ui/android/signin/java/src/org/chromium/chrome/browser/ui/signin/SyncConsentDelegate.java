// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

/** Defines the host interface for Sync Consent page. */
public interface SyncConsentDelegate {
    /** Return the {@link WindowAndroid} for the host activity. */
    @Nullable
    WindowAndroid getWindowAndroid();

    /** Return the {@link Profile} for the Sync Consent. */
    @Nullable
    Profile getProfile();
}
