// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_DELEGATE_H_
#define CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/printing/printer_configuration.h"

#include "base/task/post_task.h"

namespace cups_proxy {

using SetupPrinterCallback = base::OnceCallback<void(bool)>;

// This delegate grants the CupsProxyService access to the Chrome printing
// stack. This class can be created anywhere but must be accessed from a
// sequenced context.
class CupsProxyServiceDelegate {
 public:
  CupsProxyServiceDelegate();
  virtual ~CupsProxyServiceDelegate();

  // Exposing |weak_factory_|.GetWeakPtr method. Needed to share delegate with
  // CupsProxyService internal managers.
  base::WeakPtr<CupsProxyServiceDelegate> GetWeakPtr();

  virtual std::vector<chromeos::Printer> GetPrinters(
      chromeos::PrinterClass printer_class) = 0;
  virtual base::Optional<chromeos::Printer> GetPrinter(
      const std::string& id) = 0;
  virtual bool IsPrinterInstalled(const chromeos::Printer& printer) = 0;
  virtual void PrinterInstalled(const chromeos::Printer& printer) = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() = 0;

  // |cb| will be run on this delegate's sequenced context.
  virtual void SetupPrinter(const chromeos::Printer& printer,
                            SetupPrinterCallback cb) = 0;

 private:
  base::WeakPtrFactory<CupsProxyServiceDelegate> weak_factory_{this};
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_DELEGATE_H_
