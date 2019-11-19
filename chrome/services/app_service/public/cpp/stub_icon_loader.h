// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_STUB_ICON_LOADER_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_STUB_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/services/app_service/public/cpp/icon_loader.h"

namespace apps {

// Helper IconLoader implementation to served canned answers for testing.
class StubIconLoader : public IconLoader {
 public:
  StubIconLoader();
  ~StubIconLoader() override;

  // IconLoader overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;

  int NumLoadIconFromIconKeyCalls();

  std::map<std::string, uint64_t> timelines_by_app_id_;

 private:
  int num_load_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(StubIconLoader);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_STUB_ICON_LOADER_H_
