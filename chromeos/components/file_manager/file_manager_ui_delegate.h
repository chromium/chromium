// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_

namespace content {
class WebUIDataSource;
}  // namespace content

// Delegate to expose //chrome services to //components FileManagerUI.
class FileManagerUIDelegate {
 public:
  virtual ~FileManagerUIDelegate() = default;

  // Populates (writes) load time data to the source.
  virtual void PopulateLoadTimeData(content::WebUIDataSource*) const = 0;
};

#endif  // CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
