// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/locale_util.h"

#include <string_view>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {
namespace {

typedef testing::Test LocaleUtilTest;

TEST_F(LocaleUtilTest, SplitIntoMainAndTail) {
  typedef std::pair<std::string_view, std::string_view> StringPiecePair;

  EXPECT_EQ(StringPiecePair("", ""), SplitIntoMainAndTail(""));
  EXPECT_EQ(StringPiecePair("en", ""), SplitIntoMainAndTail("en"));
  EXPECT_EQ(StringPiecePair("ogard543i", ""),
            SplitIntoMainAndTail("ogard543i"));
  EXPECT_EQ(StringPiecePair("en", "-AU"), SplitIntoMainAndTail("en-AU"));
  EXPECT_EQ(StringPiecePair("es", "-419"), SplitIntoMainAndTail("es-419"));
  EXPECT_EQ(StringPiecePair("en", "-AU-2009"),
            SplitIntoMainAndTail("en-AU-2009"));
}

TEST_F(LocaleUtilTest, ConvertToActualUILocale) {
  std::string locale;

  //---------------------------------------------------------------------------
  // Languages that are enabled as display UI.
  //---------------------------------------------------------------------------
  locale = "en-US";
  bool is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("en-US", locale);

  locale = "it";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("it", locale);

  locale = "fr-FR";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("fr", locale);

  //---------------------------------------------------------------------------
  // Languages that are converted to their fallback version.
  //---------------------------------------------------------------------------

  // All Latin American Spanish languages fall back to "es-419".
  for (const char* es_locale : {"es-AR", "es-CL", "es-CO", "es-CR", "es-HN",
                                "es-MX", "es-PE", "es-US", "es-UY", "es-VE"}) {
    locale = es_locale;
    is_ui = ConvertToActualUILocale(&locale);
    EXPECT_TRUE(is_ui) << es_locale;
#if BUILDFLAG(IS_IOS)
    // iOS uses a different name for es-419 (es-MX).
    EXPECT_EQ("es-MX", locale) << es_locale;
#else
    EXPECT_EQ("es-419", locale) << es_locale;
#endif
  }

  // English falls back to US.
  locale = "en";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
#if BUILDFLAG(IS_APPLE)
  // On Apple platforms, "en" is used instead of "en-US".
  EXPECT_EQ("en", locale);
#else
  EXPECT_EQ("en-US", locale);
#endif

  // All other regional English languages fall back to UK.
  for (const char* en_locale :
       {"en-AU", "en-CA", "en-GB-oxendict", "en-IN", "en-NZ", "en-ZA"}) {
    locale = en_locale;
    is_ui = ConvertToActualUILocale(&locale);
    EXPECT_TRUE(is_ui) << en_locale;
    EXPECT_EQ("en-GB", locale) << en_locale;
  }

  locale = "pt";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
#if BUILDFLAG(IS_IOS)
  // On iOS, "pt" is used instead of "pt-BR".
  EXPECT_EQ("pt", locale);
#else
  EXPECT_EQ("pt-BR", locale);
#endif

  locale = "it-CH";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("it", locale);

  locale = "no";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("nb", locale);

  locale = "it-IT";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("it", locale);

  locale = "de-DE";
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_TRUE(is_ui);
  EXPECT_EQ("de", locale);

//---------------------------------------------------------------------------
// Languages that cannot be used as display UI.
//---------------------------------------------------------------------------
// This only matters for ChromeOS and Windows, as they are the only systems
// where users can set the display UI.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
  locale = "sd";  // Sindhi
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_FALSE(is_ui);

  locale = "ga";  // Irish
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_FALSE(is_ui);

  locale = "ky";  // Kyrgyz
  is_ui = ConvertToActualUILocale(&locale);
  EXPECT_FALSE(is_ui);
#endif
}

}  // namespace
}  // namespace language
