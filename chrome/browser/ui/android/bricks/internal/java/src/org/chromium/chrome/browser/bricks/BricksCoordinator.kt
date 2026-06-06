// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks

import android.content.Context
import android.view.View
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import org.chromium.components.browser_ui.styles.ChromeColors

/** Coordinator for Bricks feature, manages Compose view. */
class BricksCoordinator(context: Context) : BricksCoordinatorInterface {
    private val mComposeView: ComposeView = ComposeView(context).apply {
        setBackgroundColor(
            ChromeColors.getPrimaryBackgroundColor(context, /* isIncognito= */ false)
        )
        setContent {
            BricksContent()
        }
    }

    override fun getView(): View = mComposeView

    override fun destroy() {
        // Cleanup if needed.
    }

    @Composable
    fun BricksContent() {
        MaterialTheme {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("Hello from Bricks Compose native page!")
            }
        }
    }
}
