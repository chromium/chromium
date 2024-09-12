// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import android.content.Context;

import androidx.annotation.MainThread;

// TODO(b/355054098): Remove after the 2-sided patch series for cross-repo refactoring is done.
// Currently we need it because the internal repo still expects the existence of this class.
@Deprecated
public class SearchEngineCountryDelegateImpl extends SearchEngineChoiceServiceDelegateImpl {
    @MainThread
    public SearchEngineCountryDelegateImpl(Context context) {
        super(context);
    }
}
