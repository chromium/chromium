// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.chromium.chrome.browser.ChromeTabbedActivity;

/**
 * Custom ActivityTestRule for tests using ChromeTabbedActivity
 */
public class ChromeTabbedActivityTestRule extends ChromeActivityTestRule<ChromeTabbedActivity> {
    public ChromeTabbedActivityTestRule() {
        super(ChromeTabbedActivity.class);
    }
}
