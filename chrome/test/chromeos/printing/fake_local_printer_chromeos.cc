// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"

#include "testing/gtest/include/gtest/gtest.h"

void FakeLocalPrinter::CancelPrintJob(const std::string& printer_id,
                                      unsigned int job_id,
                                      CancelPrintJobCallback callback) {
  FAIL();
}

void FakeLocalPrinter::GetPrintServersConfig(
    GetPrintServersConfigCallback callback) {
  FAIL();
}

void FakeLocalPrinter::ChoosePrintServers(
    const std::vector<std::string>& print_server_ids,
    ChoosePrintServersCallback callback) {
  FAIL();
}

void FakeLocalPrinter::AddPrintServerObserver(
    mojo::PendingRemote<crosapi::mojom::PrintServerObserver> remote,
    AddPrintServerObserverCallback callback) {
  FAIL();
}

void FakeLocalPrinter::GetPolicies(GetPoliciesCallback callback) {
  FAIL();
}


void FakeLocalPrinter::GetPrinterTypeDenyList(
    GetPrinterTypeDenyListCallback callback) {
  FAIL();
}

void FakeLocalPrinter::AddPrintJobObserver(
    mojo::PendingRemote<crosapi::mojom::PrintJobObserver> remote,
    crosapi::mojom::PrintJobSource source,
    AddPrintJobObserverCallback callback) {
  FAIL();
}
