// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_
#define CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class Profile;

// BrowserManagerService is responsible for owning and destroying Browser object
// instances for a given Profile.
// TODO(crbug.com/431671448): Expand this API to support
// browser_window_interface_iterator functionality (such as tracking Browsers in
// order of activation per-profile).
class BrowserManagerService : public KeyedService {
 public:
  explicit BrowserManagerService(Profile* profile);
  ~BrowserManagerService() override;

  // KeyedService:
  void Shutdown() override;

  // Adds a new Browser to be owned by the service.
  void AddBrowser(std::unique_ptr<Browser> browser);

  // Destroys `browser` if owned and managed by the service.
  void DeleteBrowser(Browser* browser);

 private:
  // Profile associated with this service.
  const raw_ptr<Profile> profile_;

  std::vector<std::unique_ptr<Browser>> browsers_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_H_
