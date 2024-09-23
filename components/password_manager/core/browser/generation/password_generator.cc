// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/generation/password_generator.h"

#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"

namespace autofill {

// The default length for a generated password. Keep this the same as the
// default length known to the classification pipeline on the autofill
// crowd-sourcing server. (The server predicts password lengths only if the
// prediction is smaller than the default.)
const uint32_t kDefaultPasswordLength = 15;

// The minimum length to chunk password with
// `password_manager::features::PasswordGenerationChunking` feature.
const uint32_t kMinLengthToChunkPassword = 9;

namespace {

// Default character sets used if the spec does not override the character set.
// Removed characters due to visual similarity:
// - l (lowercase L)
// - I (capital i)
// - 1 (one)
// - O (capital o)
// - 0 (zero)
// - o (lowercase O)
constexpr char kLowerCaseChars[] = "abcdefghijkmnpqrstuvwxyz";
constexpr char kUpperCaseChars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
constexpr char kAlphabeticChars[] =
    "abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ";
constexpr char kDigits[] = "23456789";
constexpr char kSymbols[] = "-_.:!";

// Returns a default password requirements specification that requires:
// - at least one lower case letter
// - at least one upper case letter
// - at least one number
// - no symbols
PasswordRequirementsSpec BuildDefaultSpec() {
  // Note the the fields below should be initialized in the order of their
  // proto field numbers to reduce the risk of forgetting a field.
  PasswordRequirementsSpec spec;
  spec.set_priority(0);
  spec.set_spec_version(1);
  // spec.min_length and spec.max_length remain unset to fall back to
  // PasswordGenerator::kDefaultPasswordLength.

  spec.mutable_lower_case()->set_character_set(kLowerCaseChars);
  spec.mutable_lower_case()->set_min(1);
  spec.mutable_lower_case()->set_max(std::numeric_limits<int32_t>::max());

  spec.mutable_upper_case()->set_character_set(kUpperCaseChars);
  spec.mutable_upper_case()->set_min(1);
  spec.mutable_upper_case()->set_max(std::numeric_limits<int32_t>::max());

  spec.mutable_alphabetic()->set_character_set(kAlphabeticChars);
  spec.mutable_alphabetic()->set_min(0);
  spec.mutable_alphabetic()->set_max(0);

  spec.mutable_numeric()->set_character_set(kDigits);
  spec.mutable_numeric()->set_min(1);
  spec.mutable_numeric()->set_max(std::numeric_limits<int32_t>::max());

  spec.mutable_symbols()->set_character_set(kSymbols);
  spec.mutable_symbols()->set_min(0);
  spec.mutable_symbols()->set_max(0);
  return spec;
}

// Returns whether the password is difficult to read because it contains
// sequences of '-' or '_' that are joined into long strokes on the screen
// in many fonts.
bool IsDifficultToRead(const std::u16string& password) {
  return base::ranges::adjacent_find(password, [](auto a, auto b) {
           return a == b && (a == '-' || a == '_');
         }) != password.end();
}

bool ChunkingPasswordEnabled() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordGenerationChunking);
#else
  return false;
#endif
}

// Generates a password according to |spec| and tries to maximize the entropy
// while not caring for pronounceable passwords.
//
// |spec| must contain values for at least all fields that are defined
// in the spec of BuildDefaultSpec().
std::u16string GenerateMaxEntropyPassword(PasswordRequirementsSpec spec) {
  using CharacterClass = PasswordRequirementsSpec_CharacterClass;

  // Determine target length.
  uint32_t target_length = kDefaultPasswordLength;
  if (spec.has_min_length()) {
    target_length = std::max(target_length, spec.min_length());
  }
  if (spec.has_max_length()) {
    target_length = std::min(target_length, spec.max_length());
  }
  // Avoid excessively long passwords.
  target_length = std::min(target_length, 200u);

  // The password that is being generated in this function.
  std::u16string password;
  password.reserve(target_length);

  // A list of CharacterClasses that have not been fully used.
  std::vector<CharacterClass*> classes;
  // The list of allowed characters in a specific class. This map exists
  // to calculate the string16 conversion only once.
  std::map<CharacterClass*, std::u16string> characters_of_class;

  // These are guaranteed to exist because |spec| is an overlay of the default
  // spec.
  DCHECK(spec.has_lower_case());
  DCHECK(spec.has_upper_case());
  DCHECK(spec.has_alphabetic());
  DCHECK(spec.has_numeric());
  DCHECK(spec.has_symbols());

  // Initialize |classes| and |characters_of_class| and sanitize |spec|
  // if necessary.
  for (CharacterClass* character_class :
       {spec.mutable_lower_case(), spec.mutable_upper_case(),
        spec.mutable_alphabetic(), spec.mutable_numeric(),
        spec.mutable_symbols()}) {
    DCHECK(character_class->has_character_set());
    DCHECK(character_class->has_min());
    DCHECK(character_class->has_max());

    // If the character set is empty, we cannot generate characters from it.
    if (character_class->character_set().empty()) {
      character_class->set_max(0);
    }

    // The the maximum is smaller than the minimum, limit the minimum.
    if (character_class->max() < character_class->min()) {
      character_class->set_min(character_class->max());
    }

    if (character_class->max() > 0) {
      classes.push_back(character_class);
      characters_of_class[character_class] =
          base::UTF8ToUTF16(character_class->character_set());
    }
  }

  // Generate a password that contains the minimum number of characters of the
  // various character classes as per requirements. This stops when the target
  // length is achieved. Note that this is just a graceful handling of a buggy
  // spec. It should not happen that more characters are needed than can
  // accommodated.
  for (CharacterClass* character_class : classes) {
    while (character_class->min() > 0 && password.length() < target_length) {
      const std::u16string& possible_chars =
          characters_of_class[character_class];
      password += possible_chars[base::RandGenerator(possible_chars.length())];
      character_class->set_min(character_class->min() - 1);
      character_class->set_max(character_class->max() - 1);
    }
  }

  // Now fill the rest of the password with random characters.
  while (password.length() < target_length) {
    // Determine how many different characters are in all remaining character
    // classes.
    size_t number_of_possible_chars = 0;
    for (CharacterClass* character_class : classes) {
      if (character_class->max() > 0) {
        number_of_possible_chars +=
            characters_of_class[character_class].length();
      }
    }
    if (number_of_possible_chars == 0) {
      break;
    }
    uint64_t choice = base::RandGenerator(number_of_possible_chars);
    // Now figure out which character was chosen and append it.
    for (CharacterClass* character_class : classes) {
      if (character_class->max() > 0) {
        size_t size_of_class = characters_of_class[character_class].length();
        if (choice < size_of_class) {
          password += characters_of_class[character_class][choice];
          character_class->set_max(character_class->max() - 1);
          break;
        } else {
          choice -= size_of_class;
        }
      }
    }
  }

  // So far the password contains the minimally required characters at the
  // the beginning. Therefore, we create a random permutation.
  // TODO(crbug.com/41391422): Once the unittests allow controlling the
  // generated string, test that '--' and '__' are eliminated.
  int remaining_attempts = 5;
  do {
    base::RandomShuffle(password.begin(), password.end());
  } while (IsDifficultToRead(password) && remaining_attempts-- > 0);

  return password;
}

// Generates a max entropy password with a dash symbol every 4th character by
// modifying `spec` in the following way:
// * max_length() is reduced to make space for dashes,
// * symbols() are removed to not conflict visually with the added dashes.
//
// If the modified `spec` contains all values for the required fields, then we
// insert dash every 4th character. Otherwise, the password using default spec
// is returned.
std::u16string GenerateMaxEntropyChunkedPassword(
    PasswordRequirementsSpec spec) {
  // Disallow symbols so they do not conflict visually with the added dashes.
  PasswordRequirementsSpec modified_spec = spec;
  modified_spec.mutable_symbols()->set_min(0);
  modified_spec.mutable_symbols()->set_max(0);
  modified_spec.mutable_symbols()->mutable_character_set()->clear();
  // Generate max entropy password without dashes.
  int number_of_dashes = std::ceil(modified_spec.max_length() / 5.0) - 1;
  modified_spec.set_max_length(modified_spec.max_length() - number_of_dashes);

  std::u16string password =
      GenerateMaxEntropyPassword(std::move(modified_spec));

  // Catch cases where the modified spec is infeasible.
  if (password.empty()) {
    return GenerateMaxEntropyPassword(std::move(spec));
  }

  // Add dash every 4th character.
  for (int i = 0; i < number_of_dashes; i++) {
    password.insert((i + 1) * 4 + i, u"-");
  }
  return password;
}

}  // namespace

void ConditionallyAddNumericDigitsToAlphabet(PasswordRequirementsSpec* spec) {
  DCHECK(spec);
  if (spec->lower_case().max() == 0 && spec->upper_case().max() == 0) {
    spec->mutable_numeric()->mutable_character_set()->append("01");
  }
}

std::u16string GeneratePassword(const PasswordRequirementsSpec& spec) {
  PasswordRequirementsSpec actual_spec = BuildDefaultSpec();

  // Override all fields that are set in |spec|. Character classes are merged
  // recursively.
  actual_spec.MergeFrom(spec);

  // For passwords without letters, add the '0' and '1' to the numeric alphabet.
  ConditionallyAddNumericDigitsToAlphabet(&actual_spec);

  std::u16string password;

  // For specs that allow dash symbol and can be longer than 8 chars generate a
  // chunked password with `PasswordGenerationChunking` feature enabled.
  if (actual_spec.symbols().character_set().find('-') != std::string::npos &&
      actual_spec.max_length() >= kMinLengthToChunkPassword &&
      ChunkingPasswordEnabled()) {
    password = GenerateMaxEntropyChunkedPassword(std::move(actual_spec));
    CHECK_LE(4u, password.size());
    return password;
  }

  password = GenerateMaxEntropyPassword(std::move(actual_spec));

  // Catch cases where supplied spec is infeasible.
  // TODO(b/40065733): we should never generate specs for small generated
  // passwords
  if (password.size() < 4) {
    password = GenerateMaxEntropyPassword(BuildDefaultSpec());
  }

  CHECK_LE(4u, password.size());
  return password;
}

}  // namespace autofill
