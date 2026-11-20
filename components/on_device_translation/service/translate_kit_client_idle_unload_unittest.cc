// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/on_device_translation/service/on_device_translation_service.h"
#include "components/on_device_translation/service/test/test_util.h"
#include "components/on_device_translation/service/translate_kit_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace on_device_translation {
namespace {

constexpr base::TimeDelta kIdleUnloadTimeout = base::Seconds(60);
constexpr char kModelFileMmapSuccessMetricName[] =
    "Translate.OnDeviceTranslation.ModelFileMmapSuccess";
constexpr char kModelUnloadReasonMetricName[] =
    "AI.Translation.ModelUnloadReason";
constexpr char kModelReloadSuccessMetricName[] =
    "AI.Translation.ModelReloadSuccess";
constexpr char kModelReloadTimeMetricName[] = "AI.Translation.ModelReloadTime";

using Packages = std::vector<std::pair<std::string, std::string>>;

// Tests for unloading the TranslateKit models of translators that are idle or
// that exist under memory pressure.
class TranslateKitClientIdleUnloadTest : public testing::Test {
 public:
  TranslateKitClientIdleUnloadTest()
      : file_operation_proxy_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}
  ~TranslateKitClientIdleUnloadTest() override = default;

 protected:
  void SetIdleUnloadTimeout(base::TimeDelta idle_unload_timeout) {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kTranslationAPI,
        {{"TranslationAPIModelIdleUnloadTimeout",
          base::StrCat(
              {base::NumberToString(idle_unload_timeout.InSeconds()), "s"})}});
  }

  std::unique_ptr<TranslateKitClient> CreateClient(
      const Packages& packages,
      const std::vector<TestFile>& files) {
    auto client = TranslateKitClient::CreateForTest(GetMockLibraryPath());
    SetConfig(*client, packages, files);
    return client;
  }

  void SetConfig(TranslateKitClient& client,
                 const Packages& packages,
                 const std::vector<TestFile>& files) {
    auto config = mojom::OnDeviceTranslationServiceConfig::New();
    for (const auto& pair : packages) {
      config->packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          pair.first, pair.second));
    }
    file_operation_proxy_ = FakeFileOperationProxy::Create(
        config->file_operation_proxy.InitWithNewPipeAndPassReceiver(), files);
    client.SetConfig(std::move(config));
  }

  void SimulateMemoryPressure(base::MemoryLimit memory_limit) {
    memory_consumer_registry_.NotifyUpdateMemoryLimit(memory_limit);
    memory_consumer_registry_.NotifyReleaseMemory();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  base::TestMemoryConsumerRegistry memory_consumer_registry_;

 private:
  std::unique_ptr<FakeFileOperationProxy, base::OnTaskRunnerDeleter>
      file_operation_proxy_;
};

// Tests that a translator unloads its model after the idle timeout, and that
// the next translation loads the model again.
TEST_F(TranslateKitClientIdleUnloadTest, UnloadsAfterIdleTimeoutAndReloads) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  base::HistogramTester histogram_tester;
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  EXPECT_EQ(translator->Translate("test"), "En to Ja - test");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);

  task_environment_.FastForwardBy(kIdleUnloadTimeout - base::Seconds(1));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);
  histogram_tester.ExpectUniqueSample(kModelFileMmapSuccessMetricName, true, 1);
  histogram_tester.ExpectUniqueSample(kModelUnloadReasonMetricName,
                                      TranslateKitModelUnloadReason::kIdle, 1);

  // The first load is not a reload.
  histogram_tester.ExpectTotalCount(kModelReloadSuccessMetricName, 0);
  histogram_tester.ExpectTotalCount(kModelReloadTimeMetricName, 0);

  // The translator pointer stays valid, and the model loads again.
  EXPECT_EQ(translator->Translate("again"), "En to Ja - again");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
  histogram_tester.ExpectUniqueSample(kModelFileMmapSuccessMetricName, true, 2);
  histogram_tester.ExpectUniqueSample(kModelReloadSuccessMetricName, true, 1);
  histogram_tester.ExpectTotalCount(kModelReloadTimeMetricName, 1);

  // The reload starts a new idle period.
  task_environment_.FastForwardBy(kIdleUnloadTimeout);
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);
  histogram_tester.ExpectUniqueSample(kModelUnloadReasonMetricName,
                                      TranslateKitModelUnloadReason::kIdle, 2);

  // Destroying the client deletes the translators without logging an unload.
  client.reset();
  histogram_tester.ExpectTotalCount(kModelUnloadReasonMetricName, 2);
}

// Tests that each translation restarts the idle timeout.
TEST_F(TranslateKitClientIdleUnloadTest, TranslateRestartsIdleTimeout) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));

  task_environment_.FastForwardBy(kIdleUnloadTimeout / 2);
  EXPECT_EQ(translator->Translate("test"), "En to Ja - test");
  task_environment_.FastForwardBy(kIdleUnloadTimeout / 2);
  EXPECT_EQ(translator->Translate("test"), "En to Ja - test");
  task_environment_.FastForwardBy(kIdleUnloadTimeout - base::Seconds(1));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);
}

// Tests that a translator that was only created, for example by
// CanTranslate(), also unloads after the idle timeout.
TEST_F(TranslateKitClientIdleUnloadTest, UnusedTranslatorUnloads) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  EXPECT_TRUE(client->CanTranslate("en", "ja"));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
  task_environment_.FastForwardBy(kIdleUnloadTimeout);
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);

  // An unloaded translator still reports that it can translate.
  EXPECT_TRUE(client->CanTranslate("en", "ja"));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);
}

// Tests that only the translators that were idle for the timeout unload.
TEST_F(TranslateKitClientIdleUnloadTest, OnlyIdleTranslatorsUnload) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  auto client = CreateClient({{"en", "ja"}, {"en", "es"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                                 {"1/dict.dat", "En to Es - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* en_ja, client->GetTranslator("en", "ja"));
  task_environment_.FastForwardBy(kIdleUnloadTimeout / 2);
  ASSERT_OK_AND_ASSIGN(auto* en_es, client->GetTranslator("en", "es"));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 2u);

  task_environment_.FastForwardBy(kIdleUnloadTimeout / 2);
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
  EXPECT_EQ(en_es->Translate("test"), "En to Es - test");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);

  EXPECT_EQ(en_ja->Translate("test"), "En to Ja - test");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 2u);
}

// Tests that SplitSentences() does not load an unloaded model, as sentence
// splitting does not use it.
TEST_F(TranslateKitClientIdleUnloadTest, SplitSentencesDoesNotLoadModel) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  task_environment_.FastForwardBy(kIdleUnloadTimeout);
  ASSERT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);

  EXPECT_EQ(translator->SplitSentences("First. Second."),
            std::vector<std::string>({"First", "Second"}));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);
}

// Tests that a translation fails when the model can't load again, and that a
// later translation succeeds once the language pack is back.
TEST_F(TranslateKitClientIdleUnloadTest, ReloadFailureFailsTranslation) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  base::HistogramTester histogram_tester;
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  task_environment_.FastForwardBy(kIdleUnloadTimeout);
  ASSERT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);

  SetConfig(*client, {{"en", "ja"}}, {});
  EXPECT_EQ(translator->Translate("test"), std::nullopt);
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);

  SetConfig(*client, {{"en", "ja"}}, {{"0/dict.dat", "En to Ja v2 - "}});
  EXPECT_EQ(translator->Translate("test"), "En to Ja v2 - test");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);

  histogram_tester.ExpectBucketCount(kModelReloadSuccessMetricName, false, 1);
  histogram_tester.ExpectBucketCount(kModelReloadSuccessMetricName, true, 1);
  histogram_tester.ExpectTotalCount(kModelReloadTimeMetricName, 1);
}

// Tests that a zero timeout keeps models loaded for as long as the translators
// exist.
TEST_F(TranslateKitClientIdleUnloadTest, ZeroTimeoutKeepsModelsLoaded) {
  SetIdleUnloadTimeout(base::TimeDelta());
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  EXPECT_EQ(translator->Translate("test"), "En to Ja - test");
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
}

// Tests that moderate memory pressure unloads all models, including those of
// translators that were just used.
TEST_F(TranslateKitClientIdleUnloadTest, MemoryPressureUnloadsAllModels) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  base::HistogramTester histogram_tester;
  auto client = CreateClient({{"en", "ja"}, {"en", "es"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                                 {"1/dict.dat", "En to Es - "},
                             });
  EXPECT_EQ(memory_consumer_registry_.size(), 1u);
  ASSERT_OK_AND_ASSIGN(auto* en_ja, client->GetTranslator("en", "ja"));
  ASSERT_OK_AND_ASSIGN(auto* en_es, client->GetTranslator("en", "es"));
  EXPECT_EQ(en_ja->Translate("test"), "En to Ja - test");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 2u);

  SimulateMemoryPressure(base::MemoryLimit::ModeratePressureThreshold());
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 0u);
  histogram_tester.ExpectUniqueSample(
      kModelUnloadReasonMetricName,
      TranslateKitModelUnloadReason::kMemoryPressure, 2);

  // A second notification has nothing left to unload.
  SimulateMemoryPressure(base::MemoryLimit::CriticalPressureThreshold());
  histogram_tester.ExpectTotalCount(kModelUnloadReasonMetricName, 2);

  EXPECT_EQ(en_ja->Translate("test"), "En to Ja - test");
  EXPECT_EQ(en_es->Translate("test"), "En to Es - test");
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 2u);
  histogram_tester.ExpectUniqueSample(kModelReloadSuccessMetricName, true, 2);
  histogram_tester.ExpectTotalCount(kModelReloadTimeMetricName, 2);
}

// Tests that a memory limit above the moderate pressure threshold keeps the
// models.
TEST_F(TranslateKitClientIdleUnloadTest, NoPressureKeepsModels) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  SimulateMemoryPressure(base::MemoryLimit::NoPressureThreshold());
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
}

// Tests that the client does not watch memory pressure when the idle unload
// timeout is zero.
TEST_F(TranslateKitClientIdleUnloadTest, MemoryPressureParamOff) {
  SetIdleUnloadTimeout(base::TimeDelta());
  auto client = CreateClient({{"en", "ja"}}, {{"0/dict.dat", "En to Ja - "}});
  EXPECT_EQ(memory_consumer_registry_.size(), 0u);
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  SimulateMemoryPressure(base::MemoryLimit::CriticalPressureThreshold());
  EXPECT_EQ(client->GetLoadedTranslatorCountForTesting(), 1u);
}

// Tests that a translator bound through the service keeps working across an
// idle unload.
TEST_F(TranslateKitClientIdleUnloadTest, ServiceTranslatorReloads) {
  SetIdleUnloadTimeout(kIdleUnloadTimeout);
  auto client = TranslateKitClient::CreateForTest(GetMockLibraryPath());
  TranslateKitClient* client_ptr = client.get();
  mojo::Remote<mojom::OnDeviceTranslationService> service_remote;
  auto service = OnDeviceTranslationService::CreateForTesting(
      service_remote.BindNewPipeAndPassReceiver(), std::move(client));

  auto config = mojom::OnDeviceTranslationServiceConfig::New();
  config->packages.push_back(
      mojom::OnDeviceTranslationLanguagePackage::New("en", "ja"));
  auto file_operation_proxy = FakeFileOperationProxy::Create(
      config->file_operation_proxy.InitWithNewPipeAndPassReceiver(),
      {{"0/dict.dat", "En to Ja - "}});
  service_remote->SetServiceConfig(std::move(config));

  mojo::Remote<mojom::OnDeviceTranslator> translator;
  {
    base::RunLoop run_loop;
    service_remote->CreateTranslator(
        "en", "ja", translator.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](mojom::CreateTranslatorResult result) {
          EXPECT_EQ(result, mojom::CreateTranslatorResult::kSuccess);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  task_environment_.FastForwardBy(kIdleUnloadTimeout);
  EXPECT_EQ(client_ptr->GetLoadedTranslatorCountForTesting(), 0u);

  base::RunLoop run_loop;
  translator->Translate(
      "test",
      base::BindLambdaForTesting([&](const std::optional<std::string>& output) {
        EXPECT_EQ(output, "En to Ja - test");
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(client_ptr->GetLoadedTranslatorCountForTesting(), 1u);
}

}  // namespace
}  // namespace on_device_translation
