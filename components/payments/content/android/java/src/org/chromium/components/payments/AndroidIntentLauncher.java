// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** The interface for launching Android intent-based payment apps. */
@NullMarked
public interface AndroidIntentLauncher {
    /**
     * Launch the payment app via an intent.
     *
     * @param intent The intent that includes the payment app identification and parameters.
     * @param errorCallback The callback that is invoked with a web-developer visible error string,
     *     when invoking the payment app fails.
     * @param intentCallback The callback invoked when the payment app responds to the intent.
     */
    void launchPaymentApp(
            Intent intent,
            Callback<String> errorCallback,
            WindowAndroid.IntentCallback intentCallback);
}
