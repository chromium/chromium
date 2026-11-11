// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;

import org.chromium.build.annotations.NullMarked;

/**
 * Type token for the native {@code BrowserWindowInterface}. Use it as the type argument of {@code
 * JniPtr} and {@code JniUniquePtr} to pass a native browser window across JNI.
 */
@NullMarked
@JniType("::BrowserWindowInterface")
public interface NativeBrowserWindowInterface extends JniTypeToken {}
