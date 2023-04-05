// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

#include <string>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

using DeskTemplateUtilTest = testing::Test;

TEST_F(DeskTemplateUtilTest, FindDuplicateEntry) {
  base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>> entries;
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  entries[uuid] = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);

  base::Uuid new_uuid = base::Uuid::GenerateRandomV4();
  entries[new_uuid] = std::make_unique<ash::DeskTemplate>(
      new_uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  EXPECT_TRUE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

TEST_F(DeskTemplateUtilTest, FindNoDuplicateEntryInFilledMap) {
  base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>> entries;
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  entries[uuid] = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);

  base::Uuid new_uuid = base::Uuid::GenerateRandomV4();
  entries[new_uuid] = std::make_unique<ash::DeskTemplate>(
      new_uuid, ash::DeskTemplateSource::kUser, "Template 2", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  EXPECT_FALSE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

TEST_F(DeskTemplateUtilTest, FindNoDuplicateEntryInAOneElementMap) {
  base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>> entries;
  base::Uuid uuid = base::Uuid::GenerateRandomV4();

  entries[uuid] = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  EXPECT_FALSE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

TEST_F(DeskTemplateUtilTest, FindNoDuplicateEntryWithFloatingWorkspaceName) {
  base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>> entries;
  base::Uuid uuid = base::Uuid::GenerateRandomV4();

  entries[uuid] = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);

  base::Uuid new_uuid = base::Uuid::GenerateRandomV4();
  entries[new_uuid] = std::make_unique<ash::DeskTemplate>(
      new_uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kFloatingWorkspace);
  EXPECT_FALSE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

}  // namespace desks_storage
