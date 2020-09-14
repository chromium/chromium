// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.content.Context;

/**
 * Concrete app restriction provider, that uses the default android mechanism to retrieve the
 * restrictions.
 * TODO(zmin): Delete once the internal code is migrated.
 */
public class AppRestrictionsProvider
        extends org.chromium.components.policy.AppRestrictionsProvider {
    public AppRestrictionsProvider(Context context) {
        super(context);
    }
}
