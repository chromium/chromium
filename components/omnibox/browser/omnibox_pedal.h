// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_H_

#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "url/gurl.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
namespace gfx {
struct VectorIcon;
}
#endif

class AutocompleteProviderClient;
class OmniboxEditController;
class OmniboxClient;

// Conceptually, a Pedal is a fixed action that can be taken by the user
// pressing a button or taking a new dedicated suggestion when some
// associated intention is detected in an omnibox match suggestion.
// The typical action is to navigate to an internal Chrome surface
// like settings, but other actions like translation or launching
// an incognito window are possible.  The intention is detected by
// checking trigger queries against suggested match queries.
class OmniboxPedal {
 public:
  typedef std::vector<int> Tokens;

  struct LabelStrings {
    LabelStrings(int id_hint, int id_hint_short, int id_suggestion_contents);
    const base::string16 hint;
    const base::string16 hint_short;
    const base::string16 suggestion_contents;
  };

  class SynonymGroup {
   public:
    // Note: synonyms must be specified in decreasing order by length
    // so that longest matches will be detected first.  For example,
    // "incognito window" must come before "incognito" so that the " window"
    // part will also be covered by this group -- otherwise it would be left
    // intact and wrongly treated as uncovered by the checking algorithm.
    // See OmniboxPedal::IsConceptMatch for the logic that necessitates order.
    SynonymGroup(bool required, bool match_once, size_t reserve_size);
    SynonymGroup(SynonymGroup&&);
    ~SynonymGroup();
    SynonymGroup& operator=(SynonymGroup&&);

    // Removes one or more matching synonyms from given |remaining| sequence if
    // any are found.  Returns true if checking may continue; false if no more
    // checking is required because what remains cannot be a concept match.
    bool EraseMatchesIn(Tokens* remaining) const;

    // Add a synonym token sequence to this group.
    void AddSynonym(Tokens&& synonym);

    // Increase acceptable input size range according to this group's content.
    void UpdateTokenSequenceSizeRange(size_t* out_min, size_t* out_max) const;

   protected:
    // If this is true, a synonym of the group must be present for triggering.
    // If false, then presence is simply allowed and does not inhibit triggering
    // (any text not covered by groups would stop trigger).
    bool required_;

    // If this is true, then only the rightmost instance of first synonym found
    // will be taken from text being checked, and additional instances will
    // inhibit trigger because repetition actually changes meaning.  If false,
    // then all instances of all synonyms are taken (repetition is meaningless).
    bool match_once_;

    // The set of interchangeable alternative representations for this group:
    // when trying to clear browsing data, a user may think of 'erase', 'clear',
    // 'delete', etc.  Even though these are not strictly synonymous in natural
    // language, they are considered equivalent within the context of intention
    // to perform this Pedal's action.
    std::vector<Tokens> synonyms_;

    DISALLOW_COPY_AND_ASSIGN(SynonymGroup);
  };

  // ExecutionContext provides the necessary structure for Pedal
  // execution implementations that potentially vary widely, and
  // references are preferred over pointers for members that are
  // not nullable.  If there's ever a good case for changing to
  // nullable pointers, this can change but for now presence is
  // guaranteed so Pedals can use them without needing to check.
  // The other reason to use a context is that it's easier to
  // maintain with lots of Pedal implementations because the amount
  // of boilerplate required is greatly reduced.
  class ExecutionContext {
   public:
    ExecutionContext(OmniboxClient& client,
                     OmniboxEditController& controller,
                     base::TimeTicks match_selection_timestamp)
        : client_(client),
          controller_(controller),
          match_selection_timestamp_(match_selection_timestamp) {}
    OmniboxClient& client_;
    OmniboxEditController& controller_;
    base::TimeTicks match_selection_timestamp_;
  };

  OmniboxPedal(LabelStrings strings, GURL url);
  virtual ~OmniboxPedal();

  // Provides read access to labels associated with this Pedal.
  const LabelStrings& GetLabelStrings() const;

  // Returns true if this is purely a navigation Pedal with URL.
  bool IsNavigation() const;

  // For navigation Pedals, returns the destination URL.
  const GURL& GetNavigationUrl() const;

  // Takes the action associated with this Pedal.  Non-navigation
  // Pedals must override the default, but Navigation Pedals don't need to.
  virtual void Execute(ExecutionContext& context) const;

  // Returns true if this Pedal is ready to be used now, or false if
  // it does not apply under current conditions. (Example: the UpdateChrome
  // Pedal may not be ready to trigger if no update is available.)
  virtual bool IsReadyToTrigger(const AutocompleteProviderClient& client) const;

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  // Returns the vector icon to represent this Pedal's action in suggestion.
  virtual const gfx::VectorIcon& GetVectorIcon() const;
#endif

  // Returns true if the preprocessed match suggestion sequence triggers
  // presentation of this Pedal.  This is not intended for general use,
  // and only OmniboxPedalProvider should need to call this method.
  bool IsTriggerMatch(const Tokens& match_sequence) const;

  // Move a synonym group into this Pedal's collection.
  void AddSynonymGroup(SynonymGroup&& group);

 protected:
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalTest, SynonymGroupErasesFirstMatchOnly);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalTest, SynonymGroupsDriveConceptMatches);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalImplementationsTest,
                           UnorderedSynonymExpressionsAreConceptMatches);

  // If a sufficient set of triggering synonym groups are present in
  // match_sequence then it's a concept match and this returns true.  If a
  // required group is not present, or if match_sequence contains extraneous
  // tokens not covered by any synonym group, then it's not a concept match and
  // this returns false.
  bool IsConceptMatch(const Tokens& match_sequence) const;

  // Use this for the common case of navigating to a URL.
  void OpenURL(ExecutionContext& context, const GURL& url) const;

  std::vector<SynonymGroup> synonym_groups_;
  LabelStrings strings_;

  // For navigation Pedals, this holds the destination URL; for action Pedals,
  // this remains empty.
  GURL url_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_H_
