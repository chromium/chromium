// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_registry.h"

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefRegistryTest, GetRegisteredPrefType) {
  auto registry = base::MakeRefCounted<PrefRegistrySimple>();
  registry->RegisterStringPref("string_pref", std::string());
  registry->RegisterTimePref("time_pref", base::Time());

  EXPECT_EQ(PrefRegistry::RegisteredPrefType::kOther,
            registry->GetRegisteredPrefType("string_pref"));
  EXPECT_EQ(PrefRegistry::RegisteredPrefType::kTime,
            registry->GetRegisteredPrefType("time_pref"));
  EXPECT_EQ(std::nullopt, registry->GetRegisteredPrefType("unregistered_pref"));
}
