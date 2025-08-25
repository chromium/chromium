// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_STORAGE_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_STORAGE_SERVICE_IMPL_H_

#include <memory>

#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_education/webui/whats_new_storage_service.h"

namespace whats_new {
class WhatsNewStorageServiceImpl : public WhatsNewStorageService {
 public:
  WhatsNewStorageServiceImpl() = default;
  ~WhatsNewStorageServiceImpl() override;

  // Disallow copy and assign.
  WhatsNewStorageServiceImpl(const WhatsNewStorageServiceImpl&) = delete;
  WhatsNewStorageServiceImpl& operator=(const WhatsNewStorageServiceImpl&) =
      delete;

  // Read-only access into prefs.
  const base::Value::List& ReadModuleData() const override;
  const base::Value::Dict& ReadEditionData() const override;
  std::optional<int> ReadVersionData() const override;

  int GetModuleQueuePosition(std::string_view module_name) const override;
  std::optional<int> GetUsedVersion(
      std::string_view edition_name) const override;
  std::optional<std::string_view> FindEditionForCurrentVersion() const override;
  bool IsUsedEdition(std::string_view edition_name) const override;
  bool WasVersionPageUsedForCurrentMilestone() const override;

  void SetModuleEnabled(std::string_view module_name) override;
  void SetEditionUsed(std::string_view edition_name) override;
  void SetVersionUsed() override;

  void ClearModules(std::set<std::string_view> modules_to_clear) override;
  void ClearEditions(std::set<std::string_view> editions_to_clear) override;

  void Reset() override;

 private:
  ScopedListPrefUpdate GetEnabledOrder();
  ScopedDictPrefUpdate GetUsedEditions();
};
}  // namespace whats_new

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_STORAGE_SERVICE_IMPL_H_
