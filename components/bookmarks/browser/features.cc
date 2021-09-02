// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/features.h"

namespace bookmarks {
namespace features {

// Changes the apps shortcut on the bookmarks bar to default to off.
// https://crbug.com/1236793
const base::Feature kAppsShortcutDefaultOff{"AppsShortcutDefaultOff",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace bookmarks
