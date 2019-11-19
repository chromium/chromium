// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to get notified of
 * native having been loaded.
 */
public interface NativeInitObserver extends LifecycleObserver {
    /**
     * Called when the native library has finished loading.
     */
    void onFinishNativeInitialization();
}
