// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_STORAGE_MANAGER_ARC_STORAGE_MANAGER_H_
#define COMPONENTS_ARC_STORAGE_MANAGER_ARC_STORAGE_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/arc/mojom/storage_manager.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class represents as a simple proxy of StorageManager to Chrome OS.
class ArcStorageManager : public KeyedService {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcStorageManager* GetForBrowserContext(
      content::BrowserContext* context);

  ArcStorageManager(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcStorageManager() override;

  // Opens detailed preference screen of private volume on ARC.
  // Returns false when an instance of ARC-side isn't ready yet.
  bool OpenPrivateVolumeSettings();

  // Gets storage usage of all application's APK, data, and cache size.
  using GetApplicationsSizeCallback =
      base::OnceCallback<void(bool succeeded, mojom::ApplicationsSizePtr)>;
  bool GetApplicationsSize(GetApplicationsSizeCallback callback);

  // Deletes all applications' cache files.
  bool DeleteApplicationsCache(const base::Callback<void()>& callback);

 private:
  ArcBridgeService* const arc_bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcStorageManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_STORAGE_MANAGER_ARC_STORAGE_MANAGER_H_
