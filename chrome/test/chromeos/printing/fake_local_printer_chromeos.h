// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEOS_PRINTING_FAKE_LOCAL_PRINTER_CHROMEOS_H_
#define CHROME_TEST_CHROMEOS_PRINTING_FAKE_LOCAL_PRINTER_CHROMEOS_H_

#include <string>
#include <vector>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

// FakeLocalPrinter is an implementation of the LocalPrinter interface where
// all methods FAIL(). Methods can be overridden for testing.
class FakeLocalPrinter : public crosapi::mojom::LocalPrinter {
 public:
  void CancelPrintJob(const std::string& printer_id,
                      unsigned int job_id,
                      CancelPrintJobCallback callback) override;
  void GetPrintServersConfig(GetPrintServersConfigCallback callback) override;
  void ChoosePrintServers(const std::vector<std::string>& print_server_ids,
                          ChoosePrintServersCallback callback) override;
  void AddPrintServerObserver(
      mojo::PendingRemote<crosapi::mojom::PrintServerObserver> remote,
      AddPrintServerObserverCallback callback) override;
  void GetPolicies(GetPoliciesCallback callback) override;
  void GetPrinterTypeDenyList(GetPrinterTypeDenyListCallback callback) override;
  void AddPrintJobObserver(
      mojo::PendingRemote<crosapi::mojom::PrintJobObserver> remote,
      crosapi::mojom::PrintJobSource source,
      AddPrintJobObserverCallback callback) override;
};

#endif  // CHROME_TEST_CHROMEOS_PRINTING_FAKE_LOCAL_PRINTER_CHROMEOS_H_
