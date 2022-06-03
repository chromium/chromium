// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_IMPORT_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_IMPORT_FLOW_H_

namespace password_manager {

// This represents the controller for the UI flow of importing passwords.
class ImportFlow {
 public:
  // Load the source to be imported.
  virtual void Load() = 0;

 protected:
  virtual ~ImportFlow() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_IMPORT_FLOW_H_
