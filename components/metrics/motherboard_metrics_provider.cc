// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/motherboard_metrics_provider.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "components/metrics/motherboard.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {
// Constructs the Motherboard data on the blocking pool.
std::unique_ptr<Motherboard> GetMotherboard() {
  return std::make_unique<Motherboard>();
}
}  // namespace

MotherboardMetricsProvider::MotherboardMetricsProvider() = default;
MotherboardMetricsProvider::~MotherboardMetricsProvider() = default;

void MotherboardMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  // If motherboard_info_ has not yet been initialized, skip metrics.
  if (!motherboard_info_) {
    return;
  }
  SystemProfileProto::Hardware::Motherboard* motherboard =
      system_profile->mutable_hardware()->mutable_motherboard();
  if (motherboard_info_->manufacturer().has_value()) {
    motherboard->set_manufacturer(*motherboard_info_->manufacturer());
  }
  if (motherboard_info_->model().has_value()) {
    motherboard->set_model(*motherboard_info_->model());
  }
  if (motherboard_info_->bios_manufacturer().has_value()) {
    motherboard->set_bios_manufacturer(*motherboard_info_->bios_manufacturer());
  }
  if (motherboard_info_->bios_version().has_value()) {
    motherboard->set_bios_version(*motherboard_info_->bios_version());
  }
  if (motherboard_info_->bios_type().has_value()) {
    if (*motherboard_info_->bios_type() == Motherboard::BiosType::kLegacy) {
      motherboard->set_bios_type(SystemProfileProto::Hardware::BIOS_TYPE_LEGACY);
    } else if (*motherboard_info_->bios_type() ==
               Motherboard::BiosType::kUefi) {
      motherboard->set_bios_type(SystemProfileProto::Hardware::BIOS_TYPE_UEFI);
    }
  } else {
    motherboard->set_bios_type(SystemProfileProto::Hardware::BIOS_TYPE_UNKNOWN);
  }
}

void MotherboardMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetMotherboard),
      base::BindOnce(&MotherboardMetricsProvider::InitializeMotherboard,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback)));
}

void MotherboardMetricsProvider::InitializeMotherboard(
    base::OnceClosure done_callback,
    std::unique_ptr<Motherboard> motherboard_info) {
  motherboard_info_ = std::move(motherboard_info);
  std::move(done_callback).Run();
}

}  // namespace metrics
