// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "components/cronet/native/test/test_util.h"
#include "net/cert/mock_cert_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kUserAgent = "EngineTest/1";

class EngineTest : public ::testing::Test {
 public:
  EngineTest(const EngineTest&) = delete;
  EngineTest& operator=(const EngineTest&) = delete;

 protected:
  EngineTest() = default;
  ~EngineTest() override {}
};

TEST_F(EngineTest, StartCronetEngine) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EngineParams_user_agent_set(engine_params, kUserAgent);
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_Engine_Destroy(engine);
  Cronet_EngineParams_Destroy(engine_params);
}

TEST_F(EngineTest, CronetEngineDefaultUserAgent) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Version and DefaultUserAgent don't require engine start.
  std::string version = Cronet_Engine_GetVersionString(engine);
  std::string default_agent = Cronet_Engine_GetDefaultUserAgent(engine);
  EXPECT_NE(default_agent.find(version), std::string::npos);
  Cronet_Engine_Destroy(engine);
}

TEST_F(EngineTest, InitDifferentEngines) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr first_engine = Cronet_Engine_Create();
  Cronet_Engine_StartWithParams(first_engine, engine_params);
  Cronet_EnginePtr second_engine = Cronet_Engine_Create();
  Cronet_Engine_StartWithParams(second_engine, engine_params);
  Cronet_EnginePtr third_engine = Cronet_Engine_Create();
  Cronet_Engine_StartWithParams(third_engine, engine_params);
  Cronet_EngineParams_Destroy(engine_params);
  Cronet_Engine_Destroy(first_engine);
  Cronet_Engine_Destroy(second_engine);
  Cronet_Engine_Destroy(third_engine);
}

TEST_F(EngineTest, StartResults) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Disable runtime CHECK of the result, so it could be verified.
  Cronet_EngineParams_enable_check_result_set(engine_params, false);
  Cronet_EngineParams_http_cache_mode_set(
      engine_params, Cronet_EngineParams_HTTP_CACHE_MODE_DISK);
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_ARGUMENT_STORAGE_PATH_MUST_EXIST,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_EngineParams_storage_path_set(engine_params, "InvalidPath");
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_ARGUMENT_STORAGE_PATH_MUST_EXIST,
            Cronet_Engine_StartWithParams(engine, engine_params));
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  Cronet_EngineParams_storage_path_set(engine_params,
                                       temp_path.AsUTF8Unsafe().c_str());
  // Now the engine should start successfully.
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(engine, engine_params));
  // The second start should fail.
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_STATE_ENGINE_ALREADY_STARTED,
            Cronet_Engine_StartWithParams(engine, engine_params));
  // The second engine should fail because storage path is already used.
  Cronet_EnginePtr second_engine = Cronet_Engine_Create();
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_STATE_STORAGE_PATH_IN_USE,
            Cronet_Engine_StartWithParams(second_engine, engine_params));
  // Shutdown first engine to free storage path.
  EXPECT_EQ(Cronet_RESULT_SUCCESS, Cronet_Engine_Shutdown(engine));
  // Now the second engine should start.
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(second_engine, engine_params));
  Cronet_Engine_Destroy(second_engine);
  Cronet_Engine_Destroy(engine);
  Cronet_EngineParams_Destroy(engine_params);
}

TEST_F(EngineTest, InvalidPkpParams) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Disable runtime CHECK of the result, so it could be verified.
  Cronet_EngineParams_enable_check_result_set(engine_params, false);
  // Try adding invalid public key pins.
  Cronet_PublicKeyPinsPtr public_key_pins = Cronet_PublicKeyPins_Create();
  Cronet_EngineParams_public_key_pins_add(engine_params, public_key_pins);
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_HOSTNAME,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_EngineParams_public_key_pins_clear(engine_params);
  // Detect long host name.
  Cronet_PublicKeyPins_host_set(public_key_pins, std::string(256, 'a').c_str());
  Cronet_EngineParams_public_key_pins_add(engine_params, public_key_pins);
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HOSTNAME,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_EngineParams_public_key_pins_clear(engine_params);
  // Detect invalid host name.
  Cronet_PublicKeyPins_host_set(public_key_pins, "invalid:host/name");
  Cronet_EngineParams_public_key_pins_add(engine_params, public_key_pins);
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HOSTNAME,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_EngineParams_public_key_pins_clear(engine_params);
  // Set valid host name.
  Cronet_PublicKeyPins_host_set(public_key_pins, "valid.host.name");
  Cronet_EngineParams_public_key_pins_add(engine_params, public_key_pins);
  // Detect missing pins.
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_SHA256_PINS,
            Cronet_Engine_StartWithParams(engine, engine_params));
  // Detect invalid pin.
  Cronet_EngineParams_public_key_pins_clear(engine_params);
  Cronet_PublicKeyPins_pins_sha256_add(public_key_pins, "invalid_sha256");
  Cronet_EngineParams_public_key_pins_add(engine_params, public_key_pins);
  EXPECT_EQ(Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_PIN,
            Cronet_Engine_StartWithParams(engine, engine_params));
  // THe engine cannot start with these params, and have to be destroyed.
  Cronet_Engine_Destroy(engine);
  Cronet_EngineParams_Destroy(engine_params);
  Cronet_PublicKeyPins_Destroy(public_key_pins);
}

TEST_F(EngineTest, ValidPkpParams) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Disable runtime CHECK of the result, so it could be verified.
  Cronet_EngineParams_enable_check_result_set(engine_params, false);
  // Add valid public key pins.
  Cronet_PublicKeyPinsPtr public_key_pins = Cronet_PublicKeyPins_Create();
  Cronet_PublicKeyPins_host_set(public_key_pins, "valid.host.name");
  Cronet_PublicKeyPins_pins_sha256_add(
      public_key_pins, "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");
  Cronet_EngineParams_public_key_pins_add(engine_params, public_key_pins);
  // The engine should start successfully.
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_Engine_Destroy(engine);
  Cronet_EngineParams_Destroy(engine_params);
  Cronet_PublicKeyPins_Destroy(public_key_pins);
}

// Verify that Cronet_Engine_SetMockCertVerifierForTesting() doesn't crash or
// leak anything.
TEST_F(EngineTest, SetMockCertVerifierForTesting) {
  auto cert_verifier(std::make_unique<net::MockCertVerifier>());
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  Cronet_Engine_SetMockCertVerifierForTesting(engine, cert_verifier.release());
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_Engine_StartWithParams(engine, engine_params);
  Cronet_Engine_Destroy(engine);
  Cronet_EngineParams_Destroy(engine_params);
}

TEST_F(EngineTest, StartNetLogToFile) {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = base::MakeAbsoluteFilePath(temp_dir.GetPath());
  base::FilePath net_log_file =
      temp_path.Append(FILE_PATH_LITERAL("netlog.json"));

  Cronet_EnginePtr engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EngineParams_experimental_options_set(
      engine_params,
      "{ \"QUIC\" : {\"max_server_configs_stored_in_properties\" : 8} }");
  // Test that net log cannot start/stop before engine start.
  EXPECT_FALSE(Cronet_Engine_StartNetLogToFile(
      engine, net_log_file.AsUTF8Unsafe().c_str(), true));
  Cronet_Engine_StopNetLog(engine);

  // Start the engine.
  Cronet_Engine_StartWithParams(engine, engine_params);
  Cronet_EngineParams_Destroy(engine_params);

  // Test that normal start/stop net log works.
  EXPECT_TRUE(Cronet_Engine_StartNetLogToFile(
      engine, net_log_file.AsUTF8Unsafe().c_str(), true));
  Cronet_Engine_StopNetLog(engine);

  // Test that double start/stop net log works.
  EXPECT_TRUE(Cronet_Engine_StartNetLogToFile(
      engine, net_log_file.AsUTF8Unsafe().c_str(), true));
  // Test that second start fails.
  EXPECT_FALSE(Cronet_Engine_StartNetLogToFile(
      engine, net_log_file.AsUTF8Unsafe().c_str(), true));
  // Test that multiple stops work.
  Cronet_Engine_StopNetLog(engine);
  Cronet_Engine_StopNetLog(engine);
  Cronet_Engine_StopNetLog(engine);

  // Test that net log contains effective experimental options.
  std::string net_log;
  EXPECT_TRUE(base::ReadFileToString(net_log_file, &net_log));
  EXPECT_TRUE(
      net_log.find(
          "{\"QUIC\":{\"max_server_configs_stored_in_properties\":8}") !=
      std::string::npos);

  // Test that bad file name fails.
  EXPECT_FALSE(Cronet_Engine_StartNetLogToFile(engine, "bad/file/name", true));

  Cronet_Engine_Shutdown(engine);
  // Test that net log cannot start/stop after engine shutdown.
  EXPECT_FALSE(Cronet_Engine_StartNetLogToFile(
      engine, net_log_file.AsUTF8Unsafe().c_str(), true));
  Cronet_Engine_StopNetLog(engine);
  Cronet_Engine_Destroy(engine);
}

}  // namespace
