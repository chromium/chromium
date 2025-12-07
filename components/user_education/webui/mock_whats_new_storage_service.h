// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_MOCK_WHATS_NEW_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_MOCK_WHATS_NEW_STORAGE_SERVICE_H_

#include "components/user_education/webui/whats_new_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockWhatsNewStorageService : public whats_new::WhatsNewStorageService {
 public:
  MockWhatsNewStorageService();
  ~MockWhatsNewStorageService() override;

  MOCK_METHOD(const base::Value::List&, ReadModuleData, (), (const override));
  MOCK_METHOD(const base::Value::Dict&, ReadEditionData, (), (const, override));
  MOCK_METHOD(std::optional<int>, ReadVersionData, (), (const, override));
  MOCK_METHOD(std::optional<int>,
              GetUsedVersion,
              (std::string_view edition_name),
              (const override));
  MOCK_METHOD(std::optional<std::string_view>,
              FindEditionForCurrentVersion,
              (),
              (const, override));
  MOCK_METHOD(int,
              GetModuleQueuePosition,
              (std::string_view),
              (const, override));
  MOCK_METHOD(bool, IsUsedEdition, (std::string_view), (const, override));
  MOCK_METHOD(bool,
              WasVersionPageUsedForCurrentMilestone,
              (),
              (const, override));
  MOCK_METHOD(void, SetModuleEnabled, (std::string_view), (override));
  MOCK_METHOD(void, SetEditionUsed, (std::string_view), (override));
  MOCK_METHOD(void, SetVersionUsed, (), (override));
  MOCK_METHOD(void, ClearModules, (std::set<std::string_view>), (override));
  MOCK_METHOD(void, ClearEditions, (std::set<std::string_view>), (override));
  MOCK_METHOD(void, Reset, (), (override));
};

#endif  // COMPONENTS_USER_EDUCATION_WEBUI_MOCK_WHATS_NEW_STORAGE_SERVICE_H_
