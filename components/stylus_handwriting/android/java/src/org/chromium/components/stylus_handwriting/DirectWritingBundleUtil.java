// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_EDIT_RECT;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_EDIT_RECT_RELOCATED;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_EVENT;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_ROOT_VIEW_RECT;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_SERVICE_HOST_SOURCE;
import static android.widget.directwriting.IDirectWritingService.VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW;

import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;

/** Utility class for creating Direct Writing service Bundle for various service calls as needed. */
class DirectWritingBundleUtil {
    private DirectWritingBundleUtil() {}

    static Bundle buildBundle(MotionEvent me, View rootView) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(KEY_BUNDLE_EVENT, me);
        bundle.putParcelable(KEY_BUNDLE_ROOT_VIEW_RECT, getViewBoundsOnScreen(rootView));
        return bundle;
    }

    static Bundle buildBundle(MotionEvent me, Rect editRect, View rootView) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(KEY_BUNDLE_EVENT, me);
        bundle.putParcelable(KEY_BUNDLE_EDIT_RECT, editRect);
        bundle.putParcelable(KEY_BUNDLE_ROOT_VIEW_RECT, getViewBoundsOnScreen(rootView));
        bundle.putString(KEY_BUNDLE_SERVICE_HOST_SOURCE, VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW);
        return bundle;
    }

    static Rect getViewBoundsOnScreen(View view) {
        int[] viewCoordinates = new int[2];
        view.getLocationOnScreen(viewCoordinates);
        int x = viewCoordinates[0];
        int y = viewCoordinates[1];
        int width = view.getWidth();
        int height = view.getHeight();

        return new Rect(x, y, x + width, y + height);
    }

    static Bundle buildBundle(Rect rect, View rootView, boolean isOnlyRectChanged) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(KEY_BUNDLE_EDIT_RECT, rect);
        bundle.putParcelable(KEY_BUNDLE_ROOT_VIEW_RECT, getViewBoundsOnScreen(rootView));
        bundle.putBoolean(KEY_BUNDLE_EDIT_RECT_RELOCATED, isOnlyRectChanged);
        bundle.putString(KEY_BUNDLE_SERVICE_HOST_SOURCE, VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW);
        return bundle;
    }

    static Bundle buildBundle() {
        Bundle bundle = new Bundle();
        bundle.putString(KEY_BUNDLE_SERVICE_HOST_SOURCE, VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW);
        return bundle;
    }
}
