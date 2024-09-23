// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system_info/cpu_usage_data.h"

namespace system_info {

CpuUsageData::CpuUsageData(uint64_t user_time,
                           uint64_t system_time,
                           uint64_t idle_time)
    : user_time_(user_time), system_time_(system_time), idle_time_(idle_time) {}

bool CpuUsageData::IsInitialized() const {
  return user_time_ != std::numeric_limits<uint64_t>::max() &&
         system_time_ != std::numeric_limits<uint64_t>::max() &&
         idle_time_ != std::numeric_limits<uint64_t>::max();
}

uint64_t CpuUsageData::GetTotalTime() const {
  return user_time_ + system_time_ + idle_time_;
}

CpuUsageData CpuUsageData::operator+(const CpuUsageData& other) const {
  return CpuUsageData(user_time_ + other.user_time_,
                      system_time_ + other.system_time_,
                      idle_time_ + other.idle_time_);
}

CpuUsageData& CpuUsageData::operator+=(const CpuUsageData& other) {
  user_time_ += other.user_time_;
  system_time_ += other.system_time_;
  idle_time_ += other.idle_time_;
  return *this;
}

CpuUsageData CpuUsageData::operator-(const CpuUsageData& other) const {
  return CpuUsageData(user_time_ - other.user_time_,
                      system_time_ - other.system_time_,
                      idle_time_ - other.idle_time_);
}

CpuUsageData& CpuUsageData::operator-=(const CpuUsageData& other) {
  user_time_ -= other.user_time_;
  system_time_ -= other.system_time_;
  idle_time_ -= other.idle_time_;
  return *this;
}

}  // namespace system_info
