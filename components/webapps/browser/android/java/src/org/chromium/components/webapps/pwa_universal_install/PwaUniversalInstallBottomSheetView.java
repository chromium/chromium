// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.util.Pair;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout.LayoutParams;

import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.webapps.R;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;

/** The view portion of the PWA Universal Install bottom sheet. */
public class PwaUniversalInstallBottomSheetView {
    private static final int APP_ICON_SIZE_DP = 40;
    private static final int APP_ICON_CORNER_RADIUS_DP = 20;
    private static final int APP_ICON_TEXT_SIZE_DP = 24;

    // The details of the bottom sheet.
    private View mContentView;

    public PwaUniversalInstallBottomSheetView() {}

    public void initialize(
            Context context,
            WebContents webContents,
            Callable<Pair<Bitmap, Boolean>> iconCall,
            int arrowId) {
        mContentView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.pwa_universal_install_bottom_sheet_content,
                                /* root= */ null);

        // Draw background shape with rounded-corners for both options.
        mContentView
                .findViewById(R.id.option_install)
                .setBackgroundResource(R.drawable.pwa_restore_app_item_background_top);
        mContentView
                .findViewById(R.id.option_shortcut)
                .setBackgroundResource(R.drawable.pwa_restore_app_item_background_bottom);

        // Add a 4dp separator view as a separate item in the ScrollView so as to not affect the
        // height of the app item view (or mess up the rounded corners).
        View separator = mContentView.findViewById(R.id.separator);
        separator.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        (int)
                                TypedValue.applyDimension(
                                        TypedValue.COMPLEX_UNIT_DIP,
                                        4,
                                        context.getResources().getDisplayMetrics())));

        // Setup the app icon, with a placeholder as fallback in case of an error.
        Pair<Bitmap, Boolean> iconWithMetadata;
        try {
            iconWithMetadata = iconCall.call();
        } catch (Exception exception) {
            iconWithMetadata = null;
        }
        Bitmap appIcon = iconWithMetadata != null ? iconWithMetadata.first : null;
        boolean isAdaptive = iconWithMetadata != null ? iconWithMetadata.second : false;
        assert (!isAdaptive || Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                : "Adaptive icons should not be provided pre-Android O.";

        if (appIcon == null) {
            int iconColor = context.getColor(R.color.default_favicon_background_color);
            RoundedIconGenerator iconGenerator =
                    new RoundedIconGenerator(
                            context.getResources(),
                            APP_ICON_SIZE_DP,
                            APP_ICON_SIZE_DP,
                            APP_ICON_CORNER_RADIUS_DP,
                            iconColor,
                            APP_ICON_TEXT_SIZE_DP);
            appIcon = iconGenerator.generateIconForText("?");
        }

        ImageView app_icon_install = mContentView.findViewById(R.id.app_icon_install);
        ImageView app_icon_shortcut = mContentView.findViewById(R.id.app_icon_shortcut);
        if (isAdaptive) {
            app_icon_install.setImageIcon(Icon.createWithAdaptiveBitmap(appIcon));
            app_icon_shortcut.setImageIcon(Icon.createWithAdaptiveBitmap(appIcon));
        } else {
            app_icon_install.setImageBitmap(appIcon);
            app_icon_shortcut.setImageBitmap(appIcon);
        }

        if (arrowId != 0) {
            ((ImageView) mContentView.findViewById(R.id.arrow_install))
                    .setBackgroundResource(arrowId);
            ((ImageView) mContentView.findViewById(R.id.arrow_shortcut))
                    .setBackgroundResource(arrowId);
        }
    }

    public View getContentView() {
        return mContentView;
    }

    int getPeekHeight() {
        return mContentView.getHeight();
    }
}
