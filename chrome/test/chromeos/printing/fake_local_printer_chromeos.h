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
  void GetPrinters(GetPrintersCallback callback) override;
  void GetCapability(const std::string& printer_id,
                     GetCapabilityCallback callback) override;
  void GetEulaUrl(const std::string& printer_id,
                  GetEulaUrlCallback callback) override;
  void GetStatus(const std::string& printer_id,
                 GetStatusCallback callback) override;
  void ShowSystemPrintSettings(
      ShowSystemPrintSettingsCallback callback) override;
  void CreatePrintJob(crosapi::mojom::PrintJobPtr job,
                      CreatePrintJobCallback callback) override;
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
  void GetUsernamePerPolicy(GetUsernamePerPolicyCallback callback) override;
  void GetPrinterTypeDenyList(GetPrinterTypeDenyListCallback callback) override;
  void AddPrintJobObserver(
      mojo::PendingRemote<crosapi::mojom::PrintJobObserver> remote,
      crosapi::mojom::PrintJobSource source,
      AddPrintJobObserverCallback callback) override;
  void AddLocalPrintersObserver(
      mojo::PendingRemote<crosapi::mojom::LocalPrintersObserver> remote,
      AddLocalPrintersObserverCallback callback) override;
  void GetOAuthAccessToken(const std::string& printer_id,
                           GetOAuthAccessTokenCallback callback) override;
  void GetIppClientInfo(const std::string& printer_id,
                        GetIppClientInfoCallback callback) override;
};

#endif  // CHROME_TEST_CHROMEOS_PRINTING_FAKE_LOCAL_PRINTER_CHROMEOS_H_
