// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/handwriting_model_loader.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace machine_learning {

using ::base::test::ScopedCommandLine;
using ::base::test::TaskEnvironment;
using mojom::LoadHandwritingModelResult;

constexpr char kOndeviceHandwritingSwitch[] = "ondevice_handwriting";
constexpr char kLibHandwritingDlcId[] = "libhandwriting";

class HandwritingModelLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    result_ = LoadHandwritingModelResult::DEPRECATED_MODEL_SPEC_ERROR;
    language_ = "en";
  }

  // Callback that called when loader_->Load() is over to save the returned
  // result.
  void OnHandwritingModelLoaderComplete(
      const LoadHandwritingModelResult result) {
    result_ = result;
  }

  // Runs loader_->Load() and check the returned result as expected.
  void ExpectLoadHandwritingModelResult(
      const LoadHandwritingModelResult expected_result) {
    LoadHandwritingModelFromRootfsOrDlc(
        mojom::HandwritingRecognizerSpec::New(language_),
        recognizer_.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            &HandwritingModelLoaderTest::OnHandwritingModelLoaderComplete,
            base::Unretained(this)),
        &fake_client_);

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(result_, expected_result);
  }

  void SetLanguage(const std::string& language) { language_ = language; }

  // Creates a dlc list with one dlc inside.
  void AddDlcsWithContent(const std::string& dlc_id) {
    dlcservice::DlcsWithContent dlcs_with_content;
    dlcs_with_content.add_dlc_infos()->set_id(dlc_id);
    fake_client_.set_dlcs_with_content(dlcs_with_content);
  }

  // Sets InstallDlc error.
  void SetInstallError(const std::string& error) {
    fake_client_.set_install_error(error);
    fake_client_.set_install_root_path("/any-path");
  }

  // Sets "ondevice_handwriting" value.
  void SetSwitchValue(const std::string& switch_value) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kOndeviceHandwritingSwitch, switch_value);
  }

 private:
  TaskEnvironment task_environment_{
      TaskEnvironment::MainThreadType::DEFAULT,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  ScopedCommandLine scoped_command_line_;
  FakeDlcserviceClient fake_client_;
  FakeServiceConnectionImpl fake_service_connection_;
  LoadHandwritingModelResult result_;
  std::string language_;
  mojo::Remote<mojom::HandwritingRecognizer> recognizer_;
};

TEST_F(HandwritingModelLoaderTest, HandwritingNotEnabled) {
  SetSwitchValue("random_string");

  // Random switch value should return FEATURE_NOT_SUPPORTED_ERROR.
  ExpectLoadHandwritingModelResult(
      LoadHandwritingModelResult::FEATURE_NOT_SUPPORTED_ERROR);
}

TEST_F(HandwritingModelLoaderTest, LoadingWithInvalidLanguage) {
  SetSwitchValue("use_rootfs");

  SetLanguage("random string as language");

  // Random language code should return LANGUAGE_NOT_SUPPORTED_ERROR.
  ExpectLoadHandwritingModelResult(
      LoadHandwritingModelResult::LANGUAGE_NOT_SUPPORTED_ERROR);
}

TEST_F(HandwritingModelLoaderTest, LoadingWithUseRootfs) {
  SetSwitchValue("use_rootfs");

  // Load from rootfs should return success.
  ExpectLoadHandwritingModelResult(LoadHandwritingModelResult::OK);
}

TEST_F(HandwritingModelLoaderTest, LoadingWithoutDlcOnDevice) {
  SetSwitchValue("use_dlc");

  AddDlcsWithContent("random dlc-id");

  // Random dlc id should return DLC_DOES_NOT_EXIST.
  ExpectLoadHandwritingModelResult(
      LoadHandwritingModelResult::DLC_DOES_NOT_EXIST);
}

TEST_F(HandwritingModelLoaderTest, DlcInstalledWithError) {
  SetSwitchValue("use_dlc");

  AddDlcsWithContent(kLibHandwritingDlcId);
  SetInstallError("random error");

  // InstallDlc error should return DLC_INSTALL_ERROR.
  ExpectLoadHandwritingModelResult(
      LoadHandwritingModelResult::DLC_INSTALL_ERROR);
}

TEST_F(HandwritingModelLoaderTest, DlcInstalledWithoutError) {
  SetSwitchValue("use_dlc");

  AddDlcsWithContent(kLibHandwritingDlcId);
  SetInstallError(dlcservice::kErrorNone);

  // InstallDlc without an error should return success.
  ExpectLoadHandwritingModelResult(LoadHandwritingModelResult::OK);
}

}  // namespace machine_learning
}  // namespace chromeos
