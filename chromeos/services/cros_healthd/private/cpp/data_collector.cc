// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/data_collector.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {
namespace {

class DataCollectorImpl : public DataCollector,
                          public mojom::ChromiumDataCollector {
 public:
  DataCollectorImpl();
  DataCollectorImpl(const DataCollectorImpl&) = delete;
  DataCollectorImpl& operator=(const DataCollectorImpl&) = delete;
  ~DataCollectorImpl() override;

  // DataCollector overrides.
  void BindReceiver(
      mojo::PendingReceiver<mojom::ChromiumDataCollector> receiver) override;

 private:
  // mojom::ChromiumDataCollector overrides.
  void GetTouchscreenDevices(GetTouchscreenDevicesCallback callback) override;
  void GetTouchpadLibraryName(GetTouchpadLibraryNameCallback callback) override;

  // The receiver set to keep the mojo receivers.
  mojo::ReceiverSet<mojom::ChromiumDataCollector> receiver_set_;
};

DataCollectorImpl::DataCollectorImpl() = default;

DataCollectorImpl::~DataCollectorImpl() = default;

void DataCollectorImpl::BindReceiver(
    mojo::PendingReceiver<mojom::ChromiumDataCollector> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void DataCollectorImpl::GetTouchscreenDevices(
    GetTouchscreenDevicesCallback callback) {
  NOTIMPLEMENTED();
}

void DataCollectorImpl::GetTouchpadLibraryName(
    GetTouchpadLibraryNameCallback callback) {
  NOTIMPLEMENTED();
}

// The pointer to the global instance.
DataCollector* g_instance = nullptr;

};  // namespace

DataCollector::DataCollector() {
  CHECK(!g_instance) << "Can have only one instance";
  g_instance = this;
}

DataCollector::~DataCollector() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
void DataCollector::Initialize() {
  new DataCollectorImpl();
}

// static
void DataCollector::Shutdown() {
  delete g_instance;
}

// static
DataCollector* DataCollector::Get() {
  CHECK(g_instance) << "Not initialized.";
  return g_instance;
}

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
