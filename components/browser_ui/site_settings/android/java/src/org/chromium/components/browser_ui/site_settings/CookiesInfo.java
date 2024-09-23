// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import java.io.Serializable;

/** Cookies information for a given origin. */
public class CookiesInfo implements Serializable {
    private int mCookies;

    public CookiesInfo() {
        mCookies = 0;
    }

    public CookiesInfo(int cookies) {
        mCookies = cookies;
    }

    public void increment() {
        mCookies++;
    }

    public int getCount() {
        return mCookies;
    }
}
