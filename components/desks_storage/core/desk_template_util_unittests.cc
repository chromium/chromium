// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

#include <string>
#include "base/time/time.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

using DeskTemplateUtilTest = testing::Test;

TEST_F(DeskTemplateUtilTest, FindDuplicateEntry) {
  std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>> entries;
  const base::GUID uuid = base::GUID::GenerateRandomV4();
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  entries[uuid] = std::move(desk_template);

  const base::GUID new_uuid = base::GUID::GenerateRandomV4();
  auto new_desk_template = std::make_unique<ash::DeskTemplate>(
      new_uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  entries[new_uuid] = std::move(new_desk_template);
  EXPECT_TRUE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

TEST_F(DeskTemplateUtilTest, FindNoDuplicateEntryInFilledMap) {
  std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>> entries;
  const base::GUID uuid = base::GUID::GenerateRandomV4();
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  entries[uuid] = std::move(desk_template);

  const base::GUID new_uuid = base::GUID::GenerateRandomV4();
  auto new_desk_template = std::make_unique<ash::DeskTemplate>(
      new_uuid, ash::DeskTemplateSource::kUser, "Template 2", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);
  entries[new_uuid] = std::move(new_desk_template);
  EXPECT_FALSE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

TEST_F(DeskTemplateUtilTest, FindNoDuplicateEntryInAOneElementMap) {
  std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>> entries;
  const base::GUID uuid = base::GUID::GenerateRandomV4();
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, "Template 1", base::Time::Now(),
      ash::DeskTemplateType::kTemplate);

  entries[uuid] = std::move(desk_template);
  EXPECT_FALSE(
      desk_template_util::FindOtherEntryWithName(u"Template 1", uuid, entries));
}

}  // namespace desks_storage
