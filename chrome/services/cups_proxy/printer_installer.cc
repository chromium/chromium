// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/printer_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/services/cups_proxy/public/cpp/cups_util.h"
#include "chromeos/printing/printer_configuration.h"

namespace cups_proxy {

PrinterInstaller::PrinterInstaller(CupsProxyServiceDelegate* const delegate)
    : delegate_(delegate) {}

PrinterInstaller::~PrinterInstaller() = default;

void PrinterInstaller::InstallPrinter(std::string printer_id,
                                      InstallPrinterCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto printer = delegate_->GetPrinter(printer_id);
  if (!printer) {
    // If the requested printer DNE, we proxy to CUPSd and allow it to
    // handle the error.
    Finish(std::move(cb), InstallPrinterResult::kUnknownPrinterFound);
    return;
  }

  if (delegate_->IsPrinterInstalled(*printer)) {
    Finish(std::move(cb), InstallPrinterResult::kSuccess);
    return;
  }

  // Install printer.
  delegate_->SetupPrinter(
      *printer,
      base::BindOnce(&PrinterInstaller::OnInstallPrinter,
                     weak_factory_.GetWeakPtr(), std::move(cb), *printer));
}

// TODO(crbug.com/945409): Test whether we need to call
// CupsPrintersManager::PrinterInstalled here.
void PrinterInstaller::OnInstallPrinter(InstallPrinterCallback cb,
                                        const chromeos::Printer& printer,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (success) {
    delegate_->PrinterInstalled(printer);
  }

  Finish(std::move(cb),
         success ? InstallPrinterResult::kSuccess
                 : InstallPrinterResult::kPrinterInstallationFailure);
}

void PrinterInstaller::Finish(InstallPrinterCallback cb,
                              InstallPrinterResult res) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), res));
}

}  // namespace cups_proxy
