// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_MOCKS_MOCK_SKILLS_SERVICE_H_
#define COMPONENTS_SKILLS_MOCKS_MOCK_SKILLS_SERVICE_H_

#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace skills {

class MockSkillsService : public SkillsService {
 public:
  MockSkillsService();
  MockSkillsService(const MockSkillsService&) = delete;
  MockSkillsService& operator=(const MockSkillsService&) = delete;
  ~MockSkillsService() override;

  MOCK_METHOD(void, LoadInitialSkills, (std::vector<std::unique_ptr<Skill>>));
  MOCK_METHOD(const Skill*, GetSkillById, (std::string_view), (const));
  MOCK_METHOD(const std::vector<std::unique_ptr<Skill>>&,
              GetSkills,
              (),
              (const));
  MOCK_METHOD(const SkillsMap&, Get1PSkills, (), (const));
  MOCK_METHOD(const Skill*,
              AddSkill,
              (const std::string&,
               const std::string&,
               const std::string&,
               const std::string&));
  MOCK_METHOD(const Skill*,
              AddOrUpdateSkillFromSync,
              (std::string_view,
               std::string_view,
               std::string_view,
               std::string_view,
               std::string_view,
               std::string_view,
               base::Time,
               base::Time,
               sync_pb::SkillSource));
  MOCK_METHOD(
      const Skill*,
      UpdateSkill,
      (std::string_view, std::string_view, std::string_view, std::string_view));
  MOCK_METHOD(bool, IsInitialized, (), (const));
  MOCK_METHOD(ServiceStatus, GetServiceStatus, (), (const));
  MOCK_METHOD(void, DeleteSkill, (std::string_view, UpdateSource));
  MOCK_METHOD(void, FetchDiscoverySkills, ());
  MOCK_METHOD(void, Handle1pSkillsMap, (std::unique_ptr<SkillsMap>));
  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
  MOCK_METHOD(void, SyncStatusChanged, ());
  MOCK_METHOD(void, SetServiceStatusForTesting, (ServiceStatus));
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_MOCKS_MOCK_SKILLS_SERVICE_H_
