// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tabs;

import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;

import org.chromium.build.annotations.NullMarked;

/** Type token for the C++ tabs::TabStripCollection pointer. */
@NullMarked
@JniType("tabs::TabStripCollection")
public interface TabStripCollection extends JniTypeToken {}
