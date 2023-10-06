// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * A {@link BasePageStation} used as an entry point.
 *
 * <p>Differs from the {@link PageStation} only in that it uses fewer enter conditions since the
 * TabModel does not exist before the transition, so it cannot be observed.
 */
public class EntryPageStation extends BasePageStation {
    public EntryPageStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
        super(chromeTabbedActivityTestRule, incognito);
    }
}
