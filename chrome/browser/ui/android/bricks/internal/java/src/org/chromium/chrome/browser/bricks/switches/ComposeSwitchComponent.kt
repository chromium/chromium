// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.switches

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.scale
import org.chromium.chrome.browser.bricks.theme.ChromeDimens
import org.chromium.chrome.browser.bricks.theme.ChromeTypography

/** Composable switch row displaying text label alongside Compose [Switch]. */
@Composable
fun ComposeSwitchWithText(
    text: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    scaleFraction: Float = ChromeDimens.SWITCH_SCALE_FRACTION
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .defaultMinSize(minHeight = ChromeDimens.MIN_TOUCH_TARGET_HEIGHT)
            .clickable(enabled = enabled) { onCheckedChange(!checked) },
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = text,
            style = ChromeTypography.textLargePrimary(enabled),
            modifier = Modifier.weight(1f)
        )
        Spacer(modifier = Modifier.width(ChromeDimens.SWITCH_TEXT_GAP))
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            enabled = enabled,
            modifier = if (scaleFraction != 1.0f) Modifier.scale(scaleFraction) else Modifier
        )
    }
}

/** Composable switch row displaying title and summary alongside Compose [Switch]. */
@Composable
fun ComposeSwitchWithTitleAndSummary(
    title: String,
    summary: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    scaleFraction: Float = ChromeDimens.SWITCH_SCALE_FRACTION
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .defaultMinSize(minHeight = ChromeDimens.MIN_TOUCH_TARGET_HEIGHT)
            .clickable(enabled = enabled) { onCheckedChange(!checked) },
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title,
                style = ChromeTypography.textLargePrimary(enabled)
            )
            Text(
                text = summary,
                style = ChromeTypography.textMediumSecondary(enabled)
            )
        }
        Spacer(modifier = Modifier.width(ChromeDimens.SWITCH_TEXT_GAP))
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            enabled = enabled,
            modifier = if (scaleFraction != 1.0f) Modifier.scale(scaleFraction) else Modifier
        )
    }
}
