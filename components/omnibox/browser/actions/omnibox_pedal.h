// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_H_

#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/buildflags.h"
#include "url/gurl.h"

// Conceptually, a Pedal is a fixed action that can be taken by the user
// pressing a button or taking a new dedicated suggestion when some
// associated intention is detected in an omnibox match suggestion.
// The typical action is to navigate to an internal Chrome surface
// like settings, but other actions like translation or launching
// an incognito window are possible.  The intention is detected by
// checking trigger queries against suggested match queries.
class OmniboxPedal : public OmniboxAction {
 public:
  struct Token {
    // Token identifier from the common token dictionary.
    int id;

    // Index of the next unconsumed token node. Initially this is set to
    // the token's own index, indicating this token is unconsumed. Calls
    // to |TokenSequence::Consume| may then update it to greater values,
    // indicating that this token is consumed.
    size_t link;
  };

  // This is a specialized container for the sequence matching algorithm.
  // It is intended only for single-threaded access by OmniboxPedal classes.
  class TokenSequence {
   public:
    // Construct with reserved size; used when loading real data.
    explicit TokenSequence(size_t reserve_size);

    // Construct with given sequence of |token_ids|; used by tests.
    explicit TokenSequence(std::vector<int> token_ids);

    // Don't use copies. They were necessary with old algorithm,
    // but this structure is amenable to efficient resets on kept instances.
    TokenSequence(const TokenSequence&) = delete;
    TokenSequence& operator=(const TokenSequence&) = delete;
    TokenSequence(TokenSequence&&);
    TokenSequence& operator=(TokenSequence&&);
    ~TokenSequence();

    // Returns true if all tokens are consumed (true for empty sequences).
    bool IsFullyConsumed();

    // Returns the number of unconsumed tokens remaining. Used by tests.
    size_t CountUnconsumed() const;

    // Add token with given |id| to sequence.
    void Add(int id);

    // Clears all tokens from this sequence.
    inline void Clear() { tokens_.clear(); }

    // Initializes all links in sequence to their own index, indicating
    // unconsumed state for all. This is needed after calls to Erase.
    void ResetLinks();

    // Removes one or more instances of |erase_sequence| from this sequence
    // by erasing token items from the |tokens_| container.
    // Returns true if this sequence was changed; false if no match is found.
    bool Erase(const TokenSequence& erase_sequence, bool erase_only_once);

    // Consumes one or more instances of |consume_sequence| from this sequence
    // by updating links on matching tokens. The container is not modified,
    // only its contained elements may be mutated.
    // Returns true if matches were found and consumed; false if no match.
    bool Consume(const TokenSequence& consume_sequence, bool consume_only_once);

    // Returns the total number of tokens, regardless of consumed status.
    inline size_t Size() const { return tokens_.size(); }

    // Returns collection memory estimate for tracing.
    size_t EstimateMemoryUsage() const;

   private:
    // Returns true if this sequence, starting at |index|, matches given
    // |sequence|. The |index_mask| can be used to disregard consumed status (0)
    // or require that all tokens must be unconsumed to match (~0).
    bool MatchesAt(const TokenSequence& sequence,
                   size_t index,
                   size_t index_mask) const;

    // Follows links on tokens starting at |from_index| and returns the first
    // unconsumed index. Returns |Size()| if no unconsumed token is found.
    size_t WalkToUnconsumedIndexFrom(size_t from_index);

    // Storage for tokens.
    std::vector<Token> tokens_;
  };

  struct SynonymGroupSpec {
    bool required;
    bool match_once;
    int message_id;
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
    SynonymGroup(const SynonymGroup&) = delete;
    SynonymGroup& operator=(const SynonymGroup&) = delete;
    SynonymGroup& operator=(SynonymGroup&&);

    // Removes one or more matching synonyms from given |remaining| sequence if
    // any are found.  Returns true if checking may continue; false if no more
    // checking is required because what remains cannot be a concept match.
    // Note, if |fully_erase| is true and this method returns true, the
    // |remaining| container has changed structure so ResetLinks must be called.
    // This method doesn't call ResetLinks in that case, for efficiency.
    bool EraseMatchesIn(TokenSequence& remaining, bool fully_erase) const;

    // Add a synonym token sequence to this group.
    void AddSynonym(TokenSequence synonym);

    // When runtime data was preprocessed by pedal_processor,
    // it avoided the need to sort at runtime in Chromium, but with
    // the TC-based l10n technique, data loading needs to be robust
    // enough to handle various forms and orders in translation data.
    // Hence, a call to `SortSynonyms` is required after all calls
    // to `AddSynonym` are complete. We may eliminate this step
    // if we implement post-processing of the .xtb translation files.
    void SortSynonyms();

    // Estimates RAM usage in bytes for this synonym group.
    size_t EstimateMemoryUsage() const;

    // Erases sequences in ignore group from all synonyms in this group.
    void EraseIgnoreGroup(const SynonymGroup& ignore_group);

    // Returns true if this synonym group contains nontrivial data that can
    // be used by the matching algorithm.
    bool IsValid() const;

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
    std::vector<TokenSequence> synonyms_;
  };

  OmniboxPedal(OmniboxPedalId id, LabelStrings strings, GURL url);

  // Downcasts the given OmniboxAction to an OmniboxPedal if the supplied
  // instance represents one, otherwise returns nullptr.
  static const OmniboxPedal* FromAction(const OmniboxAction* action);

  // Called after the OmniboxPedalProvider finishes loading all pedals data.
  // This can be used to override implementation bits based on flags, etc.
  virtual void OnLoaded();

  // Sets the destination URL for the Pedal.
  void SetNavigationUrl(const GURL& url);

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  // Returns the default vector icon to use for Pedals that do not specify one.
  static const gfx::VectorIcon& GetDefaultVectorIcon();
#endif

  // Add a verbatim token sequence for direct matching.
  // This can improve user experience of omnibox pedals by ensuring that
  // button text entered verbatim is always a sufficient trigger. Since
  // button text labels are not always within the specified set of triggers,
  // it may be possible to discover a pedal, memorize the button
  // label, and then go seeking it out again with the button label, but
  // not find it. With the verbatim sequence, learning a pedal by label will
  // always make the pedal available with that exact input (ignoring case and
  // the common ignore group).
  void AddVerbatimSequence(TokenSequence sequence);

  // Move a synonym group into this Pedal's collection.
  void AddSynonymGroup(SynonymGroup&& group);

  // Specify synonym groups to load from localization strings.
  // `locale_is_english` provides a hint about which locale is being loaded,
  // used to support both synonym-groups and whole-phrase localization.
  virtual std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const;

  OmniboxPedalId PedalId() const { return id_; }

  // Sometimes pedals report different IDs for metrics, either to enable
  // feature discrimination (e.g. incognito mode) or to unify metrics
  // of closely related pedals (e.g. a ChromeOS specialization of a pedal).
  virtual OmniboxPedalId GetMetricsId() const;

  // If a sufficient set of triggering synonym groups are present in
  // match_sequence then it's a concept match and this returns true.  If a
  // required group is not present, or if match_sequence contains extraneous
  // tokens not covered by any synonym group, then it's not a concept match and
  // this returns false. |match_sequence| is consumed/mutated by this method.
  bool IsConceptMatch(TokenSequence& match_sequence) const;

  // OmniboxAction overrides:
  void RecordActionShown(size_t position, bool executed) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif
  size_t EstimateMemoryUsage() const override;
  OmniboxActionId ActionId() const override;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif

 protected:
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalTest, SynonymGroupErasesFirstMatchOnly);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalTest, SynonymGroupsDriveConceptMatches);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalTest,
                           VerbatimSynonymGroupDrivesConceptMatches);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPedalImplementationsTest,
                           UnorderedSynonymExpressionsAreConceptMatches);

  ~OmniboxPedal() override;

  OmniboxPedalId id_;

  // Before standard synonym group matching, we can check the verbatim
  // syonym group for direct matches, e.g. with the button label text.
  SynonymGroup verbatim_synonym_group_;

  std::vector<SynonymGroup> synonym_groups_;
};

// This is a simple pedal suitable only for use by tests.
class TestOmniboxPedalClearBrowsingData : public OmniboxPedal {
 public:
  explicit TestOmniboxPedalClearBrowsingData();

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override;

 protected:
  ~TestOmniboxPedalClearBrowsingData() override = default;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_H_
