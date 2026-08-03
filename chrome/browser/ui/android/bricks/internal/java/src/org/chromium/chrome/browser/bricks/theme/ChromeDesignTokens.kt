// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.android.material.color.DynamicColors
import org.chromium.ui.util.ColorUtils

/**
 * Applies Chromium's MaterialTheme color scheme (Light/Dark & Dynamic Blue) to Compose UI.
 */
@Composable
fun ChromeMaterialTheme(content: @Composable () -> Unit) {
    val context = LocalContext.current
    val isDark = ColorUtils.inNightMode(context)
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
        content()
    }
}

/**
 * Chromium design tokens for Compose UI, mirroring Clank XML styles and dimensions.
 */
object ChromeDimens {
    /** Scale factor for MaterialSwitch to match Widget.BrowserUI.Switch */
    const val SWITCH_SCALE_FRACTION = 0.8f

    /** Minimum touch target height for switch rows (48dp) */
    val MIN_TOUCH_TARGET_HEIGHT: Dp = 48.dp

    /** Horizontal padding for list items and preference rows */
    val ROW_HORIZONTAL_PADDING: Dp = 16.dp

    /** Vertical padding for preference switch rows */
    val ROW_VERTICAL_PADDING: Dp = 8.dp

    /** Gap between text column and switch widget */
    val SWITCH_TEXT_GAP: Dp = 16.dp
}

/**
 * Typography definitions mirroring Clank XML TextAppearance styles.
 */
object ChromeTypography {
    val TEXT_SIZE_LARGE = 16.sp
    val TEXT_SIZE_LARGE_LINE_HEIGHT = 24.sp

    val TEXT_SIZE_MEDIUM = 14.sp
    val TEXT_SIZE_MEDIUM_LINE_HEIGHT = 20.sp

    val HEADLINE_SMALL_SIZE = 20.sp
    val HEADLINE_SMALL_LINE_HEIGHT = 28.sp

    /** Mirrors @style/TextAppearance.TextLarge.Primary */
    @Composable
    fun textLargePrimary(enabled: Boolean = true): TextStyle = TextStyle(
        fontSize = TEXT_SIZE_LARGE,
        lineHeight = TEXT_SIZE_LARGE_LINE_HEIGHT,
        color = ChromeColorTokens.primaryTextColor(enabled)
    )

    /** Mirrors @style/TextAppearance.TextMedium.Secondary */
    @Composable
    fun textMediumSecondary(enabled: Boolean = true): TextStyle = TextStyle(
        fontSize = TEXT_SIZE_MEDIUM,
        lineHeight = TEXT_SIZE_MEDIUM_LINE_HEIGHT,
        color = ChromeColorTokens.secondaryTextColor(enabled)
    )

    /** Mirrors @style/TextAppearance.Headline */
    val headlineSmall: TextStyle
        @Composable get() = TextStyle(
            fontSize = HEADLINE_SMALL_SIZE,
            lineHeight = HEADLINE_SMALL_LINE_HEIGHT,
            color = MaterialTheme.colorScheme.onSurface
        )
}

/**
 * Color token functions mirroring Clank semantic color lists.
 */
object ChromeColorTokens {
    /** Alpha value applied to text/icons in disabled state (38%) */
    const val DISABLED_ALPHA = 0.38f

    /** Primary text color taking into account enabled state */
    @Composable
    fun primaryTextColor(enabled: Boolean = true): Color {
        return if (enabled) MaterialTheme.colorScheme.onSurface
        else MaterialTheme.colorScheme.onSurface.copy(alpha = DISABLED_ALPHA)
    }

    /** Secondary text color taking into account enabled state */
    @Composable
    fun secondaryTextColor(enabled: Boolean = true): Color {
        return if (enabled) MaterialTheme.colorScheme.onSurfaceVariant
        else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = DISABLED_ALPHA)
    }
}
