// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/scanning/scanner.h"

namespace chromeos {

ScannerDeviceName::ScannerDeviceName(const std::string& device_name)
    : device_name(device_name) {}

ScannerDeviceName::ScannerDeviceName(const std::string& device_name,
                                     bool usable)
    : device_name(device_name), usable(usable) {}

ScannerDeviceName::~ScannerDeviceName() = default;

ScannerDeviceName::ScannerDeviceName(const ScannerDeviceName& other) = default;

ScannerDeviceName& ScannerDeviceName::operator=(
    const ScannerDeviceName& other) = default;

bool ScannerDeviceName::operator<(const ScannerDeviceName& other) const {
  return device_name < other.device_name;
}

bool ScannerDeviceName::operator==(const ScannerDeviceName& other) const {
  return device_name == other.device_name;
}

Scanner::Scanner() = default;

Scanner::~Scanner() = default;

Scanner::Scanner(const Scanner& other) = default;

Scanner& Scanner::operator=(const Scanner& other) = default;

}  // namespace chromeos
