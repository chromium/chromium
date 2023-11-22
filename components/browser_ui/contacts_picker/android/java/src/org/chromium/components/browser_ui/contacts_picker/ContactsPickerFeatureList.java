// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

/**
 * Lists base::Features that can be accessed through {@link ContactsPickerFeatureMap}.
 *
 * <p>Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/browser_ui/contacts_picker/android/contacts_picker_feature_map.cc
 */
public abstract class ContactsPickerFeatureList {
    public static final String CONTACTS_PICKER_SELECT_ALL = "ContactsPickerSelectAll";
}
