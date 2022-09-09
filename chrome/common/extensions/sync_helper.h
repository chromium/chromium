// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_SYNC_HELPER_H_
#define CHROME_COMMON_EXTENSIONS_SYNC_HELPER_H_

namespace extensions {

class Extension;

namespace sync_helper {

// NOTE: The check in the functions here only considers the data in extension
// itself, not the environment it is in. To determine whether an extension
// should be synced, you probably want to use util::ShouldSync.

// Returns true if |extension| should be synced.
bool IsSyncable(const Extension* extension);

// Component extensions usually aren't synced, but some are so that they'll
// retain their position in the app list. Returns true for component extensions
// that are allowed.
bool IsSyncableComponentExtension(const Extension* extension);

}  // namespace sync_helper
}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_SYNC_HELPER_H_
