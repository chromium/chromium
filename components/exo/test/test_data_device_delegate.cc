// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_data_device_delegate.h"

#include "components/exo/data_offer_delegate.h"
#include "components/exo/test/test_data_offer_delegate.h"

namespace exo::test {

TestDataDeviceDelegate::TestDataDeviceDelegate() = default;
TestDataDeviceDelegate::~TestDataDeviceDelegate() = default;

size_t TestDataDeviceDelegate::PopEvents(std::vector<DataEvent>* out) {
  out->swap(events_);
  events_.clear();
  return out->size();
}

void TestDataDeviceDelegate::DeleteDataOffer(bool finished) {
  if (finished) {
    data_offer_->Finish();
  }
  data_offer_.reset();
}

void TestDataDeviceDelegate::OnDataDeviceDestroying(DataDevice* data_device) {
  events_.push_back(DataEvent::kDestroy);
}

DataOffer* TestDataDeviceDelegate::OnDataOffer() {
  events_.push_back(DataEvent::kOffer);
  // Reset data_offer_ while delegate is still valid.
  data_offer_.reset();
  data_offer_delegate_ = std::make_unique<TestDataOfferDelegate>();
  data_offer_ = std::make_unique<DataOffer>(data_offer_delegate_.get());
  return data_offer_.get();
}

void TestDataDeviceDelegate::OnEnter(Surface* surface,
                                     const gfx::PointF& location,
                                     const DataOffer& data_offer) {
  events_.push_back(DataEvent::kEnter);
  entered_surface_ = surface;
}

void TestDataDeviceDelegate::OnLeave() {
  events_.push_back(DataEvent::kLeave);
}

void TestDataDeviceDelegate::OnMotion(base::TimeTicks time_stamp,
                                      const gfx::PointF& location) {
  events_.push_back(DataEvent::kMotion);
}

void TestDataDeviceDelegate::OnDrop() {
  events_.push_back(DataEvent::kDrop);
}

void TestDataDeviceDelegate::OnSelection(const DataOffer& data_offer) {
  events_.push_back(DataEvent::kSelection);
}

bool TestDataDeviceDelegate::CanAcceptDataEventsForSurface(
    Surface* surface) const {
  return can_accept_data_events_for_surface_;
}

}  // namespace exo::test
