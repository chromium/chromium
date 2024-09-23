// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/language_packs/language_packs_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  std::optional<std::string> handwriting_language;
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
              std::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              std::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              std::nullopt);
}

TEST_F(HandwritingTest,
       MapEngineIdToHandwritingLocaleInputMethodsWithoutHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", std::nullopt}, {"xkb:fr::fra", std::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:us::eng"),
              std::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              std::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              std::nullopt);
}

TEST_F(HandwritingTest,
       MapEngineIdToHandwritingLocaleSomeInputMethodsWithHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", "en"}, {"xkb:fr::fra", std::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:us::eng"),
              Optional(Eq("en")));
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:fr::fra"),
              std::nullopt);
  EXPECT_THAT(MapEngineIdToHandwritingLocale(util, "xkb:de::ger"),
              std::nullopt);
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
              std::nullopt);
}

TEST_F(HandwritingTest, MapEngineIdsToHandwritingLocalesIntegration) {
  DelegateUtil delegate_util({{{"xkb:us::eng", "en"},
                               {"xkb:gb:extd:eng", "en"},
                               {"xkb:fr::fra", "fr"}}});
  input_method::InputMethodUtil* util = delegate_util.util();

  EXPECT_THAT(
      MapThenFilterStrings(
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
      std::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      std::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      std::nullopt);
}

TEST_F(HandwritingTest,
       MapInputMethodIdToHandwritingLocaleInputMethodsWithoutHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", std::nullopt}, {"xkb:fr::fra", std::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng")),
      std::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      std::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      std::nullopt);
}

TEST_F(HandwritingTest,
       MapInputMethodIdToHandwritingLocaleSomeInputMethodsWithHandwriting) {
  DelegateUtil delegate_util(
      {{{"xkb:us::eng", "en"}, {"xkb:fr::fra", std::nullopt}}});
  input_method::InputMethodUtil* util = delegate_util.util();
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng")),
      Optional(Eq("en")));
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:fr::fra")),
      std::nullopt);
  EXPECT_THAT(
      MapInputMethodIdToHandwritingLocale(
          util, extension_ime_util::GetInputMethodIDByEngineID("xkb:de::ger")),
      std::nullopt);
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
      std::nullopt);
}

TEST_F(HandwritingTest, MapInputMethodIdsToHandwritingLocalesIntegration) {
  DelegateUtil delegate_util({{{"xkb:us::eng", "en"},
                               {"xkb:gb:extd:eng", "en"},
                               {"xkb:fr::fra", "fr"}}});
  input_method::InputMethodUtil* util = delegate_util.util();

  EXPECT_THAT(
      MapThenFilterStrings(
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
  std::optional<std::string> expected;
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
        {{"InvalidEmpty", "", std::nullopt},
         {"InvalidDeDe", "de-DE", std::nullopt},
         {"InvalidCy", "cy", std::nullopt},
         {"ValidDe", "de", "handwriting-de"},
         {"ValidEn", "en", "handwriting-en"},
         {"ValidZhHk", "zh-HK", "handwriting-zh-HK"}}),
    [](const testing::TestParamInfo<HandwritingLocaleToDlcTest::ParamType>&
           info) { return info.param.test_name; });

struct DlcToHandwritingLocaleTestCase {
  std::string test_name;
  std::string_view dlc_id;
  std::optional<std::string> expected;
};

class DlcToHandwritingLocaleTest
    : public HandwritingTest,
      public testing::WithParamInterface<DlcToHandwritingLocaleTestCase> {};

TEST_P(DlcToHandwritingLocaleTest, Test) {
  const DlcToHandwritingLocaleTestCase& test_case = GetParam();

  EXPECT_EQ(DlcToHandwritingLocale(test_case.dlc_id), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    DlcToHandwritingLocaleTests,
    DlcToHandwritingLocaleTest,
    testing::ValuesIn<DlcToHandwritingLocaleTestCase>(
        {{"InvalidEmpty", "", std::nullopt},
         {"InvalidCy", "handwriting-cy", std::nullopt},
         {"InvalidDeDe", "handwriting-de-DE", std::nullopt},
         {"InvalidTypoDe", "handwritting-de", std::nullopt},
         {"InvalidTtsEnUs", "tts-en-us", std::nullopt},
         {"InvalidDeWithoutPrefix", "de", std::nullopt},
         {"ValidDe", "handwriting-de", "de"},
         {"ValidEn", "handwriting-en", "en"},
         {"ValidZhHk", "handwriting-zh-HK", "zh-HK"}}),
    [](const testing::TestParamInfo<DlcToHandwritingLocaleTest::ParamType>&
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
         {"InvalidCy", "handwriting-cy", false},
         {"InvalidDeDe", "handwriting-de-DE", false},
         {"InvalidTypoDe", "handwritting-de", false},
         {"InvalidTtsEnUs", "tts-en-us", false},
         {"InvalidDeWithoutPrefix", "de", false},
         {"ValidDe", "handwriting-de", true},
         {"ValidEn", "handwriting-en", true},
         {"ValidZhHk", "handwriting-zh-HK", true}}),
    [](const testing::TestParamInfo<IsHandwritingDlcTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_F(HandwritingTest, FilterHandwritingDlcsDefault) {
  EXPECT_THAT(
      ConvertDlcsWithContentToHandwritingLocales(dlcservice::DlcsWithContent()),
      IsEmpty());
}

TEST_F(HandwritingTest, FilterHandwritingDlcsNotHandwriting) {
  const dlcservice::DlcsWithContent dlcs_without_handwriting =
      CreateDlcsWithContent({"tts-en-us", "grammar-it"});

  EXPECT_THAT(
      ConvertDlcsWithContentToHandwritingLocales(dlcs_without_handwriting),
      IsEmpty());
}

TEST_F(HandwritingTest, FilterHandwritingDlcsVariousEntries) {
  // "handwriting-cy" refer to Welsh language, which is not a language that is
  // supported in Handwriting.
  const dlcservice::DlcsWithContent dlcs_with_some_handwriting =
      CreateDlcsWithContent({"handwriting-fr", "tts-en-us", "handwriting-it",
                             "grammar-it", "handwriting-cy"});

  EXPECT_THAT(
      ConvertDlcsWithContentToHandwritingLocales(dlcs_with_some_handwriting),
      UnorderedElementsAre("fr", "it"));
}

}  // namespace

}  // namespace ash::language_packs
