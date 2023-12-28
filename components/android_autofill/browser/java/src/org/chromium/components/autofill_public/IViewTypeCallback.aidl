// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_public;

import android.os.Bundle;

import org.chromium.components.autofill_public.ViewType;

/**
 * The interface for AutofillHintsService to provide the type of view.
 */
interface IViewTypeCallback {
    // Invoked when the query succeeds, though the server might not have the
    // prediction of the views.
    void onViewTypeAvailable(in List<ViewType> viewTypes);

    // @deprecated - no longer called by WebView. Do not use, will be removed.
    void onQueryFailed();
}
