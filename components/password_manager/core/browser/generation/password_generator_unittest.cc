// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/generation/password_generator.h"

#include <cstdint>
#include <string>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// These are strings instead of enums to have an easy way of logging them.
constexpr char kLowerCase[] = "lower_case";
constexpr char kUpperCase[] = "upper_case";
constexpr char kAlphabetic[] = "alphabetic";
constexpr char kNumeric[] = "numeric";
constexpr char kSymbol[] = "symbol";

constexpr const char* kAllClassesButSymbols[] = {kLowerCase, kUpperCase,
                                                 kAlphabetic, kNumeric};
constexpr const char* kAllClassesButSymbolsAndAlphabetic[] = {
    kLowerCase, kUpperCase, kNumeric};

bool IsCharInClass(char16_t c, const std::string& class_name) {
  if (class_name == kLowerCase) {
    return 'a' <= c && c <= 'z';
  } else if (class_name == kUpperCase) {
    return 'A' <= c && c <= 'Z';
  } else if (class_name == kAlphabetic) {
    return IsCharInClass(c, kLowerCase) || IsCharInClass(c, kUpperCase);
  } else if (class_name == kNumeric) {
    return '0' <= c && c <= '9';
  }
  // Symbols are not covered because there is not fixed definition and because
  // symbols are treated like other character classes, so the importance of
  // dealing with them here is limited.
  NOTREACHED_IN_MIGRATION() << "Don't call IsCharInClass for symbols";
  return false;
}

size_t CountCharsInClass(const std::u16string& password,
                         const std::string& class_name) {
  return base::ranges::count_if(password, [&class_name](char16_t c) {
    return IsCharInClass(c, class_name);
  });
}

PasswordRequirementsSpec_CharacterClass* GetMutableCharClass(
    PasswordRequirementsSpec* spec,
    const std::string& class_name) {
  if (class_name == kLowerCase) {
    return spec->mutable_lower_case();
  } else if (class_name == kUpperCase) {
    return spec->mutable_upper_case();
  } else if (class_name == kAlphabetic) {
    return spec->mutable_alphabetic();
  } else if (class_name == kNumeric) {
    return spec->mutable_numeric();
  } else if (class_name == kSymbol) {
    return spec->mutable_symbols();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

class PasswordGeneratorTest : public testing::Test {
 public:
  PasswordGeneratorTest() { spec_.set_spec_version(1); }
  ~PasswordGeneratorTest() override = default;

  PasswordRequirementsSpec spec_;
};

TEST_F(PasswordGeneratorTest, PasswordLengthDefault) {
  EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, ConditionallyAddNumericDigitsToAlphabet) {
  // Check if '0' and '1' is added to the numeric alphabet if the password
  // does not contain letters.
  spec_.mutable_lower_case()->set_max(0);
  spec_.mutable_upper_case()->set_max(0);
  ConditionallyAddNumericDigitsToAlphabet(&spec_);
  EXPECT_TRUE(spec_.numeric().character_set().find('0') != std::string::npos);
  EXPECT_TRUE(spec_.numeric().character_set().find('1') != std::string::npos);
}

TEST_F(PasswordGeneratorTest, ConditionallyDoNotAddNumericDigitsToAlphabet) {
  // Check if '0' and '1' is not added to the numeric alphabet if the password
  // does contain letters lower and/or upper case letters.

  // Check for only lower case letters.
  spec_.mutable_lower_case()->set_max(1);
  spec_.mutable_upper_case()->set_max(0);
  ConditionallyAddNumericDigitsToAlphabet(&spec_);
  EXPECT_TRUE(spec_.numeric().character_set().find('0') == std::string::npos);
  EXPECT_TRUE(spec_.numeric().character_set().find('1') == std::string::npos);

  // Check for only upper case letters.
  spec_.mutable_lower_case()->set_max(0);
  spec_.mutable_upper_case()->set_max(1);
  ConditionallyAddNumericDigitsToAlphabet(&spec_);
  EXPECT_TRUE(spec_.numeric().character_set().find('0') == std::string::npos);
  EXPECT_TRUE(spec_.numeric().character_set().find('1') == std::string::npos);

  // Mixed case with lower and upper case letters.
  spec_.mutable_lower_case()->set_max(1);
  spec_.mutable_upper_case()->set_max(1);
  ConditionallyAddNumericDigitsToAlphabet(&spec_);
  EXPECT_TRUE(spec_.numeric().character_set().find('0') == std::string::npos);
  EXPECT_TRUE(spec_.numeric().character_set().find('1') == std::string::npos);
}

TEST_F(PasswordGeneratorTest, PasswordLengthMaxLength) {
  // Limit length according to requirement.
  spec_.set_max_length(kDefaultPasswordLength - 5u);
  EXPECT_EQ(kDefaultPasswordLength - 5u, GeneratePassword(spec_).length());

  // If max is higher than default, it does not matter.
  spec_.set_max_length(kDefaultPasswordLength + 5);
  EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, PasswordLengthMinLength) {
  // If min is smaller than default, it does not matter.
  spec_.set_min_length(kDefaultPasswordLength - 5u);
  EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());

  // If a higher minimum length is explicitly set, use it.
  spec_.set_min_length(kDefaultPasswordLength + 5u);
  EXPECT_EQ(kDefaultPasswordLength + 5u, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, PasswordLengthMinAndMax) {
  // Configure a contradicting min and max length. The max length wins.
  spec_.set_min_length(kDefaultPasswordLength + 5u);
  spec_.set_max_length(kDefaultPasswordLength - 5u);
  EXPECT_EQ(kDefaultPasswordLength - 5u, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, MinCharFrequenciesRespected) {
  for (std::string char_class : kAllClassesButSymbols) {
    SCOPED_TRACE(char_class);
    spec_ = PasswordRequirementsSpec();
    spec_.set_spec_version(1);
    auto* char_class_config = GetMutableCharClass(&spec_, char_class);
    char_class_config->set_min(10);
    char_class_config->set_max(1000);

    std::u16string password = GeneratePassword(spec_);
    EXPECT_GE(CountCharsInClass(password, char_class), 10u);
  }
}

TEST_F(PasswordGeneratorTest, MinCharFrequenciesInsane) {
  // Nothing breaks if the min frequencies are way beyond what's possible
  // with the password length. In this case the generated password may contain
  // just characters of one class but its target length does not increase.
  for (std::string char_class : kAllClassesButSymbols) {
    SCOPED_TRACE(char_class);
    spec_ = PasswordRequirementsSpec();
    spec_.set_spec_version(1);
    auto* char_class_config = GetMutableCharClass(&spec_, char_class);
    char_class_config->set_min(1000);
    char_class_config->set_max(1000);

    std::u16string password = GeneratePassword(spec_);
    // At least some of the characters should be there.
    EXPECT_GE(CountCharsInClass(password, char_class), 1u);
    // But the password length does not change by minimum length requirements.
    EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
  }
}

TEST_F(PasswordGeneratorTest, MinCharFrequenciesBiggerThanMax) {
  spec_.set_min_length(15);
  spec_.set_max_length(15);
  for (std::string char_class : kAllClassesButSymbolsAndAlphabetic) {
    auto* char_class_config = GetMutableCharClass(&spec_, char_class);
    // Min is reduced to max --> each class should have 5 representatives.
    char_class_config->set_min(10);
    char_class_config->set_max(5);
  }

  std::u16string password = GeneratePassword(spec_);

  for (std::string char_class : kAllClassesButSymbolsAndAlphabetic) {
    SCOPED_TRACE(char_class);
    // Check that each character class is represented by 5 characters.
    EXPECT_EQ(5u, CountCharsInClass(password, char_class));
  }
  EXPECT_EQ(15u, password.length());
}

TEST_F(PasswordGeneratorTest, MaxFrequenciesRespected) {
  // Limit the maximum occurrences of characters of some classes to 2 and
  // validate that this is respected.
  for (std::string char_class : kAllClassesButSymbolsAndAlphabetic) {
    SCOPED_TRACE(char_class);
    spec_ = PasswordRequirementsSpec();
    spec_.set_spec_version(1);
    auto* char_class_config = GetMutableCharClass(&spec_, char_class);
    char_class_config->set_max(2);

    std::u16string password = GeneratePassword(spec_);
    EXPECT_LE(CountCharsInClass(password, char_class), 2u);
    // Ensure that the other character classes that are not limited fill up the
    // password to the desired length.
    EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
  }
}

TEST_F(PasswordGeneratorTest, MaxFrequenciesInsufficient) {
  spec_.set_min_length(15);
  spec_.set_max_length(15);
  // Limit the maximum occurrences of characters of all classes to 2 which
  // sums up to less than 15.
  for (std::string char_class : kAllClassesButSymbolsAndAlphabetic) {
    auto* char_class_config = GetMutableCharClass(&spec_, char_class);
    char_class_config->set_max(2);
  }
  // The resulting password can contain only 6 characters.
  EXPECT_EQ(6u, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, CharacterSetCanBeOverridden) {
  // Limit lower case chars to 'a' and 'b' and require exacty 5 of those.
  spec_.mutable_lower_case()->set_character_set("ab");
  spec_.mutable_lower_case()->set_min(5);
  spec_.mutable_lower_case()->set_max(5);
  std::u16string password = GeneratePassword(spec_);
  // Then ensure that 'a's and 'b's are generated at the expected frequencies
  // as an indicator that the override was respected.
  size_t num_as_and_bs = 0;
  for (char16_t c : password) {
    if (c == 'a' || c == 'b') {
      ++num_as_and_bs;
    }
  }
  EXPECT_EQ(5u, num_as_and_bs);
}

TEST_F(PasswordGeneratorTest, AllCharactersAreGenerated) {
  // Limit lower case chars to 'a' and 'b' and require exacty 5 of those.
  spec_.mutable_lower_case()->set_character_set("ab");
  spec_.mutable_lower_case()->set_min(5);
  spec_.mutable_lower_case()->set_max(5);
  bool success = false;
  // The chance of not seing both an 'a' and a 'b' in one run are only 2/32 per
  // run. Ultimately we should see success. If none of the 100 generated
  // passwords contained an 'a' (or 'b'), then this would indicate that the
  // generator does not fully use the character set (probably due to an
  // off-by-one error).
  for (size_t attempt = 0; attempt < 100; ++attempt) {
    std::u16string password = GeneratePassword(spec_);
    size_t num_as = 0;
    size_t num_bs = 0;
    for (char16_t c : password) {
      if (c == 'a') {
        ++num_as;
      }
      if (c == 'b') {
        ++num_bs;
      }
    }
    if (num_as > 0u && num_bs > 0u) {
      success = true;
      break;
    }
  }
  EXPECT_TRUE(success);
}

TEST_F(PasswordGeneratorTest, PasswordCanBeGeneratedWithEmptyCharSet) {
  // If the character set is empty, min and max should be ignored.
  spec_.mutable_lower_case()->set_character_set("");
  spec_.mutable_lower_case()->set_min(5);
  spec_.mutable_lower_case()->set_max(5);
  std::u16string password = GeneratePassword(spec_);
  EXPECT_EQ(0u, CountCharsInClass(password, kLowerCase));
  EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, AllCharactersForbidden) {
  spec_.set_min_length(kDefaultPasswordLength + 2);
  spec_.set_max_length(kDefaultPasswordLength + 2);
  for (std::string char_class : kAllClassesButSymbolsAndAlphabetic) {
    auto* char_class_config = GetMutableCharClass(&spec_, char_class);
    char_class_config->set_max(0);
  }
  // If it is impossible to generate a password (due to max = 0), the generator
  // delivers a password as per default spec and ignores everything else.
  EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
}

TEST_F(PasswordGeneratorTest, ZeroLength) {
  spec_.set_min_length(0);
  spec_.set_max_length(0);
  // If the generated password following the spec is empty, a default spec is
  // applied.
  EXPECT_EQ(kDefaultPasswordLength, GeneratePassword(spec_).length());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
class PasswordGeneratorChunkingTest : public testing::Test {
 public:
  PasswordGeneratorChunkingTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kPasswordGenerationChunking);
  }
  ~PasswordGeneratorChunkingTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PasswordGeneratorChunkingTest, ChunkedWithDifferentMaxLengths) {
  PasswordRequirementsSpec spec;
  spec.set_min_length(0);
  spec.mutable_symbols()->set_character_set("-");

  for (uint32_t max_length = 9; max_length <= kDefaultPasswordLength;
       max_length++) {
    spec.set_max_length(max_length);
    std::u16string password = GeneratePassword(spec);

    // Check every 5th char is a dash except when it's a last char.
    for (uint32_t i = 4; i < max_length; i += 5) {
      if (i < max_length - 1) {
        EXPECT_EQ(password[i], '-');
      }
    }
    EXPECT_NE(password[max_length - 1], '-');
  }
}

TEST_F(PasswordGeneratorChunkingTest, DashDisallowed) {
  PasswordRequirementsSpec spec;
  spec.set_min_length(0);
  spec.set_max_length(kDefaultPasswordLength);
  spec.mutable_symbols()->set_character_set("");

  EXPECT_TRUE(GeneratePassword(spec).find('-') == std::string::npos);
}

TEST_F(PasswordGeneratorChunkingTest, TooShortMaxLength) {
  PasswordRequirementsSpec spec;
  spec.set_min_length(0);
  spec.set_max_length(8);
  spec.mutable_symbols()->set_character_set("-");

  EXPECT_TRUE(GeneratePassword(spec).find('-') == std::string::npos);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

}  // namespace autofill
