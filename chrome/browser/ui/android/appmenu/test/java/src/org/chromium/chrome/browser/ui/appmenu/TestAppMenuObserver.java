// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import org.chromium.base.test.util.CallbackHelper;

/**
 * AppMenuObserver that notifies callbacks when app menu events occur.
 */
public class TestAppMenuObserver implements AppMenuObserver {
    public CallbackHelper menuShownCallback = new CallbackHelper();
    public CallbackHelper menuHiddenCallback = new CallbackHelper();
    public CallbackHelper menuHighlightChangedCallback = new CallbackHelper();
    public boolean menuHighlighting;

    @Override
    public void onMenuVisibilityChanged(boolean isVisible) {
        if (isVisible) {
            menuShownCallback.notifyCalled();
        } else {
            menuHiddenCallback.notifyCalled();
        }
    }

    @Override
    public void onMenuHighlightChanged(boolean highlighting) {
        menuHighlighting = highlighting;
        menuHighlightChangedCallback.notifyCalled();
    }
}
