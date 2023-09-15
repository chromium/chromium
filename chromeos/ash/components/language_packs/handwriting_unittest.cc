// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace ash::language_packs {

namespace {

using ::ash::input_method::InputMethodManager;
using ::ash::input_method::InputMethodUtil;
using ::ash::input_method::MockInputMethodManager;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

class HandwritingTest : public testing::Test {};

absl::optional<std::string> GetSecondUnderscorePart(
    const std::string& engine_id) {
  std::vector<std::string> split = base::SplitString(
      engine_id, "_", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2) {
    return absl::nullopt;
  } else {
    return split[1];
  }
}

// Populates proto message DlcsWithContent with the given list of dlc_ids.
// All fields other than `id` are left as default.
dlcservice::DlcsWithContent CreateDlcsWithContent(
    const std::vector<std::string>& dlc_ids) {
  dlcservice::DlcsWithContent output;
  for (const auto& dlc_id : dlc_ids) {
    output.add_dlc_infos()->set_id(dlc_id);
  }
  return output;
}

struct PartialDescriptor {
  std::string engine_id;
  absl::optional<std::string> handwriting_language;
};

// Ensures the lifetime of the fake `InputMethodDelegate` outlives the
// `InputMethodUtil` it is used in.
// As both `InputMethodDelegate` and `InputMethodUtil` have deleted copy
// constructors, this cannot be copied or moved.
class DelegateUtil {
 public:
  explicit DelegateUtil(base::span<const PartialDescriptor> partial_descriptors)
      : util_(&delegate_) {
    Init(partial_descriptors);
  }

  input_method::InputMethodUtil* util() { return &util_; }

 private:
  input_method::FakeInputMethodDelegate delegate_;
  input_method::InputMethodUtil util_;

  void Init(base::span<const PartialDescriptor> partial_descriptors) {
    std::vector<input_method::InputMethodDescriptor> descriptors;
    descriptors.reserve(partial_descriptors.size());

    for (const PartialDescriptor& partial_descriptor : partial_descriptors) {
      const std::string id = extension_ime_util::GetInputMethodIDByEngineID(
          partial_descriptor.engine_id);
      descriptors.push_back(input_method::InputMethodDescriptor(
          id, /*name=*/"", /*indicator=*/"", /*keyboard_layout=*/"",
          /*language_codes=*/{""},  // Must be non-empty to avoid a DCHECK.
          /*is_login_keyboard=*/false,
          /*options_page_url=*/{}, /*input_view_url=*/{},
          partial_descriptor.handwriting_language));
    }

    util_.InitXkbInputMethodsForTesting(descriptors);
  }
};

// Fake InputMethodManager used for testing.
// InputMethodManager with available IMEs.
class FakeInputMethodManager : public MockInputMethodManager {
 public:
  class TestState : public MockInputMethodManager::State {
   public:
    // This constructor takes in engine IDs for conciseness and readability in
    // testing, so we convert them to input method IDs here.
    TestState(const std::vector<std::string>& engine_ids) {
      for (const auto& engine_id : engine_ids) {
        input_method_ids_.push_back(
            extension_ime_util::GetInputMethodIDByEngineID(engine_id));
      }
    }

    TestState(const TestState&) = delete;
    TestState& operator=(const TestState&) = delete;

    const std::vector<std::string>& GetEnabledInputMethodIds() const override {
      return input_method_ids_;
    }

    std::vector<std::string> input_method_ids_;

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override {}
  };

  // Constructor.
  // The first argument `enabled_engine_ids` contains the list of engine IDs
  // that corresponds to the input methods that the user has currently enabled.
  // The second argument `partial_descriptors` is used for internal mapping: in
  // correct scenarios it should always include the IDs of the first argument.
  explicit FakeInputMethodManager(
      const std::vector<std::string>& enabled_engine_ids,
      base::span<const PartialDescriptor> partial_descriptors)
      : state_(base::MakeRefCounted<TestState>(enabled_engine_ids)),
        delegate_util_(partial_descriptors) {}

  FakeInputMethodManager(const FakeInputMethodManager&) = delete;
  FakeInputMethodManager& operator=(const FakeInputMethodManager&) = delete;

  ~FakeInputMethodManager() override = default;

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  InputMethodUtil* GetInputMethodUtil() override {
    return delegate_util_.util();
  }

  scoped_refptr<TestState> state_;
  DelegateUtil delegate_util_;
};

TEST_F(HandwritingTest, MapIdsToHandwritingLocalesNoInput) {
  EXPECT_THAT(
      MapIdsToHandwritingLocales(
          {}, base::BindRepeating([](const std::string& unused_engine_id)
                                      -> absl::optional<std::string> {
            ADD_FAILURE() << "engine_id_to_handwriting_locale was called";
            return "en";
          })),
      IsEmpty());
}

TEST_F(HandwritingTest, MapIdsToHandwritingLocalesAllToNullopt) {
  EXPECT_THAT(MapIdsToHandwritingLocales(
                  {{"qwerty_en", "qwertz_de"}},
                  base::BindRepeating([](const std::string& unused_engine_id)
                                          -> absl::optional<std::string> {
                    return absl::nullopt;
                  })),
              IsEmpty());
}

TEST_F(HandwritingTest, MapIdsToHandwritingLocalesAllToUniqueStrings) {
  EXPECT_THAT(
      MapIdsToHandwritingLocales({{"qwerty_en", "qwertz_de"}},
                                 base::BindRepeating(GetSecondUnderscorePart)),
      UnorderedElementsAre("en", "de"));
}

TEST_F(HandwritingTest, MapIdsToHandwritingLocalesRepeatedString) {
  EXPECT_THAT(
      MapIdsToHandwritingLocales({{"qwerty_en", "qzertz_de", "qwertz_en"}},
                                 base::BindRepeating(GetSecondUnderscorePart)),
      UnorderedElementsAre("en", "de"));
}

TEST_F(HandwritingTest, MapIdsToHandwritingLocalesSomeNullopt) {
  EXPECT_THAT(
      MapIdsToHandwritingLocales({{"qwerty_en", "nohandwriting", "qwertz_de"}},
                                 base::BindRepeating(GetSecondUnderscorePart)),
      UnorderedElementsAre("en", "de"));
}

TEST_F(HandwritingTest, MapEngineIdToHandwritingLocaleNoInputMethods) {
  DelegateUtil delegate_util({});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:us::eng"),
              absl::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              absl::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              absl::nullopt);
}

TEST_F(HandwritingTest,
       MapEngineIdToHandwritingLocaleInputMethodsWithoutHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", absl::nullopt}, {"xkb:fr::fra", absl::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:us::eng"),
              absl::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              absl::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              absl::nullopt);
}

TEST_F(HandwritingTest,
       MapEngineIdToHandwritingLocaleSomeInputMethodsWithHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", "en"}, {"xkb:fr::fra", absl::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:us::eng"),
              Optional(Eq("en")));
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              absl::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              absl::nullopt);
}

TEST_F(HandwritingTest,
       MapEngineIdToHandwritingLocaleInputMethodsWithHandwriting) {
  DelegateUtil delegate_util({{{"xkb:us::eng", "en"}, {"xkb:fr::fra", "fr"}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:us::eng"),
              Optional(Eq("en")));
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              Optional(Eq("fr")));
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              absl::nullopt);
}

TEST_F(HandwritingTest, MapEngineIdsToHandwritingLocalesIntegration) {
  DelegateUtil delegate_util({{{"xkb:us::eng", "en"},
                               {"xkb:gb:extd:eng", "en"},
                               {"xkb:fr::fra", "fr"}}});
  input_method::InputMethodUtil* util = delegate_util.util();

  EXPECT_THAT(
      MapIdsToHandwritingLocales(
          {{"xkb:de::ger", "xkb:us::eng", "xkb:gb:extd:eng", "xkb:fr::fra"}},
          base::BindRepeating(MapEngineIdToHandwritingLocale, util)),
      UnorderedElementsAre("en", "fr"));
}

TEST_F(HandwritingTest, MapInputMethodIdToHandwritingLocaleNoInputMethods) {
  DelegateUtil delegate_util({});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng")),
      absl::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      absl::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      absl::nullopt);
}

TEST_F(HandwritingTest,
       MapInputMethodIdToHandwritingLocaleInputMethodsWithoutHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", absl::nullopt}, {"xkb:fr::fra", absl::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng")),
      absl::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      absl::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      absl::nullopt);
}

TEST_F(HandwritingTest,
       MapInputMethodIdToHandwritingLocaleSomeInputMethodsWithHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", "en"}, {"xkb:fr::fra", absl::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng")),
      Optional(Eq("en")));
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      absl::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      absl::nullopt);
}

TEST_F(HandwritingTest,
       MapInputMethodIdToHandwritingLocaleInputMethodsWithHandwriting) {
  DelegateUtil delegate_util({{{"xkb:us::eng", "en"}, {"xkb:fr::fra", "fr"}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng")),
      Optional(Eq("en")));
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      Optional(Eq("fr")));
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      absl::nullopt);
}

TEST_F(HandwritingTest, MapInputMethodIdsToHandwritingLocalesIntegration) {
  DelegateUtil delegate_util({{{"xkb:us::eng", "en"},
                               {"xkb:gb:extd:eng", "en"},
                               {"xkb:fr::fra", "fr"}}});
  input_method::InputMethodUtil* util = delegate_util.util();

  EXPECT_THAT(
      MapIdsToHandwritingLocales(
          {{extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger"),
            extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng"),
            extension_ime_util::GetInputMethodIDByEngineID("xkb:gb:extd:eng"),
            extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")}},
          base::BindRepeating(MapInputMethodIdToHandwritingLocale, util)),
      UnorderedElementsAre("en", "fr"));
}

struct HandwritingLocaleToDlcTestCase {
  std::string test_name;
  std::string_view locale;
  absl::optional<std::string> expected;
};

class HandwritingLocaleToDlcTest
    : public HandwritingTest,
      public testing::WithParamInterface<HandwritingLocaleToDlcTestCase> {};

TEST_P(HandwritingLocaleToDlcTest, Test) {
  const HandwritingLocaleToDlcTestCase& test_case = GetParam();

  EXPECT_EQ(HandwritingLocaleToDlc(test_case.locale), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    HandwritingLocaleToDlcTests,
    HandwritingLocaleToDlcTest,
    testing::ValuesIn<HandwritingLocaleToDlcTestCase>(
        {{"InvalidEmpty", "", absl::nullopt},
         {"InvalidEn", "en", absl::nullopt},
         {"InvalidDeDe", "de-DE", absl::nullopt},
         {"InvalidCy", "cy", absl::nullopt},
         {"ValidDe", "de", "handwriting-de"},
         {"ValidZhHk", "zh-HK", "handwriting-zh-HK"}}),
    [](const testing::TestParamInfo<HandwritingLocaleToDlcTest::ParamType>&
           info) { return info.param.test_name; });

struct IsHandwritingDlcTestCase {
  std::string test_name;
  std::string_view dlc_id;
  bool expected;
};

class IsHandwritingDlcTest
    : public HandwritingTest,
      public testing::WithParamInterface<IsHandwritingDlcTestCase> {};

TEST_P(IsHandwritingDlcTest, Test) {
  const IsHandwritingDlcTestCase& test_case = GetParam();

  EXPECT_EQ(IsHandwritingDlc(test_case.dlc_id), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    IsHandwritingDlcTests,
    IsHandwritingDlcTest,
    testing::ValuesIn<IsHandwritingDlcTestCase>(
        {{"InvalidEmpty", "", false},
         {"InvalidEn", "handwriting-en", false},
         {"InvalidCy", "handwriting-cy", false},
         {"InvalidDeDe", "handwriting-de-DE", false},
         {"InvalidTypoDe", "handwritting-de", false},
         {"InvalidTtsEnUs", "tts-en-us", false},
         {"InvalidDeWithoutPrefix", "de", false},
         {"ValidDe", "handwriting-de", true},
         {"ValidZhHk", "handwriting-zh-HK", true}}),
    [](const testing::TestParamInfo<IsHandwritingDlcTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_F(HandwritingTest, FilterHandwritingDlcsDefault) {
  EXPECT_THAT(FilterHandwritingDlcsWithContent(dlcservice::DlcsWithContent()),
              IsEmpty());
}

TEST_F(HandwritingTest, FilterHandwritingDlcsNotHandwriting) {
  const dlcservice::DlcsWithContent dlcs_without_handwriting =
      CreateDlcsWithContent({"tts-en-us", "grammar-it"});

  EXPECT_THAT(FilterHandwritingDlcsWithContent(dlcs_without_handwriting),
              IsEmpty());
}

TEST_F(HandwritingTest, FilterHandwritingDlcsVariousEntries) {
  // "handwriting-cy" refer to Welsh language, which is not a language that is
  // supported in Handwriting.
  const dlcservice::DlcsWithContent dlcs_with_some_handwriting =
      CreateDlcsWithContent({"handwriting-fr", "tts-en-us", "handwriting-it",
                             "grammar-it", "handwriting-cy"});

  EXPECT_THAT(FilterHandwritingDlcsWithContent(dlcs_with_some_handwriting),
              UnorderedElementsAre("handwriting-fr", "handwriting-it"));
}

TEST_F(HandwritingTest, GetTargetDlcIdsEmpty) {
  FakeInputMethodManager fake_imm({}, {});

  EXPECT_THAT(GetDlcIdsFromEnabledInputMethods(&fake_imm), IsEmpty());
}

TEST_F(HandwritingTest, GetTargetDlcIdsNoMapping) {
  const std::vector<std::string> enabled_engine_ids(
      {"xkb:us::eng", "xkb:it::ita"});
  const std::vector<const PartialDescriptor> partial_descriptors({});
  FakeInputMethodManager fake_imm(enabled_engine_ids, partial_descriptors);

  EXPECT_THAT(GetDlcIdsFromEnabledInputMethods(&fake_imm), IsEmpty());
}

TEST_F(HandwritingTest, GetTargetDlcIdsValidMapping) {
  const std::vector<std::string> enabled_engine_ids(
      {"xkb:fr::fra", "xkb:de::ger", "xkb:it::ita"});
  const std::vector<const PartialDescriptor> partial_descriptors(
      {{{"xkb:fr::fra", "fr"}, {"xkb:it::ita", "it"}}});
  FakeInputMethodManager fake_imm(enabled_engine_ids, partial_descriptors);

  EXPECT_THAT(GetDlcIdsFromEnabledInputMethods(&fake_imm),
              UnorderedElementsAre("handwriting-fr", "handwriting-it"));
}

TEST_F(HandwritingTest, GetTargetDlcIdsMappingMissing) {
  const std::vector<std::string> enabled_engine_ids(
      {"xkb:de::ger", "xkb:it::ita"});
  const std::vector<const PartialDescriptor> partial_descriptors(
      {{{"xkb:fr::fra", "fr"}, {"xkb:it::ita", "it"}}});
  FakeInputMethodManager fake_imm(enabled_engine_ids, partial_descriptors);

  EXPECT_THAT(GetDlcIdsFromEnabledInputMethods(&fake_imm),
              UnorderedElementsAre("handwriting-it"));
}

TEST_F(HandwritingTest, GetTargetDlcIdsMalformedIds) {
  const std::vector<std::string> enabled_engine_ids(
      {"xkb:fr::fra", "xkb:de::ger", "xkb:it::ita"});
  const std::vector<const PartialDescriptor> partial_descriptors(
      {{{"xkb:cy::woo", "fr"}, {"xkb:it::ita", "it"}, {"xkb_ime_lol", "it"}}});
  FakeInputMethodManager fake_imm(enabled_engine_ids, partial_descriptors);

  EXPECT_THAT(GetDlcIdsFromEnabledInputMethods(&fake_imm),
              UnorderedElementsAre("handwriting-it"));
}

}  // namespace

}  // namespace ash::language_packs
