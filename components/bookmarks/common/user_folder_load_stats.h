// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_USER_FOLDER_LOAD_STATS_H_
#define COMPONENTS_BOOKMARKS_COMMON_USER_FOLDER_LOAD_STATS_H_

namespace bookmarks {

// Stats regarding user-generated bookmark folders recorded at profile load
// time.
struct UserFolderLoadStats {
  int total_folders = 0;
  int total_top_level_folders = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_USER_FOLDER_LOAD_STATS_H_
