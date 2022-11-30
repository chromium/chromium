// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import org.chromium.components.webapps.R;

/**
 * UI for the zoomed image view used for screenshots in the bottom-sheet UI for PWA installs.
 */
public class ImageZoomView extends AlertDialog {
    public ImageZoomView(Context context, Bitmap bitmap) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);

        View view = LayoutInflater.from(context).inflate(R.layout.image_zoom_view, null);
        view.setOnClickListener(v -> dismiss());
        ((ImageView) view.findViewById(R.id.image_zoom)).setImageBitmap(bitmap);
        setView(view);
    }
}
