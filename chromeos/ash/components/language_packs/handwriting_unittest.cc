// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::language_packs {

namespace {

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

}  // namespace

}  // namespace ash::language_packs
