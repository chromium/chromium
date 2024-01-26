// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the properties that a pwa-universal-install {@link PropertyModel} can have. */
public class PwaUniversalInstallProperties {
    static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyKey[] ALL_KEYS = {TITLE};

    static PropertyModel createModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
