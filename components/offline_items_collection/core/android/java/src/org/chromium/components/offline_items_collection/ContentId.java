// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import android.text.TextUtils;

/**
 * This class is a Java counterpart to the C++ ContentId
 * (components/offline_items_collection/core/offline_item.h) class.
 *
 * For all member variable descriptions see the C++ class.
 * TODO(dtrainor): Investigate making all class members for this and the C++ counterpart const.
 */
public class ContentId {
    public String namespace;
    public String id;

    public ContentId() {}

    public ContentId(String namespace, String id) {
        assert namespace == null || !namespace.contains(",");
        this.namespace = namespace != null ? namespace : "";
        this.id = id != null ? id : "";
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof ContentId)) return false;

        ContentId rhs = (ContentId) o;
        return TextUtils.equals(namespace, rhs.namespace) && TextUtils.equals(id, rhs.id);
    }

    @Override
    public int hashCode() {
        int result = 61;

        result = 31 * result + (namespace == null ? 0 : namespace.hashCode());
        result = 31 * result + (id == null ? 0 : id.hashCode());

        return result;
    }

    @Override
    public String toString() {
        return namespace + "_" + id;
    }
}
