// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEOS_PRINTING_MOCK_LOCAL_PRINTER_CHROMEOS_H_
#define CHROME_TEST_CHROMEOS_PRINTING_MOCK_LOCAL_PRINTER_CHROMEOS_H_

#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "testing/gmock/include/gmock/gmock.h"

// MockLocalPrinter is a subclass of FakeLocalPrinter with selected methods
// mocked out.
class MockLocalPrinter : public FakeLocalPrinter {
 public:
  MockLocalPrinter();
  ~MockLocalPrinter() override;

  MOCK_METHOD(void, GetPrinters, (GetPrintersCallback callback), (override));
  MOCK_METHOD(void,
              GetCapability,
              (const std::string& printer_id, GetCapabilityCallback callback),
              (override));
  MOCK_METHOD(void,
              AddPrintJobObserver,
              (mojo::PendingRemote<crosapi::mojom::PrintJobObserver> remote,
               crosapi::mojom::PrintJobSource source,
               AddPrintJobObserverCallback callback),
              (override));
  MOCK_METHOD(void,
              AddPrintServerObserver,
              (mojo::PendingRemote<crosapi::mojom::PrintServerObserver> remote,
               AddPrintServerObserverCallback callback),
              (override));
  MOCK_METHOD(void,
              CreatePrintJob,
              (crosapi::mojom::PrintJobPtr job,
               CreatePrintJobCallback callback),
              (override));
  MOCK_METHOD(void,
              CancelPrintJob,
              (const std::string& printer_id,
               uint32_t job_id,
               CancelPrintJobCallback callback),
              (override));
  MOCK_METHOD(void, GetPolicies, (GetPoliciesCallback callback), (override));
  MOCK_METHOD(void,
              GetEulaUrl,
              (const std::string& destination_id, GetEulaUrlCallback callback),
              (override));
};

#endif  // CHROME_TEST_CHROMEOS_PRINTING_MOCK_LOCAL_PRINTER_CHROMEOS_H_
