// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_

// A delegate which exposes browser functionality from //chrome to the
// FileManagerUI handler.
class FileManagerUIDelegate {
 public:
  virtual ~FileManagerUIDelegate() = default;
};

#endif  // CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
