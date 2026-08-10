// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.progress

/** State for the progress bar. */
data class ProgressUiState(
  val progressFraction: Float = 0f,
  val isRunning: Boolean = false,
)
