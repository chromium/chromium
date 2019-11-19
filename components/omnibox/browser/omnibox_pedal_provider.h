// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_PROVIDER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/strings/utf_offset_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"

class OmniboxPedal;
class AutocompleteProviderClient;

class OmniboxPedalProvider {
 public:
  explicit OmniboxPedalProvider(AutocompleteProviderClient& client);
  ~OmniboxPedalProvider();

  // Returns the Pedal triggered by given |match_text| or nullptr if none
  // trigger.
  OmniboxPedal* FindPedalMatch(const base::string16& match_text) const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalImplementationsTest,
                           ProviderFiltersPedalUpdateChrome);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalImplementationsTest,
                           UnorderedSynonymExpressionsAreConceptMatches);

  // Generate a token sequence for text using internal dictionary & delimiters.
  // Returns empty sequence if any delimited part of text is not in dictionary.
  // Note, the ignore_group is applied to eliminate stop words from output.
  OmniboxPedal::Tokens Tokenize(const base::string16& text) const;

  void LoadPedalConcepts();
  OmniboxPedal::SynonymGroup LoadSynonymGroup(
      const base::Value& group_value) const;

  AutocompleteProviderClient& client_;

  // Contains mapping from well-known identifier to Pedal implementation.
  // Note: since the set is small, we use one map here for simplicity; but if
  // someday there are lots of Pedals, it may make sense to switch this to a
  // vector and index by id separately.  The lookup is needed rarely but
  // iterating over the whole collection happens very frequently, so we should
  // really optimize for iteration (vector), not lookup (map).
  std::unordered_map<OmniboxPedalId, std::unique_ptr<OmniboxPedal>> pedals_;

  // Common words that may be used when typing to trigger Pedals.  All instances
  // of these words are removed from match text when looking for triggers.
  // Therefore no Pedal should have a trigger or synonym group that includes
  // any of these words (as a whole word; substrings are fine).
  OmniboxPedal::SynonymGroup ignore_group_;

  // Map from string token to unique int token identifier.
  std::unordered_map<base::string16, int> dictionary_;

  // This contains all token delimiter characters.  It may be empty, in which
  // case no delimiting takes place (input is treated as raw token sequence).
  base::string16 tokenize_characters_;

  // This serves as an upper bound on the number of tokens we will accept from
  // text before giving up and treating it as non-match for all Pedals.
  size_t max_tokens_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPedalProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_PROVIDER_H_
