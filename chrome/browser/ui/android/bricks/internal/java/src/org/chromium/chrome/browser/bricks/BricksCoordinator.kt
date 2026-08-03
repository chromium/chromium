// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks

import android.content.Context
import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.google.android.material.color.DynamicColors
import org.chromium.chrome.browser.bricks.progress.ProgressCoordinator
import org.chromium.chrome.browser.bricks.progress.VibesProgressProvider
import org.chromium.components.browser_ui.styles.ChromeColors
import org.chromium.ui.util.ColorUtils

/** Coordinator for Bricks feature, manages [ComposeView]. */
class BricksCoordinator(context: Context) : BricksCoordinatorInterface {
    private val mProvider = VibesProgressProvider()
    private val mProgressCoordinator = ProgressCoordinator(mProvider)

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
        mProgressCoordinator.destroy()
        mProvider.destroy()
    }

    @Composable
    private fun BricksContent() {
        val context = LocalContext.current
        val isDark = ColorUtils.inNightMode(context)
        // TODO(crbug.com/518038940): Come up with a way to use the color scheme from the activity
        // context. We will need to figure this out for the whole app at some point.
        val colorScheme = when {
            DynamicColors.isDynamicColorAvailable() -> {
                if (isDark) {
                    dynamicDarkColorScheme(context)
                } else {
                    dynamicLightColorScheme(context)
                }
            }
            isDark -> darkColorScheme()
            else -> lightColorScheme()
        }
        MaterialTheme(colorScheme = colorScheme) {
            Surface(
                modifier = Modifier.fillMaxSize(),
                color = MaterialTheme.colorScheme.surface
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        text = "Hello from Bricks Compose native page!",
                        style = MaterialTheme.typography.titleMedium
                    )

                    Spacer(modifier = Modifier.height(16.dp))

                    Button(onClick = mProvider::triggerProgress) {
                        Text("Trigger Progress")
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    mProgressCoordinator.Content()
                }
            }
        }
    }
}
