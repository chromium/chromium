// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import android.content.Context;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Handles the additional message view responsibilities needed for InfoBars. - Makes the full text
 * view clickable if there is just a single link.
 */
public class InfoBarMessageView extends TextViewWithClickableSpans {
    private boolean mExternalOnClickListenerSet;

    public InfoBarMessageView(Context context) {
        super(context);
    }

    public InfoBarMessageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean retVal = super.onTouchEvent(event);
        if (!mExternalOnClickListenerSet && event.getActionMasked() == MotionEvent.ACTION_UP) {
            long downDuration = event.getEventTime() - event.getDownTime();
            boolean validClickEvent =
                    downDuration >= ViewConfiguration.getTapTimeout()
                            && downDuration <= ViewConfiguration.getLongPressTimeout();

            ClickableSpan[] spans = getClickableSpans();
            if (validClickEvent
                    && spans != null
                    && spans.length == 1
                    && !touchIntersectsAnyClickableSpans(event)) {
                spans[0].onClick(this);
            }
        }
        return retVal;
    }

    @Override
    public final void setOnClickListener(OnClickListener l) {
        super.setOnClickListener(l);
        if (l != null) mExternalOnClickListenerSet = true;
    }
}
