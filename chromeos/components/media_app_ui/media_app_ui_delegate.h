// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_

#include <string>

#include "base/optional.h"

// A delegate which exposes browser functionality from //chrome to the media app
// ui page handler.
class MediaAppUIDelegate {
 public:
  virtual ~MediaAppUIDelegate() = default;

  // Opens the native chrome feedback dialog scoped to chrome://media-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual base::Optional<std::string> OpenFeedbackDialog() = 0;
};

#endif  // CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
