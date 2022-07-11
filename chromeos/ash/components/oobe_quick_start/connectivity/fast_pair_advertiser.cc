// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/fast_pair_advertiser.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/unguessable_token.h"

namespace {

constexpr const char kFastPairServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
constexpr uint8_t kFastPairModelId[] = {0x41, 0xc0, 0xd9};
constexpr uint16_t kCompanyId = 0x00e0;
constexpr const char kAdvertisingSuccessHistogramName[] =
    "OOBE.QuickStart.FastPairAdvertising";

}  // namespace

// static
FastPairAdvertiser::Factory* FastPairAdvertiser::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<FastPairAdvertiser> FastPairAdvertiser::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(adapter);

  return std::make_unique<FastPairAdvertiser>(adapter);
}

// static
void FastPairAdvertiser::Factory::SetFactoryForTesting(
    FastPairAdvertiser::Factory* factory) {
  factory_instance_ = factory;
}

FastPairAdvertiser::FastPairAdvertiser(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter && adapter->IsPresent() && adapter->IsPowered());
  adapter_ = adapter;
}

FastPairAdvertiser::~FastPairAdvertiser() {
  StopAdvertising(base::DoNothing());
}

void FastPairAdvertiser::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  StopAdvertising(base::DoNothing());
}

void FastPairAdvertiser::StartAdvertising(
    base::OnceCallback<void()> callback,
    base::OnceCallback<void()> error_callback) {
  DCHECK(adapter_->IsPresent() && adapter_->IsPowered());
  DCHECK(!advertisement_);
  RegisterAdvertisement(std::move(callback), std::move(error_callback));
}

void FastPairAdvertiser::StopAdvertising(base::OnceCallback<void()> callback) {
  if (!advertisement_) {
    std::move(callback).Run();
    // |this| might be destroyed here, do not access local fields.
    return;
  }

  UnregisterAdvertisement(std::move(callback));
}

void FastPairAdvertiser::RegisterAdvertisement(
    base::OnceClosure callback,
    base::OnceClosure error_callback) {
  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  auto list = std::make_unique<device::BluetoothAdvertisement::UUIDList>();
  list->push_back(kFastPairServiceUuid);
  advertisement_data->set_service_uuids(std::move(list));

  auto service_data =
      std::make_unique<device::BluetoothAdvertisement::ServiceData>();
  auto payload = std::vector<uint8_t>(std::begin(kFastPairModelId),
                                      std::end(kFastPairModelId));
  service_data->insert(std::pair<std::string, std::vector<uint8_t>>(
      kFastPairServiceUuid, payload));
  advertisement_data->set_service_data(std::move(service_data));

  auto manufacturer_data =
      std::make_unique<device::BluetoothAdvertisement::ManufacturerData>();
  auto manufacturer_metadata = GenerateManufacturerMetadata();
  manufacturer_data->insert(std::pair<uint16_t, std::vector<uint8_t>>(
      kCompanyId, manufacturer_metadata));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(&FastPairAdvertiser::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::BindOnce(&FastPairAdvertiser::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(error_callback)));
}

void FastPairAdvertiser::OnRegisterAdvertisement(
    base::OnceClosure callback,
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  advertisement_ = advertisement;
  advertisement_->AddObserver(this);
  base::UmaHistogramBoolean(kAdvertisingSuccessHistogramName, true);
  std::move(callback).Run();
}

void FastPairAdvertiser::OnRegisterAdvertisementError(
    base::OnceClosure error_callback,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  LOG(ERROR) << __func__ << " failed with error code = " << error_code;
  base::UmaHistogramBoolean(kAdvertisingSuccessHistogramName, false);
  std::move(error_callback).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastPairAdvertiser::UnregisterAdvertisement(base::OnceClosure callback) {
  stop_callback_ = std::move(callback);
  advertisement_->RemoveObserver(this);
  advertisement_->Unregister(
      base::BindOnce(&FastPairAdvertiser::OnUnregisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairAdvertiser::OnUnregisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairAdvertiser::OnUnregisterAdvertisement() {
  advertisement_.reset();
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastPairAdvertiser::OnUnregisterAdvertisementError(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  LOG(WARNING) << __func__ << " failed with error code = " << error_code;
  advertisement_.reset();
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

std::vector<uint8_t> FastPairAdvertiser::GenerateManufacturerMetadata() {
  // TODO(b/235403498): This code may need to be updated later to be derived
  // from the device BT address. It is not required in order for the
  // advertisement to trigger the Fast Pair halfsheet.
  auto token = base::UnguessableToken::Create();
  base::span<const uint8_t> fast_pair_code = token.AsBytes().first(2);

  std::vector<uint8_t> metadata(std::begin(fast_pair_code),
                                std::end(fast_pair_code));

  return metadata;
}