// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_

#include <memory>
#include <string>
#include "base/values.h"

// A delegate which exposes browser functionality from //chrome to the
// FileManagerUI handler.
class FileManagerUIDelegate {
 public:
  virtual ~FileManagerUIDelegate() = default;

  // Returns a map from message labels to actual messages used by the Files App.
  virtual std::unique_ptr<base::DictionaryValue> GetFileManagerAppStrings()
      const = 0;
};

#endif  // CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
