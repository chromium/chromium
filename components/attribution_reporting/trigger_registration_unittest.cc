// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_registration.h"

#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {
namespace {

TEST(TriggerRegistrationTest, Create) {
  EXPECT_FALSE(TriggerRegistration::Create(
      /*reporting_origin=*/url::Origin::Create(GURL("http://insecure.com")),
      /*filters=*/Filters(),
      /*not_filters=*/Filters(),
      /*debug_key=*/absl::nullopt,
      /*aggregatable_dedup_key=*/absl::nullopt,
      /*event_triggers=*/{}, /*aggregatable_trigger_data=*/{},
      AggregatableValues(), /*debug_reporting=*/false));

  EXPECT_TRUE(TriggerRegistration::Create(
      /*reporting_origin=*/url::Origin::Create(GURL("https://secure.com")),
      /*filters=*/Filters(),
      /*not_filters=*/Filters(),
      /*debug_key=*/absl::nullopt,
      /*aggregatable_dedup_key=*/absl::nullopt,
      /*event_triggers=*/{}, /*aggregatable_trigger_data=*/{},
      AggregatableValues(), /*debug_reporting=*/false));
}

}  // namespace
}  // namespace attribution_reporting
