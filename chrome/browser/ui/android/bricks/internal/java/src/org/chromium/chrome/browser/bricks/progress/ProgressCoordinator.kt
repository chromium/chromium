// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.progress

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import kotlinx.coroutines.flow.MutableStateFlow

/** Coordinator for the [ProgressBar] component. */
class ProgressCoordinator(provider: ProgressProvider) {
  private val mStateFlow = MutableStateFlow(ProgressUiState())
  private val mProvider = provider
  private val mMediator = ProgressMediator(mStateFlow, mProvider)

  /** Composable content for the progress bar. */
  @Composable
  fun Content(modifier: Modifier = Modifier) {
    val state by mStateFlow.collectAsStateWithLifecycle()
    ProgressBar(state = state, modifier = modifier)
  }

  /** Destroys the [ProgressCoordinator] and its dependencies. */
  fun destroy() {
    mMediator.destroy()
  }
}
