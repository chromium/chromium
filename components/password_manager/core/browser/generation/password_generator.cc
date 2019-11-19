// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/generation/password_generator.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"

namespace autofill {

// The default length for a generated password. Keep this the same as the
// default length known to the classification pipeline on the autofill
// crowd-sourcing server. (The server predicts password lengths only if the
// prediction is smaller than the default.)
const uint32_t kDefaultPasswordLength = 15;

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
bool IsDifficultToRead(const base::string16& password) {
  return std::adjacent_find(password.begin(), password.end(),
                            [](auto a, auto b) {
                              return a == b && (a == '-' || a == '_');
                            }) != password.end();
}

// Generates a password according to |spec| and tries to maximze the entropy
// while not caring for pronounceable passwords.
//
// |spec| must contain values for at least all fields that are defined
// in the spec of BuildDefaultSpec().
base::string16 GenerateMaxEntropyPassword(PasswordRequirementsSpec spec) {
  using CharacterClass = PasswordRequirementsSpec_CharacterClass;

  // Determine target length.
  uint32_t target_length = kDefaultPasswordLength;
  if (spec.has_min_length())
    target_length = std::max(target_length, spec.min_length());
  if (spec.has_max_length())
    target_length = std::min(target_length, spec.max_length());
  // Avoid excessively long passwords.
  target_length = std::min(target_length, 200u);

  // The password that is being generated in this function.
  base::string16 password;
  password.reserve(target_length);

  // A list of CharacterClasses that have not been fully used.
  std::vector<CharacterClass*> classes;
  // The list of allowed characters in a specific class. This map exists
  // to calculate the string16 conversion only once.
  std::map<CharacterClass*, base::string16> characters_of_class;

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
    if (character_class->character_set().empty())
      character_class->set_max(0);

    // The the maximum is smaller than the minimum, limit the minimum.
    if (character_class->max() < character_class->min())
      character_class->set_min(character_class->max());

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
      const base::string16& possible_chars =
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
    if (number_of_possible_chars == 0)
      break;
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
  // TODO(crbug.com/847200): Once the unittests allow controlling the generated
  // string, test that '--' and '__' are eliminated.
  int remaining_attempts = 5;
  do {
    base::RandomShuffle(password.begin(), password.end());
  } while (IsDifficultToRead(password) && remaining_attempts-- > 0);

  return password;
}

}  // namespace

base::string16 GeneratePassword(const PasswordRequirementsSpec& spec) {
  PasswordRequirementsSpec actual_spec = BuildDefaultSpec();

  // Override all fields that are set in |spec|. Character classes are merged
  // recursively.
  actual_spec.MergeFrom(spec);

  base::string16 password = GenerateMaxEntropyPassword(std::move(actual_spec));

  // Catch cases where supplied spec is infeasible.
  if (password.empty())
    password = GenerateMaxEntropyPassword(BuildDefaultSpec());

  return password;
}

}  // namespace autofill
