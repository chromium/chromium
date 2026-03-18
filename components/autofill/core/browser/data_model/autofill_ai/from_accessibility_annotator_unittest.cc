// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::testing::UnorderedElementsAre;

namespace aa = accessibility_annotator;

// Tests that a set of accessibility annotator entity types is correctly
// mapped to a set of Autofill AI entity types.
TEST(FromAccessibilityAnnotatorTest, EntityTypeEnumSet) {
  EXPECT_THAT(
      FromAccessibilityAnnotator({aa::EntityType::kFlightReservation,
                                  aa::EntityType::kOrder,
                                  aa::EntityType::kDriversLicense}),
      UnorderedElementsAre(EntityType(EntityTypeName::kFlightReservation),
                           EntityType(EntityTypeName::kOrder),
                           EntityType(EntityTypeName::kDriversLicense)));
}

}  // namespace
}  // namespace autofill
