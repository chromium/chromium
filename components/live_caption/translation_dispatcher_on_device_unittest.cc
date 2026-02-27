// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/translation_dispatcher_on_device.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/on_device_translation/service_controller.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "components/on_device_translation/test/fake_service_controller_manager.h"
#include "components/on_device_translation/test/fake_translator.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captions {
namespace {

using ::on_device_translation::FakeOnDeviceTranslationInstaller;
using ::testing::_;

using CreateTranslatorCallback = base::OnceCallback<void(
    base::expected<
        mojo::PendingRemote<on_device_translation::mojom::Translator>,
        blink::mojom::CreateTranslatorError>)>;

class MockOnDeviceTranslationServiceController
    : public on_device_translation::OnDeviceTranslationServiceController {
 public:
  MockOnDeviceTranslationServiceController(
      PrefService* local_state,
      on_device_translation::ServiceControllerManager* manager)
      : OnDeviceTranslationServiceController(local_state,
                                             manager,
                                             url::Origin()) {}
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
               base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)>
                   callback),
              (override));

 protected:
  ~MockOnDeviceTranslationServiceController() override = default;
};

class TranslationDispatcherOnDeviceTest : public testing::Test {
 public:
  void SetUp() override {
    fake_installer_ =
        std::make_unique<FakeOnDeviceTranslationInstaller>(base::FilePath());
    service_controller_manager_ =
        std::make_unique<on_device_translation::FakeServiceControllerManager>(
            &local_state_);
    mock_service_controller_ =
        base::MakeRefCounted<MockOnDeviceTranslationServiceController>(
            &local_state_, service_controller_manager_.get());
    service_controller_manager_->SetServiceControllerForTest(
        url::Origin(), mock_service_controller_);
    translation_dispatcher_on_device_ =
        std::make_unique<TranslationDispatcherOnDevice>(
            service_controller_manager_.get());
  }

  void TearDown() override {
    translation_dispatcher_on_device_.reset();
    mock_service_controller_.reset();
    service_controller_manager_.reset();
  }

  void OnTranslated(const TranslateEvent& result) {
    if (result.has_value()) {
      translated_text_ = result.value();
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<on_device_translation::FakeServiceControllerManager>
      service_controller_manager_;
  scoped_refptr<MockOnDeviceTranslationServiceController>
      mock_service_controller_;
  std::unique_ptr<TranslationDispatcherOnDevice>
      translation_dispatcher_on_device_;
  std::string translated_text_;
  std::unique_ptr<on_device_translation::FakeTranslator> fake_translator_;
  std::unique_ptr<FakeOnDeviceTranslationInstaller> fake_installer_;

  TranslationDispatcherOnDeviceTest() = default;
};

TEST_F(TranslationDispatcherOnDeviceTest, GetTranslationSuccess) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_service_controller_, CanTranslate("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)>
                 callback) {
            std::move(callback).Run(
                blink::mojom::CanCreateTranslatorResult::kReadily);
          });
  EXPECT_CALL(*mock_service_controller_, CreateTranslator("en", "es", _))
      .WillOnce([&](const std::string& source_lang,
                    const std::string& target_lang,
                    CreateTranslatorCallback callback) {
        mojo::PendingRemote<on_device_translation::mojom::Translator> remote;
        auto receiver = remote.InitWithNewPipeAndPassReceiver();
        fake_translator_ =
            std::make_unique<on_device_translation::FakeTranslator>(
                std::move(receiver));
        std::move(callback).Run(std::move(remote));
      });
  TranslateEventCallback on_translated_cb = base::BindOnce(
      &TranslationDispatcherOnDeviceTest::OnTranslated, base::Unretained(this));
  translation_dispatcher_on_device_->GetTranslation(
      "Hello world", "en-US", "es",
      std::move(on_translated_cb).Then(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(translated_text_, "Hola mundo");
}

TEST_F(TranslationDispatcherOnDeviceTest, GetTranslationFailure) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_service_controller_, CanTranslate("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)>
                 callback) {
            std::move(callback).Run(
                blink::mojom::CanCreateTranslatorResult::kReadily);
          });
  EXPECT_CALL(*mock_service_controller_, CreateTranslator("en", "es", _))
      .WillOnce([](const std::string& source_lang,
                   const std::string& target_lang,
                   CreateTranslatorCallback callback) {
        std::move(callback).Run(base::unexpected(
            blink::mojom::CreateTranslatorError::kFailedToInitialize));
      });
  TranslateEventCallback on_translated_cb = base::BindOnce(
      &TranslationDispatcherOnDeviceTest::OnTranslated, base::Unretained(this));
  translation_dispatcher_on_device_->GetTranslation(
      "Hello world", "en-US", "es",
      std::move(on_translated_cb).Then(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(translated_text_.empty());
}

TEST_F(TranslationDispatcherOnDeviceTest, GetTranslationFailureOnCanTranslate) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_service_controller_, CanTranslate("en", "es", _))
      .WillOnce(
          [](const std::string& source_lang, const std::string& target_lang,
             base::OnceCallback<void(blink::mojom::CanCreateTranslatorResult)>
                 callback) {
            std::move(callback).Run(
                blink::mojom::CanCreateTranslatorResult::kNoServiceCrashed);
          });
  EXPECT_CALL(*mock_service_controller_, CreateTranslator("en", "es", _))
      .Times(0);
  TranslateEventCallback on_translated_cb = base::BindOnce(
      &TranslationDispatcherOnDeviceTest::OnTranslated, base::Unretained(this));
  translation_dispatcher_on_device_->GetTranslation(
      "Hello world", "en-US", "es",
      std::move(on_translated_cb).Then(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(translated_text_.empty());
}

}  // namespace
}  // namespace captions
