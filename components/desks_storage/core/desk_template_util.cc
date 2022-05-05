// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Duplicate value format.
constexpr char kDuplicateNumberFormat[] = "(%d)";
// Initial duplicate number value.
constexpr char kInitialDuplicateNumberValue[] = " (1)";
// Regex used in determining if duplicate name should be incremented.
constexpr char kDuplicateNumberRegex[] = "\\(([0-9]+)\\)$";
constexpr char kTestPwaAppId[] = "test_pwa_app_id";
constexpr char kTestChromeAppId[] = "test_chrome_app_id";
constexpr char kTestArcAppId[] = "test_arc_app_id";
constexpr char kTestChromeAppId1[] = "test_chrome_app_1";
constexpr char kTestPwaAppId1[] = "test_pwa_app_1";

}  // namespace

namespace desks_storage {

namespace desk_template_util {

apps::AppPtr MakeApp(const char* app_id,
                     const char* name,
                     apps::AppType app_type) {
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = apps::Readiness::kReady;
  app->name = name;
  return app;
}

std::u16string AppendDuplicateNumberToDuplicateName(
    const std::u16string& duplicate_name_u16) {
  std::string duplicate_name = base::UTF16ToUTF8(duplicate_name_u16);
  int found_duplicate_number;

  if (RE2::PartialMatch(duplicate_name, kDuplicateNumberRegex,
                        &found_duplicate_number)) {
    RE2::Replace(
        &duplicate_name, kDuplicateNumberRegex,
        base::StringPrintf(kDuplicateNumberFormat, found_duplicate_number + 1));
  } else {
    duplicate_name.append(kInitialDuplicateNumberValue);
  }

  return base::UTF8ToUTF16(duplicate_name);
}

void PopulateAppRegistryCache(AccountId account_id,
                              apps::AppRegistryCache* cache) {
  std::vector<apps::AppPtr> deltas;

  deltas.push_back(MakeApp(kTestPwaAppId, "Test PWA App", apps::AppType::kWeb));
  // chromeAppId returns kExtension in the real Apps cache.
  deltas.push_back(MakeApp(app_constants::kChromeAppId, "Chrome Browser",
                           apps::AppType::kChromeApp));
  deltas.push_back(
      MakeApp(kTestChromeAppId, "Test Chrome App", apps::AppType::kChromeApp));
  deltas.push_back(MakeApp(kTestArcAppId, "Arc app", apps::AppType::kArc));
  deltas.push_back(
      MakeApp(kTestPwaAppId1, "Test PWA App", apps::AppType::kWeb));
  deltas.push_back(
      MakeApp(kTestChromeAppId1, "Test Chrome App", apps::AppType::kChromeApp));

  if (base::FeatureList::IsEnabled(apps::kAppServiceOnAppUpdateWithoutMojom)) {
    cache->OnApps(std::move(deltas), apps::AppType::kUnknown,
                  /*should_notify_initialized=*/false);
  } else {
    std::vector<apps::mojom::AppPtr> mojom_deltas;
    for (const auto& delta : deltas) {
      mojom_deltas.push_back(apps::ConvertAppToMojomApp(delta));
    }
    cache->OnApps(std::move(mojom_deltas), apps::mojom::AppType::kUnknown,
                  /*should_notify_initialized=*/false);
  }

  cache->SetAccountId(account_id);

  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id, cache);
}

void AddAppIdToAppRegistryCache(AccountId account_id,
                                apps::AppRegistryCache* cache,
                                const char* app_id) {
  std::vector<apps::AppPtr> deltas;

  // We need to add the app as any type that's not a `apps::AppType::kChromeApp`
  // since there's a default hard coded string for that type, which will merge
  // all app_id to it.
  deltas.push_back(MakeApp(app_id, "Arc app", apps::AppType::kArc));

  if (base::FeatureList::IsEnabled(apps::kAppServiceOnAppUpdateWithoutMojom)) {
    cache->OnApps(std::move(deltas), apps::AppType::kUnknown,
                  /*should_notify_initialized=*/false);
  } else {
    std::vector<apps::mojom::AppPtr> mojom_deltas;
    for (const auto& delta : deltas) {
      mojom_deltas.push_back(apps::ConvertAppToMojomApp(delta));
    }
    cache->OnApps(std::move(mojom_deltas), apps::mojom::AppType::kUnknown,
                  /*should_notify_initialized=*/false);
  }

  cache->SetAccountId(account_id);

  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id, cache);
}

}  // namespace desk_template_util

}  // namespace desks_storage
