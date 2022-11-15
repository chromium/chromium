// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.text;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewStructure;

import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.url.GURL;

/**
 * Wrapper class needed due to b/122113958.
 *
 * Note that for password fields the hint text is expected to be set in XML so that it is available
 * during inflation.
 */
public class AlertDialogEditText extends AppCompatEditText {
    private GURL mUrl;

    public AlertDialogEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setUrl(GURL url) {
        mUrl = url;
    }

    @Override
    @SuppressLint("NewApi")
    public void onProvideAutofillStructure(ViewStructure structure, int flags) {
        if (mUrl != null && !mUrl.isEmpty()) {
            structure.setWebDomain(mUrl.getSpec());
        }
        super.onProvideAutofillStructure(structure, flags);
    }
}
