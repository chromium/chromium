// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_FAKE_CUPS_PROXY_SERVICE_DELEGATE_H_
#define CHROME_SERVICES_CUPS_PROXY_FAKE_CUPS_PROXY_SERVICE_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"
#include "chromeos/printing/printer_configuration.h"

namespace cups_proxy {

// Fake implementation for use in unit_tests.
class FakeCupsProxyServiceDelegate : public CupsProxyServiceDelegate {
 public:
  FakeCupsProxyServiceDelegate() = default;
  ~FakeCupsProxyServiceDelegate() override = default;

  // CupsProxyServiceDelegate overrides.
  bool IsPrinterAccessAllowed() const override;
  std::vector<chromeos::Printer> GetPrinters(
      chromeos::PrinterClass printer_class) override;
  std::optional<chromeos::Printer> GetPrinter(const std::string& id) override;
  std::vector<std::string> GetRecentlyUsedPrinters() override;
  bool IsPrinterInstalled(const chromeos::Printer& printer) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;
  void SetupPrinter(const chromeos::Printer& printer,
                    SetupPrinterCallback cb) override;
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_FAKE_CUPS_PROXY_SERVICE_DELEGATE_H_
