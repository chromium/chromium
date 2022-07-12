// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;

/**
 * Utility class for creating Direct Writing service Bundle for various service calls as needed.
 */
class DirectWritingBundleUtil {
    // TODO(mahesh.ma): Replace the following constants with values from Direct writing service aidl
    // when it is added.
    private static final String KEY_BUNDLE_EVENT = "KEY_BUNDLE_EVENT";
    private static final String KEY_BUNDLE_ROOT_VIEW_RECT = "KEY_BUNDLE_ROOT_VIEW_RECT";
    private static final String KEY_BUNDLE_EDIT_RECT = "KEY_BUNDLE_EDIT_RECT";
    private static final String KEY_BUNDLE_SERVICE_HOST_SOURCE = "KEY_BUNDLE_SERVICE_HOST_SOURCE";
    private static final String VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW =
            "VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW";

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

    static Bundle buildBundle(Rect rect, View rootView) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(KEY_BUNDLE_EDIT_RECT, rect);
        bundle.putParcelable(KEY_BUNDLE_ROOT_VIEW_RECT, getViewBoundsOnScreen(rootView));
        bundle.putString(KEY_BUNDLE_SERVICE_HOST_SOURCE, VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW);
        return bundle;
    }

    static Bundle buildBundle() {
        Bundle bundle = new Bundle();
        bundle.putString(KEY_BUNDLE_SERVICE_HOST_SOURCE, VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW);
        return bundle;
    }
}
