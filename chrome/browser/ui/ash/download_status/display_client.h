// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_CLIENT_H_

#include <string>

namespace ash::download_status {

struct DisplayMetadata;

// The virtual base class of Ash classes that display download updates.
class DisplayClient {
 public:
  virtual ~DisplayClient() = default;

  // Adds or updates the displayed download specified by `guid` with the given
  // display metadata.
  virtual void AddOrUpdate(const std::string& guid,
                           const DisplayMetadata& display_metadata) = 0;

  // Removes the displayed download specified by `guid`.
  virtual void Remove(const std::string& guid) = 0;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_CLIENT_H_
