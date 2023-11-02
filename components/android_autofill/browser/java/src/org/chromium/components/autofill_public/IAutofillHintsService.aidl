// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_public;

import android.os.Bundle;

import org.chromium.components.autofill_public.IViewTypeCallback;

/**
 * Interface to provide the autofill hints that are unable to be supported
 * by Android framework.
 *
 * The autofill service could get the binder from ViewStructure.
 *     Bundle bundle = viewStructure.getExtras();
 *     IBinder binder = bundle.getBinder("AUTOFILL_HINTS_SERVICE");
 */
interface IAutofillHintsService {
    // Register the IViewTypeCallback to get the server prediction type.
    void registerViewTypeCallback(IViewTypeCallback callback);
}
