// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.progress

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/** Composable progress bar displaying [ProgressUiState]. */
@Composable
fun ProgressBar(
  state: ProgressUiState,
  modifier: Modifier = Modifier,
) {
  Column(modifier = modifier.padding(16.dp)) {
    // Track
    Box(
      modifier =
        Modifier.fillMaxWidth()
          .height(8.dp)
          .background(
            color = MaterialTheme.colorScheme.secondaryContainer,
            shape = CircleShape,
          )
    ) {
      // Fill
      Box(
        modifier =
          Modifier.fillMaxWidth(state.progressFraction.coerceIn(0f, 1f))
            .fillMaxHeight()
            .background(
              color = MaterialTheme.colorScheme.primary,
              shape = CircleShape,
            )
      )
    }

    Spacer(modifier = Modifier.height(8.dp))

    Text(
      text =
        when {
          state.isRunning -> "Running... ${(state.progressFraction * 100).toInt()}%"
          state.progressFraction >= 1f -> "Done!"
          else -> "Idle"
        }
    )
  }
}
