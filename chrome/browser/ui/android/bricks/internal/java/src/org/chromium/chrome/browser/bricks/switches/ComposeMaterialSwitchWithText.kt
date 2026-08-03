// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks.switches

import android.content.Context
import android.util.AttributeSet
import android.widget.Checkable
import android.widget.CompoundButton
import android.widget.Switch
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.AbstractComposeView
import org.chromium.chrome.browser.bricks.theme.ChromeMaterialTheme

/**
 * A Compose-based variant of MaterialSwitchWithText designed for Java interoperability.
 * Extends [AbstractComposeView] and provides a traditional Java View API (setText, setChecked, setOnCheckedChangeListener).
 */
class ComposeMaterialSwitchWithText @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : AbstractComposeView(context, attrs), Checkable {

    private var mText by mutableStateOf("")
    private var mChecked by mutableStateOf(false)
    private var mEnabled by mutableStateOf(true)
    private var mListener: CompoundButton.OnCheckedChangeListener? = null
    private val mSwitch by lazy { Switch(context) }

    init {
        if (attrs != null) {
            val typedArray = context.obtainStyledAttributes(attrs, intArrayOf(android.R.attr.text))
            try {
                typedArray.getString(0)?.let { mText = it }
            } finally {
                typedArray.recycle()
            }
        }
    }

    @Composable
    override fun Content() {
        ChromeMaterialTheme {
            ComposeSwitchWithText(
                text = mText,
                checked = mChecked,
                onCheckedChange = { newChecked ->
                    if (mEnabled && mChecked != newChecked) {
                        mChecked = newChecked
                        mListener?.onCheckedChanged(mSwitch, mChecked)
                    }
                },
                enabled = mEnabled
            )
        }
    }

    fun setText(text: CharSequence?) {
        mText = text?.toString() ?: ""
    }

    fun setText(resId: Int) {
        mText = context.getString(resId)
    }

    fun getText(): String = mText

    override fun setChecked(checked: Boolean) {
        if (mChecked != checked) {
            mChecked = checked
            mListener?.onCheckedChanged(mSwitch, mChecked)
        }
    }

    override fun isChecked(): Boolean = mChecked

    override fun toggle() {
        setChecked(!mChecked)
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)
        mEnabled = enabled
    }

    override fun isEnabled(): Boolean = mEnabled

    fun setOnCheckedChangeListener(listener: CompoundButton.OnCheckedChangeListener?) {
        mListener = listener
    }
}
