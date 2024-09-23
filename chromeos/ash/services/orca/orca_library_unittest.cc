// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/orca/orca_library.h"

#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::orca {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::testing::Field;

base::FilePath GetTestSharedLibraryPath(std::string_view library_name) {
  return base::PathService::CheckedGet(base::DIR_OUT_TEST_DATA_ROOT)
      .Append(base::GetNativeLibraryName(library_name));
}

TEST(OrcaLibrary, BindMissingSharedLibraryFails) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::OrcaService> remote;

  OrcaLibrary library(GetTestSharedLibraryPath("non_existent"));

  EXPECT_THAT(library.BindReceiver(remote.BindNewPipeAndPassReceiver()),
              ErrorIs(Field(&OrcaLibrary::BindError::code,
                            OrcaLibrary::BindErrorCode::kLoadFailed)));
}

TEST(OrcaLibrary, BindSharedLibraryWithWrongExportsFails) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::OrcaService> remote;

  OrcaLibrary library(
      GetTestSharedLibraryPath("test_orca_shared_library_wrong_exports"));

  EXPECT_THAT(
      library.BindReceiver(remote.BindNewPipeAndPassReceiver()),
      ErrorIs(Field(&OrcaLibrary::BindError::code,
                    OrcaLibrary::BindErrorCode::kGetFunctionPointerFailed)));
}

TEST(OrcaLibrary, BindSharedLibraryWithBindErrorFails) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::OrcaService> remote;

  OrcaLibrary library(
      GetTestSharedLibraryPath("test_orca_shared_library_bad_bind"));

  EXPECT_THAT(library.BindReceiver(remote.BindNewPipeAndPassReceiver()),
              ErrorIs(Field(&OrcaLibrary::BindError::code,
                            OrcaLibrary::BindErrorCode::kBindFailed)));
}

TEST(OrcaLibrary, BindSharedLibrarySucceeds) {
  base::test::TaskEnvironment task_environment;

  OrcaLibrary library(
      GetTestSharedLibraryPath("test_orca_shared_library_good"));
  mojo::Remote<mojom::OrcaService> remote;
  EXPECT_THAT(library.BindReceiver(remote.BindNewPipeAndPassReceiver()),
              HasValue());
}

TEST(OrcaLibrary, BindSharedLibrarySetsUpMojo) {
  base::test::TaskEnvironment task_environment;
  mojo::PendingAssociatedReceiver<mojom::SystemActuator> system_actuator;
  mojo::PendingAssociatedReceiver<mojom::TextQueryProvider> text_query_provider;
  mojo::PendingAssociatedRemote<mojom::EditorClientConnector> client_connector;
  mojo::PendingAssociatedRemote<mojom::EditorEventSink> event_sink;

  std::string log_result;
  OrcaLibrary library(
      GetTestSharedLibraryPath("test_orca_shared_library_good"),
      base::BindRepeating(
          [](std::string& log_result, logging::LogSeverity severity,
             std::string_view message) { log_result = message; },
          std::ref(log_result)));
  mojo::Remote<mojom::OrcaService> remote;
  (void)library.BindReceiver(remote.BindNewPipeAndPassReceiver());
  remote->BindEditor(system_actuator.InitWithNewEndpointAndPassRemote(),
                     text_query_provider.InitWithNewEndpointAndPassRemote(),
                     client_connector.InitWithNewEndpointAndPassReceiver(),
                     event_sink.InitWithNewEndpointAndPassReceiver(),
                     mojom::EditorConfig::New());
  remote.FlushForTesting();

  EXPECT_EQ(log_result, "Success");
}

}  // namespace
}  // namespace ash::orca
