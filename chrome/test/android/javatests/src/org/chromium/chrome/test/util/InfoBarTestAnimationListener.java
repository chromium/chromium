// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarAnimationListener;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;

import java.util.concurrent.TimeoutException;

/**
 * Allow tests to register for animation finished notifications.
 */
public class InfoBarTestAnimationListener implements InfoBarAnimationListener {
    private final CallbackHelper mAddAnimationFinished;
    private final CallbackHelper mSwapAnimationFinished;
    private final CallbackHelper mRemoveAnimationFinished;

    private int mAddCallCount;
    private int mSwapCallCount;
    private int mRemoveCallCount;

    public InfoBarTestAnimationListener() {
        mAddAnimationFinished = new CallbackHelper();
        mSwapAnimationFinished = new CallbackHelper();
        mRemoveAnimationFinished = new CallbackHelper();
    }

    @Override
    public void notifyAnimationFinished(int animationType) {
        switch(animationType) {
            case InfoBarAnimationListener.ANIMATION_TYPE_SHOW:
                mAddAnimationFinished.notifyCalled();
                break;
            case InfoBarAnimationListener.ANIMATION_TYPE_SWAP:
                mSwapAnimationFinished.notifyCalled();
                break;
            case InfoBarAnimationListener.ANIMATION_TYPE_HIDE:
                mRemoveAnimationFinished.notifyCalled();
                break;
            default:
                throw new UnsupportedOperationException(
                        "Animation finished for unknown type " + animationType);
        }
    }

    @Override
    public void notifyAllAnimationsFinished(Item frontInfoBar) {}

    public void addInfoBarAnimationFinished(String msg) throws TimeoutException {
        mAddAnimationFinished.waitForCallback(msg, mAddCallCount);
        mAddCallCount++;
    }

    public void swapInfoBarAnimationFinished(String msg) throws TimeoutException {
        mSwapAnimationFinished.waitForCallback(msg, mSwapCallCount);
        mSwapCallCount++;
    }

    public void removeInfoBarAnimationFinished(String msg) throws TimeoutException {
        mRemoveAnimationFinished.waitForCallback(msg, mRemoveCallCount);
        mRemoveCallCount++;
    }
}
