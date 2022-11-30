// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

/** Represents various params used for opening the {@link OfflineItem}. */
public class OpenParams {
    /** The location at which the opened item will be displayed. */
    public final @LaunchLocation int location;

    /** Whether the item will be opened in incognito mode. */
    public boolean openInIncognito;

    /** Constructor. */
    public OpenParams(@LaunchLocation int location) {
        this.location = location;
    }
}
