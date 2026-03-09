// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/translation_dispatcher_on_device.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/on_device_translation/service_controller.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "components/on_device_translation/test/fake_translator.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captions {
namespace {

using ::on_device_translation::FakeOnDeviceTranslationInstaller;
using ::on_device_translation::OnDeviceTranslationController;
using ::testing::_;

class MockOnDeviceTranslationServiceController
    : public OnDeviceTranslationController {
 public:
  ~MockOnDeviceTranslationServiceController() override = default;
  MOCK_METHOD(void,
              CreateTranslator,
              (const std::string& source_lang,
               const std::string& target_lang,
               CreateTranslatorCallback callback),
              (override));
  MOCK_METHOD(void,
              CanTranslate,
              (const std::string& source_lang,
               const std::string& target_lang,
               CanTranslateCallback callback),
              (override));
  bool IsServiceRunning() const override { return true; }
};

class TranslationDispatcherOnDeviceTest : public testing::Test {
 public:
  void SetUp() override {
    fake_installer_ =
        std::make_unique<FakeOnDeviceTranslationInstaller>(base::FilePath());
  }

  std::unique_ptr<TranslationDispatcherOnDevice> CreateTranslationDispatcher(
      std::unique_ptr<OnDeviceTranslationController> translation_controller) {
    return std::make_unique<TranslationDispatcherOnDevice>(
        std::move(translation_controller));
  }

  void OnTranslated(const TranslateEvent& result) {
    if (result.has_value()) {
      translated_text_ = result.value();
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::string translated_text_;
  std::unique_ptr<on_device_translation::FakeTranslator> fake_translator_;
  std::unique_ptr<FakeOnDeviceTranslationInstaller> fake_installer_;

  TranslationDispatcherOnDeviceTest() = default;
};

TEST_F(TranslationDispatcherOnDeviceTest, GetTranslationSuccess) {
  base::RunLoop run_loop;
  auto mock_service_controller =
      std::make_unique<MockOnDeviceTranslationServiceController>();
  EXPECT_CALL(*mock_service_controller, CanTranslate("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             base::OnceCallback<void(
                 OnDeviceTranslationController::CanTranslateResult)> callback) {
            std::move(callback).Run(
                OnDeviceTranslationController::CanTranslateResult::kReadily);
          });
  EXPECT_CALL(*mock_service_controller, CreateTranslator("en", "es", _))
      .WillOnce([&](const std::string& source_lang,
                    const std::string& target_lang,
                    OnDeviceTranslationController::CreateTranslatorCallback
                        callback) {
        mojo::PendingRemote<on_device_translation::mojom::Translator> remote;
        auto receiver = remote.InitWithNewPipeAndPassReceiver();
        fake_translator_ =
            std::make_unique<on_device_translation::FakeTranslator>(
                std::move(receiver));
        std::move(callback).Run(std::move(remote));
      });
  std::unique_ptr<TranslationDispatcherOnDevice> dispatcher =
      CreateTranslationDispatcher(std::move(mock_service_controller));
  TranslateEventCallback on_translated_cb = base::BindOnce(
      &TranslationDispatcherOnDeviceTest::OnTranslated, base::Unretained(this));
  dispatcher->GetTranslation(
      "Hello world", "en-US", "es",
      std::move(on_translated_cb).Then(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(translated_text_, "Hola mundo");
}

TEST_F(TranslationDispatcherOnDeviceTest, GetTranslationFailure) {
  base::RunLoop run_loop;
  auto mock_service_controller =
      std::make_unique<MockOnDeviceTranslationServiceController>();
  EXPECT_CALL(*mock_service_controller, CanTranslate("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             base::OnceCallback<void(
                 OnDeviceTranslationController::CanTranslateResult)> callback) {
            std::move(callback).Run(
                OnDeviceTranslationController::CanTranslateResult::kReadily);
          });
  EXPECT_CALL(*mock_service_controller, CreateTranslator("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             OnDeviceTranslationController::CreateTranslatorCallback callback) {
            std::move(callback).Run(base::unexpected(
                OnDeviceTranslationController::CreateTranslatorError::
                    kFailedToInitialize));
          });
  std::unique_ptr<TranslationDispatcherOnDevice> dispatcher =
      CreateTranslationDispatcher(std::move(mock_service_controller));
  TranslateEventCallback on_translated_cb = base::BindOnce(
      &TranslationDispatcherOnDeviceTest::OnTranslated, base::Unretained(this));
  dispatcher->GetTranslation(
      "Hello world", "en-US", "es",
      std::move(on_translated_cb).Then(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(translated_text_.empty());
}

TEST_F(TranslationDispatcherOnDeviceTest, GetTranslationFailureOnCanTranslate) {
  base::RunLoop run_loop;
  auto mock_service_controller =
      std::make_unique<MockOnDeviceTranslationServiceController>();
  EXPECT_CALL(*mock_service_controller, CanTranslate("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             base::OnceCallback<void(
                 OnDeviceTranslationController::CanTranslateResult)> callback) {
            std::move(callback).Run(OnDeviceTranslationController::
                                        CanTranslateResult::kNoServiceCrashed);
          });
  EXPECT_CALL(*mock_service_controller, CreateTranslator("en", "es", _))
      .Times(0);
  std::unique_ptr<TranslationDispatcherOnDevice> dispatcher =
      CreateTranslationDispatcher(std::move(mock_service_controller));
  TranslateEventCallback on_translated_cb = base::BindOnce(
      &TranslationDispatcherOnDeviceTest::OnTranslated, base::Unretained(this));
  dispatcher->GetTranslation(
      "Hello world", "en-US", "es",
      std::move(on_translated_cb).Then(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(translated_text_.empty());
}

}  // namespace
}  // namespace captions
