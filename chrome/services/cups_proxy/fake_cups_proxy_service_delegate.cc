// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"

namespace cups_proxy {

bool FakeCupsProxyServiceDelegate::IsPrinterAccessAllowed() const {
  return true;
}

std::vector<chromeos::Printer> FakeCupsProxyServiceDelegate::GetPrinters(
    chromeos::PrinterClass printer_class) {
  return {};
}

base::Optional<chromeos::Printer> FakeCupsProxyServiceDelegate::GetPrinter(
    const std::string& id) {
  return base::nullopt;
}

std::vector<std::string>
FakeCupsProxyServiceDelegate::GetRecentlyUsedPrinters() {
  return {};
}

bool FakeCupsProxyServiceDelegate::IsPrinterInstalled(
    const chromeos::Printer& printer) {
  return false;
}

void FakeCupsProxyServiceDelegate::PrinterInstalled(
    const chromeos::Printer& printer) {}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCupsProxyServiceDelegate::GetIOTaskRunner() {
  return nullptr;
}

void FakeCupsProxyServiceDelegate::SetupPrinter(
    const chromeos::Printer& printer,
    SetupPrinterCallback cb) {}

}  // namespace cups_proxy
