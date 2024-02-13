// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_TEST_DATA_DEVICE_DELEGATE_H_
#define COMPONENTS_EXO_TEST_TEST_DATA_DEVICE_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/exo/data_device_delegate.h"

namespace exo::test {

class TestDataOfferDelegate;
enum class DataEvent {
  kOffer,
  kEnter,
  kLeave,
  kMotion,
  kDrop,
  kDestroy,
  kSelection
};

class TestDataDeviceDelegate : public DataDeviceDelegate {
 public:
  TestDataDeviceDelegate();

  TestDataDeviceDelegate(const TestDataDeviceDelegate&) = delete;
  TestDataDeviceDelegate& operator=(const TestDataDeviceDelegate&) = delete;

  ~TestDataDeviceDelegate() override;

  size_t PopEvents(std::vector<DataEvent>* out);
  Surface* entered_surface() const { return entered_surface_; }
  void DeleteDataOffer(bool finished);
  void set_can_accept_data_events_for_surface(bool value) {
    can_accept_data_events_for_surface_ = value;
  }

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* data_device) override;
  DataOffer* OnDataOffer() override;
  void OnEnter(Surface* surface,
               const gfx::PointF& location,
               const DataOffer& data_offer) override;
  void OnLeave() override;
  void OnMotion(base::TimeTicks time_stamp,
                const gfx::PointF& location) override;
  void OnDrop() override;
  void OnSelection(const DataOffer& data_offer) override;
  bool CanAcceptDataEventsForSurface(Surface* surface) const override;

 private:
  std::vector<DataEvent> events_;
  std::unique_ptr<TestDataOfferDelegate> data_offer_delegate_;
  std::unique_ptr<DataOffer> data_offer_;
  raw_ptr<Surface, DanglingUntriaged> entered_surface_ = nullptr;
  bool can_accept_data_events_for_surface_ = true;
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_TEST_DATA_DEVICE_DELEGATE_H_
