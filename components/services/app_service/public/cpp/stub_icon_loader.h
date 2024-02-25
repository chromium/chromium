// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_STUB_ICON_LOADER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_STUB_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>

#include "components/services/app_service/public/cpp/icon_loader.h"

namespace apps {

// Helper IconLoader implementation to served canned answers for testing.
class StubIconLoader : public IconLoader {
 public:
  StubIconLoader();

  StubIconLoader(const StubIconLoader&) = delete;
  StubIconLoader& operator=(const StubIconLoader&) = delete;

  ~StubIconLoader() override;

  // IconLoader overrides.
  std::optional<IconKey> GetIconKey(const std::string& id) override;
  std::unique_ptr<Releaser> LoadIconFromIconKey(
      const std::string& id,
      const IconKey& icon_key,
      IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) override;

  int NumLoadIconFromIconKeyCalls();

  std::map<std::string, int32_t> update_version_by_app_id_;

 private:
  int num_load_calls_ = 0;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_STUB_ICON_LOADER_H_
