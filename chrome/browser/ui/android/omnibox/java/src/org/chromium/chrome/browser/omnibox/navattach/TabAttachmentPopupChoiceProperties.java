// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for tabs shown in the attachments popup */
@NullMarked
public class TabAttachmentPopupChoiceProperties {

    public static final WritableObjectPropertyKey<Drawable> THUMBNAIL =
            new WritableObjectPropertyKey<>();

    /** The title of the tab. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {THUMBNAIL, TITLE};
}
