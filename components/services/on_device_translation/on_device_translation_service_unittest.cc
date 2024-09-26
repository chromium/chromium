// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/on_device_translation_service.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

const char kTestString[] = "test";
const char kMockLibraryName[] = "mock_translate_kit_lib";

class MockOnDeviceTranslationServiceTest : public testing::Test {
 public:
  MockOnDeviceTranslationServiceTest()
      : service_impl_(service_remote_.BindNewPipeAndPassReceiver()),
        weak_factory_(this) {
    base::FilePath exe_path;
    CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
    base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
        kTranslateKitBinaryPath,
        exe_path.AppendASCII(base::GetNativeLibraryName(kMockLibraryName)));
  }

  mojo::PendingReceiver<mojom::Translator>
  BindNewPipeAndPassTranslatorReceiver() {
    return translator_remote_.BindNewPipeAndPassReceiver();
  }

  // Test if the `CanTranslate()` result matches `should_succeed`.
  void TestCanTranslate(const std::string& source_lang,
                        const std::string& target_lang,
                        bool should_succeed) {
    base::RunLoop run_loop;
    service_remote_->CanTranslate(source_lang, target_lang,
                                  base::BindOnce(
                                      [](base::RepeatingClosure closure,
                                         bool should_succeed, bool result) {
                                        EXPECT_EQ(result, should_succeed);
                                        closure.Run();
                                      },
                                      run_loop.QuitClosure(), should_succeed));
    run_loop.Run();
  }

  // Test if the `CreateTranslator()` result matches `should_succeed`. If a
  // translator can be created, it also tests if the `Translate()` method works
  // as expected.
  void TestCreateTranslator(const std::string& source_lang,
                            const std::string& target_lang,
                            bool should_succeed) {
    base::RunLoop run_loop;
    service_remote_->CreateTranslator(
        source_lang, target_lang, BindNewPipeAndPassTranslatorReceiver(),
        base::BindOnce(&MockOnDeviceTranslationServiceTest::OnTranslatorCreated,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure(),
                       should_succeed));
    run_loop.Run();
  }

 protected:
  void OnTranslatorCreated(base::RepeatingClosure closure,
                           bool should_succeed,
                           bool result) {
    if (!should_succeed) {
      EXPECT_FALSE(result);
      closure.Run();
      return;
    }
    ASSERT_TRUE(result);
    translator_remote_->Translate(
        kTestString,
        base::BindOnce(
            [](base::RepeatingClosure closure, const std::string& output) {
              EXPECT_EQ(output, kTestString);
              closure.Run();
            },
            closure));
  }

  base::test::TaskEnvironment task_environment_;

 private:
  mojo::Remote<mojom::OnDeviceTranslationService> service_remote_;
  mojo::Remote<mojom::Translator> translator_remote_;
  OnDeviceTranslationService service_impl_;

  base::WeakPtrFactory<MockOnDeviceTranslationServiceTest> weak_factory_;
};

TEST_F(MockOnDeviceTranslationServiceTest, CanTranslate_SameLanguage) {
  TestCanTranslate("en", "en", true);
}

TEST_F(MockOnDeviceTranslationServiceTest, CanTranslate_DifferentLanguage) {
  TestCanTranslate("en", "ja", false);
}

TEST_F(MockOnDeviceTranslationServiceTest, CreateTranslator_SameLanguage) {
  TestCreateTranslator("en", "en", true);
}

TEST_F(MockOnDeviceTranslationServiceTest, CreateTranslator_DifferentLanguage) {
  TestCreateTranslator("en", "ja", false);
}

}  // namespace
}  // namespace on_device_translation
