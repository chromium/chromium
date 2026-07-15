// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type_util.h"

#include <vector>

#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

// Tests that `ToPersonalContextEntity` correctly converts individual memory
// entry attributes and metadata into the corresponding fields of the personal
// context Entity proto message.
TEST(MemoryDataTypeUtilTest, ToPersonalContextEntity) {
  // Test Passport conversion.
  {
    std::u16string value = u"P12345";
    MemoryDataType memory_data_type = MemoryDataType::kPassportNumber;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kPassportName, u"Passport Name",
                               u"Jane Doe");
    metadata_list.emplace_back(MemoryDataType::kPassportCountry,
                               u"Passport Country", u"US");
    metadata_list.emplace_back(MemoryDataType::kPassportExpirationDate,
                               u"Passport Expiration Date", u"2030-05-20");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_passport());
    EXPECT_EQ(entity.passport().number(), "P12345");
    EXPECT_EQ(entity.passport().name(), "Jane Doe");
    EXPECT_EQ(entity.passport().issuing_country(), "US");
    EXPECT_EQ(entity.passport().expiration_date().year(), 2030);
    EXPECT_EQ(entity.passport().expiration_date().month(), 5);
    EXPECT_EQ(entity.passport().expiration_date().day(), 20);
  }

  // Test Order conversion, including order date parsing and product names list.
  {
    std::u16string value = u"ORD-123";
    MemoryDataType memory_data_type = MemoryDataType::kOrderId;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kOrderDate, u"Order Date",
                               u"2026-07-07");
    metadata_list.emplace_back(MemoryDataType::kOrderProductNames, u"Products",
                               u"Book A, Toy B");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_order());
    EXPECT_EQ(entity.order().order_id(), "ORD-123");
    EXPECT_EQ(entity.order().order_date().year(), 2026);
    EXPECT_EQ(entity.order().order_date().month(), 7);
    EXPECT_EQ(entity.order().order_date().day(), 7);
    ASSERT_EQ(entity.order().product_names_size(), 2);
    EXPECT_EQ(entity.order().product_names(0), "Book A");
    EXPECT_EQ(entity.order().product_names(1), "Toy B");
  }

  // Test Shipment conversion, including associated order IDs parsing and
  // shipped date parsing.
  {
    std::u16string value = u"TRACK-888";
    MemoryDataType memory_data_type = MemoryDataType::kShipmentTrackingNumber;

    std::vector<EntryMetadata> metadata_list;
    metadata_list.emplace_back(MemoryDataType::kShipmentAssociatedOrderId,
                               u"Order IDs", u"ORD-001, ORD-002");
    metadata_list.emplace_back(MemoryDataType::kShipmentShippedDate,
                               u"Ship Date", u"2026-07-07");

    personal_context::proto::Entity entity =
        ToPersonalContextEntity(value, memory_data_type, metadata_list);

    ASSERT_TRUE(entity.has_shipment());
    EXPECT_EQ(entity.shipment().tracking_number(), "TRACK-888");
    ASSERT_EQ(entity.shipment().associated_order_ids_size(), 2);
    EXPECT_EQ(entity.shipment().associated_order_ids(0), "ORD-001");
    EXPECT_EQ(entity.shipment().associated_order_ids(1), "ORD-002");
    EXPECT_EQ(entity.shipment().ship_date().year(), 2026);
    EXPECT_EQ(entity.shipment().ship_date().month(), 7);
    EXPECT_EQ(entity.shipment().ship_date().day(), 7);
  }
}

}  // namespace

}  // namespace accessibility_annotator
