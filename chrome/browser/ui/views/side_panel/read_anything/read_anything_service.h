// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// This class holds profile-scoped state for the read anything feature.
// TODO(https://crbug.com/355485153): This class is currently a stub.
class ReadAnythingService : public KeyedService {
 public:
  explicit ReadAnythingService(Profile* profile);

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SERVICE_H_
