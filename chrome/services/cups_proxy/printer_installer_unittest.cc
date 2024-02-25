// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>

#include <map>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/printer_installer.h"
#include "printing/backend/cups_ipp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

using Printer = chromeos::Printer;

// Generated via base::Uuid::GenerateRandomV4().AsLowercaseString().
const char kGenericGUID[] = "fd4c5f2e-7549-43d5-b931-9bf4e4f1bf51";

// Faked delegate gives control over PrinterInstaller's printing stack
// dependencies.
class FakeServiceDelegate : public FakeCupsProxyServiceDelegate {
 public:
  FakeServiceDelegate() = default;
  ~FakeServiceDelegate() override = default;

  void AddPrinter(const Printer& printer) {
    installed_printers_.insert({printer.id(), false});
  }

  void FailSetupPrinter() { fail_printer_setup_ = true; }

  // Service delegate overrides.
  bool IsPrinterInstalled(const Printer& printer) override {
    if (!base::Contains(installed_printers_, printer.id())) {
      return false;
    }

    return installed_printers_.at(printer.id());
  }

  std::optional<Printer> GetPrinter(const std::string& id) override {
    if (!base::Contains(installed_printers_, id)) {
      return std::nullopt;
    }

    return Printer(id);
  }

  void SetupPrinter(const Printer& printer,
                    SetupPrinterCallback callback) override {
    if (fail_printer_setup_) {
      return std::move(callback).Run(false);
    }

    // PrinterInstaller is expected to have checked if |printer| is already
    // installed before trying setup.
    if (IsPrinterInstalled(printer)) {
      return std::move(callback).Run(false);
    }

    // Install printer.
    installed_printers_[printer.id()] = true;
    return std::move(callback).Run(true);
  }

 private:
  std::map<std::string, bool> installed_printers_;

  // Conditions whether calls to SetupPrinter succeed.
  bool fail_printer_setup_ = false;
};

class PrinterInstallerTest : public testing::Test {
 public:
  PrinterInstallerTest() : weak_factory_(this) {
    delegate_ = std::make_unique<FakeServiceDelegate>();
    printer_installer_ = std::make_unique<PrinterInstaller>(delegate_.get());
  }

  ~PrinterInstallerTest() override = default;

  InstallPrinterResult RunInstallPrinter(std::string printer_id) {
    InstallPrinterResult ret;

    base::RunLoop run_loop;
    printer_installer_->InstallPrinter(
        printer_id, base::BindOnce(&PrinterInstallerTest::OnRunInstallPrinter,
                                   weak_factory_.GetWeakPtr(),
                                   run_loop.QuitClosure(), &ret));

    run_loop.Run();
    return ret;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  void OnRunInstallPrinter(base::OnceClosure finish_cb,
                           InstallPrinterResult* ret,
                           InstallPrinterResult result) {
    *ret = result;
    std::move(finish_cb).Run();
  }

  // Backend fake driving the PrinterInstaller.
  std::unique_ptr<FakeServiceDelegate> delegate_;

  // The class being tested. This must be declared after the fakes, as its
  // initialization must come after that of the fakes.
  std::unique_ptr<PrinterInstaller> printer_installer_;

  base::WeakPtrFactory<PrinterInstallerTest> weak_factory_;
};

// Standard install known printer workflow.
TEST_F(PrinterInstallerTest, SimpleSanityTest) {
  Printer to_install(kGenericGUID);
  delegate_->AddPrinter(to_install);

  auto ret = RunInstallPrinter(kGenericGUID);
  EXPECT_EQ(ret, InstallPrinterResult::kSuccess);
  EXPECT_TRUE(delegate_->IsPrinterInstalled(to_install));
}

// Should fail to install an unknown(previously unseen) printer.
TEST_F(PrinterInstallerTest, UnknownPrinter) {
  Printer to_install(kGenericGUID);

  auto ret = RunInstallPrinter(kGenericGUID);
  EXPECT_EQ(ret, InstallPrinterResult::kUnknownPrinterFound);
  EXPECT_FALSE(delegate_->IsPrinterInstalled(to_install));
}

// Ensure we never setup a printer that's already installed.
TEST_F(PrinterInstallerTest, InstallPrinterTwice) {
  Printer to_install(kGenericGUID);
  delegate_->AddPrinter(to_install);

  auto ret = RunInstallPrinter(kGenericGUID);
  EXPECT_EQ(ret, InstallPrinterResult::kSuccess);

  // |printer_installer_| should notice printer is already installed and bail
  // out. If it attempts setup, FakeServiceDelegate will fail the request.
  ret = RunInstallPrinter(kGenericGUID);
  EXPECT_EQ(ret, InstallPrinterResult::kSuccess);
}

// Checks for correct response to failed SetupPrinter call.
TEST_F(PrinterInstallerTest, SetupPrinterFailure) {
  Printer to_install(kGenericGUID);
  delegate_->AddPrinter(to_install);
  delegate_->FailSetupPrinter();

  auto ret = RunInstallPrinter(kGenericGUID);
  EXPECT_EQ(ret, InstallPrinterResult::kPrinterInstallationFailure);
  EXPECT_FALSE(delegate_->IsPrinterInstalled(to_install));
}

}  // namespace
}  // namespace cups_proxy
