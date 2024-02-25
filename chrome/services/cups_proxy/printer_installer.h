// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PRINTER_INSTALLER_H_
#define CHROME_SERVICES_CUPS_PROXY_PRINTER_INSTALLER_H_

#include <cups/cups.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"

namespace cups_proxy {

enum class InstallPrinterResult {
  kSuccess = 0,

  // Referenced printer is unknown to Chrome.
  kUnknownPrinterFound,
  kPrinterInstallationFailure,
};

using InstallPrinterCallback = base::OnceCallback<void(InstallPrinterResult)>;

// This CupsProxyService internal manager ensures that any printers referenced
// by an incoming IPP request are installed into the CUPS daemon prior to
// proxying. This class must be created and accessed from a sequenced context.
class PrinterInstaller {
 public:
  explicit PrinterInstaller(CupsProxyServiceDelegate* const delegate);
  ~PrinterInstaller();

  // Pre-installs any printers required by |ipp| into the CUPS daemon, as
  // needed. |cb| will be run on this instance's sequenced context.
  void InstallPrinter(std::string printer_id, InstallPrinterCallback cb);

 private:
  void OnInstallPrinter(InstallPrinterCallback cb,
                        const chromeos::Printer& printer,
                        bool success);
  void Finish(InstallPrinterCallback cb, InstallPrinterResult res);

  // Unowned delegate granting access to printing stack dependencies.
  const raw_ptr<CupsProxyServiceDelegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PrinterInstaller> weak_factory_{this};
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_PRINTER_INSTALLER_H_
