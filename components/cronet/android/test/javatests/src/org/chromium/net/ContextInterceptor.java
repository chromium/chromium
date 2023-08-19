// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.ContextWrapper;

/**
 * An interface for providing a replacement for an Android {@link Context}. Useful for
 * faking/mocking Context calls in tests.
 *
 * @see CronetTestRule.CronetTestFramework#interceptContext
 */
public interface ContextInterceptor {
    /**
     * Provides a {@link Context} that the current context should be replaced with.
     *
     * @param context the original Context to be replaced
     * @return the new Context. Typically this would forward most calls to the original context, for
     * example using {@link ContextWrapper}.
     */
    public Context interceptContext(Context context);
}
