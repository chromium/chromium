// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/translate_kit_client.h"

#include <tuple>

#include "base/files/scoped_temp_file.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/test/test_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

using base::test::ErrorIs;
using mojom::CreateTranslatorResult;

class TranslateKitClientTest : public testing::Test {
 public:
  TranslateKitClientTest()
      : file_operation_proxy_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}
  ~TranslateKitClientTest() override = default;

 protected:
  // Creates a TranslateKitClient with the given library path, packages, and
  // files.
  std::unique_ptr<TranslateKitClient> CreateClient(
      const base::FilePath& library_path,
      const std::vector<std::pair<std::string, std::string>>& packages,
      const std::vector<TestFile>& files) {
    auto client = TranslateKitClient::CreateForTest(library_path);
    SetConfig(*client, packages, files);
    return client;
  }

  void SetConfig(
      TranslateKitClient& client,
      const std::vector<std::pair<std::string, std::string>>& packages,
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

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeFileOperationProxy, base::OnTaskRunnerDeleter>
      file_operation_proxy_;
};

// Tests that the CanTranslate method returns true for a language pair that is
// supported.
TEST_F(TranslateKitClientTest, CanTranslateMatchingLanguagePack) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  EXPECT_TRUE(client->CanTranslate("en", "ja"));
}

// Tests that the CanTranslate method returns false for a language pair that is
// not supported.
TEST_F(TranslateKitClientTest, CanTranslateNoMatchingLanguagePack) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  EXPECT_FALSE(client->CanTranslate("en", "es"));
}

// Tests that the CanTranslate method returns false for a language pair that
// language data is not available.
TEST_F(TranslateKitClientTest, CanTranslateBrokenLanguagePack) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}}, {});
  EXPECT_FALSE(client->CanTranslate("en", "ja"));
}

// Tests that the CanTranslate method crashes if no config is set.
TEST_F(TranslateKitClientTest, CanTranslateNoConfig) {
  auto client = TranslateKitClient::CreateForTest(GetMockLibraryPath());
  EXPECT_CHECK_DEATH_WITH(client->CanTranslate("en", "ja"),
                          "SetConfig must have been called");
}

// Tests that the GetTranslator method returns a valid translator for a language
// pair that is supported, and the translator can be used to translate text.
TEST_F(TranslateKitClientTest, GetTranslatorMatchingLanguagePack) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  // Check that the translator can be used to translate text.
  // Note: the mock library returns the concatenation of the content of
  // "dict.dat" and the input text.
  EXPECT_EQ(translator->Translate("test"), "En to Ja - test");
}

// Tests that the GetTranslator method returns `kErrorFailedToCreateTranslator`
// for a language pair that is not supported.
TEST_F(TranslateKitClientTest, GetTranslatorNoMatchingLanguagePack) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  EXPECT_THAT(client->GetTranslator("en", "es"),
              ErrorIs(CreateTranslatorResult::kErrorFailedToCreateTranslator));
}

// Tests that the GetTranslator method returns `kErrorFailedToCreateTranslator`
// for a language pair that language data is not available.
TEST_F(TranslateKitClientTest, GetTranslatorBrokenLanguagePack) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}}, {});
  EXPECT_THAT(client->GetTranslator("en", "ja"),
              ErrorIs(CreateTranslatorResult::kErrorFailedToCreateTranslator));
}

// Tests that the GetTranslator method crashes if no config is set.
TEST_F(TranslateKitClientTest, GetTranslatorNoConfig) {
  auto client = TranslateKitClient::CreateForTest(GetMockLibraryPath());
  EXPECT_CHECK_DEATH_WITH(
      { std::ignore = client->GetTranslator("en", "ja"); },
      "SetConfig must have been called");
}

// Tests that SetConfig updates the config of the client, and the new config is
// used to create translators.
TEST_F(TranslateKitClientTest, UpdatingConfig) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator1, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator1);
  EXPECT_EQ(translator1->Translate("test"), "En to Ja - test");
  EXPECT_THAT(client->GetTranslator("en", "es"),
              ErrorIs(CreateTranslatorResult::kErrorFailedToCreateTranslator));

  SetConfig(*client, {{"en", "ja"}, {"en", "es"}},
            {
                {"0/dict.dat", "En to Ja - "},
                {"1/dict.dat", "En to Es - "},
            });

  ASSERT_OK_AND_ASSIGN(auto* translator2, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator2);
  EXPECT_EQ(translator2->Translate("test"), "En to Ja - test");

  ASSERT_OK_AND_ASSIGN(auto* translator3, client->GetTranslator("en", "es"));
  ASSERT_TRUE(translator3);
  EXPECT_EQ(translator3->Translate("test"), "En to Es - test");
}

// Tests that two translators can be created for the different language pairs,
// and they can be used to translate text.
TEST_F(TranslateKitClientTest, TwoTranslators) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}, {"en", "es"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                                 {"1/dict.dat", "En to Es - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator1, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator1);
  ASSERT_OK_AND_ASSIGN(auto* translator2, client->GetTranslator("en", "es"));
  ASSERT_TRUE(translator2);
  EXPECT_NE(translator1, translator2);
  EXPECT_EQ(translator1->Translate("test"), "En to Ja - test");
  EXPECT_EQ(translator2->Translate("test"), "En to Es - test");
}

// Tests that the same translator is returned for the same language pair, and
// it can be used to translate text.
TEST_F(TranslateKitClientTest, SameTranslator) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}, {"en", "es"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                                 {"1/dict.dat", "En to Es - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator1, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator1);
  ASSERT_OK_AND_ASSIGN(auto* translator2, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator2);
  EXPECT_EQ(translator1, translator2);
  EXPECT_EQ(translator1->Translate("test"), "En to Ja - test");
  EXPECT_EQ(translator2->Translate("test"), "En to Ja - test");
}

// Tests that the translate method returns nullopt if the library returns an
// error.
TEST_F(TranslateKitClientTest, TranslateFailure) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  EXPECT_EQ(translator->Translate("SIMULATE_ERROR"), std::nullopt);
}

// Tests that SplitSentences will return the original content (non-empty string)
// if the library returns an error.
TEST_F(TranslateKitClientTest, SplitSentencesFailure) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  std::vector<std::string> sentences =
      translator->SplitSentences("SIMULATE_ERROR");
  EXPECT_FALSE(sentences.empty());
  ASSERT_EQ(sentences[0], "SIMULATE_ERROR");
}

// Tests that the SplitSentences method returns sentence chunks.
TEST_F(TranslateKitClientTest, SplitSentences) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  std::vector<std::string> sentences =
      translator->SplitSentences("This is a sentence. This is another one.");
  ASSERT_EQ(sentences.size(), 2u);
}

// Tests that the SplitSentences method handles empty input.
TEST_F(TranslateKitClientTest, SplitSentencesEmptyInput) {
  auto client = CreateClient(GetMockLibraryPath(), {{"en", "ja"}},
                             {
                                 {"0/dict.dat", "En to Ja - "},
                             });
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  std::vector<std::string> sentences = translator->SplitSentences("");
  ASSERT_EQ(sentences.size(), 0u);
}

// Tests that the SplitSentences method returns the original content if the
// library does not support sentence splitting.
TEST_F(TranslateKitClientTest, SplitSentencesNotSupported) {
  auto client =
      CreateClient(GetMockLibraryWithoutSplitSentencesPath(), {{"en", "ja"}},
                   {
                       {"0/dict.dat", "En to Ja - "},
                   });
  ASSERT_OK_AND_ASSIGN(auto* translator, client->GetTranslator("en", "ja"));
  ASSERT_TRUE(translator);
  std::vector<std::string> sentences =
      translator->SplitSentences("This is a sentence. This is another one.");
  ASSERT_EQ(sentences.size(), 1u);
  ASSERT_EQ(sentences[0], "This is a sentence. This is another one.");
}

// Tests that the library is loaded successfully.
TEST_F(TranslateKitClientTest, LoadBinary) {
  base::HistogramTester histogram_tester;
  auto client = TranslateKitClient::CreateForTest(GetMockLibraryPath());
  histogram_tester.ExpectBucketCount("AI.Translation.LoadTranslateKitResult",
                                     LoadTranslateKitResult::kSuccess, 1);
}

// Tests the behavior of TranslateKitClient when the library is invalid.
TEST_F(TranslateKitClientTest, InvalidBinary) {
  base::ScopedTempFile invalid_lib;
  ASSERT_TRUE(invalid_lib.Create());

  base::HistogramTester histogram_tester;
  auto client = TranslateKitClient::CreateForTest(invalid_lib.path());
  histogram_tester.ExpectBucketCount("AI.Translation.LoadTranslateKitResult",
                                     LoadTranslateKitResult::kInvalidBinary, 1);

  // Calling following methods should not cause a crash.
  client->SetConfig(mojom::OnDeviceTranslationServiceConfigPtr());
  EXPECT_FALSE(client->CanTranslate("en", "ja"));
  EXPECT_THAT(client->GetTranslator("en", "ja"),
              ErrorIs(CreateTranslatorResult::kErrorInvalidBinary));
}

// Tests the behavior of TranslateKitClient when the library doesn't contain
// any of the methods in the TranslateKit API.
TEST_F(TranslateKitClientTest, InvalidFunctionPointerBinary) {
  base::HistogramTester histogram_tester;
  auto client = TranslateKitClient::CreateForTest(
      GetMockInvalidFunctionPointerLibraryPath());
  histogram_tester.ExpectBucketCount(
      "AI.Translation.LoadTranslateKitResult",
      LoadTranslateKitResult::kInvalidFunctionPointer, 1);

  // Calling following methods should not cause a crash.
  client->SetConfig(mojom::OnDeviceTranslationServiceConfigPtr());
  EXPECT_FALSE(client->CanTranslate("en", "ja"));
  EXPECT_THAT(client->GetTranslator("en", "ja"),
              ErrorIs(CreateTranslatorResult::kErrorInvalidFunctionPointer));
}

// Tests the behavior of TranslateKitClient when the library's
// CreateTranslateKit() method fails.
TEST_F(TranslateKitClientTest, FailingBinary) {
  base::HistogramTester histogram_tester;
  auto client = TranslateKitClient::CreateForTest(GetMockFailingLibraryPath());
  histogram_tester.ExpectBucketCount("AI.Translation.LoadTranslateKitResult",
                                     LoadTranslateKitResult::kSuccess, 1);

  // Calling following methods should not cause a crash.
  client->SetConfig(mojom::OnDeviceTranslationServiceConfigPtr());
  EXPECT_FALSE(client->CanTranslate("en", "ja"));
  EXPECT_THAT(client->GetTranslator("en", "ja"),
              ErrorIs(CreateTranslatorResult::kErrorFailedToInitialize));
}

}  // namespace

}  // namespace on_device_translation
