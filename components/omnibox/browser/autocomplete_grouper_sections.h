// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_

#include <memory>
#include <vector>

#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_group_util.h"

class Section;
using Groups = std::vector<Group>;
using PSections = std::vector<std::unique_ptr<Section>>;

// `Section` class and subclasses used to implement the various autocomplete
// grouping algorithms.

// Section containing no `Groups` and, therefore, no matches.
class Section {
 public:
  explicit Section(size_t limit,
                   Groups groups,
                   omnibox::GroupConfigMap& group_configs,
                   omnibox::GroupConfig_SideType side_type);
  virtual ~Section();
  // Returns `matches` ranked and culled according to `sections`. All `matches`
  // should have `suggestion_group_id` set and be sorted by relevance.
  static ACMatches GroupMatches(PSections sections, ACMatches& matches);
  // Used to adjust this `Section`'s and its `Group`s' total limits.
  virtual void InitFromMatches(ACMatches& matches) {}

 protected:
  // Returns the first `Group` in this `Section` `match` can be added to or
  // `groups_.end()` if none can be found. Does not take the total limit into
  // account.
  Groups::iterator FindGroup(const AutocompleteMatch& match);
  // Returns whether `match` was added to a `Group` in this `Section`. Does not
  // add a match beyond the total limit.
  bool Add(const AutocompleteMatch& match);

  // Max number of matches this `Section` can contain across `groups_`.
  size_t limit_{0};
  // The number of matches this `Section` contains across `groups_`.
  size_t count_{0};
  // The `Group`s this `Section` contains.
  Groups groups_{};
  // This `Section`s map of group IDs to group information.
  omnibox::GroupConfigMap group_configs_;
  // This `Section`s side type.
  omnibox::GroupConfig_SideType side_type_;
};

// Base section for ZPS limits and grouping. Ensures that matches with higher
// relevance scores do not fill up the section if others with lower scores are
// expected to be placed earlier based on their `Group`'s position.
class ZpsSection : public Section {
 public:
  ZpsSection(size_t limit,
             Groups groups,
             omnibox::GroupConfigMap& group_configs,
             omnibox::GroupConfig_SideType side_type =
                 omnibox::GroupConfig_SideType_DEFAULT_PRIMARY);
  // Section:
  void InitFromMatches(ACMatches& matches) override;
};

// A ZpsSection that automatically counts all MV Tiles as one suggestion when
// applying the total limit.
class ZpsSectionWithMVTiles : public ZpsSection {
 public:
  explicit ZpsSectionWithMVTiles(size_t limit,
                                 Groups groups,
                                 omnibox::GroupConfigMap& group_configs);
  // Section:
  void InitFromMatches(ACMatches& matches) override;
};

// Android prefixed section for Adaptive Suggestions grouping.
class AndroidNonZPSSection : public Section {
 public:
  // Construct a new instance of the grouping class used in non-zero-prefix
  // context.
  // When `show_only_search_suggestions` is set to `true`, URLs will not be
  // offered at any position other than position 0 (the Default Match).
  explicit AndroidNonZPSSection(bool show_only_search_suggestions,
                                omnibox::GroupConfigMap& group_configs);

  // Section:
  void InitFromMatches(ACMatches& matches) override;

  // Specify number of matches that are at least 50% exposed while the
  // software keyboard is visible.
  static void set_num_visible_matches(size_t num_visible_matches) {
    num_visible_matches_ = num_visible_matches;
  }

 private:
  static size_t num_visible_matches_;
};

// Android prefix section for Hub search.
class AndroidHubNonZPSSection : public Section {
 public:
  explicit AndroidHubNonZPSSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Android ZPS limits and grouping for the NTP.
// - up to 15 + `max_related_queries` + `max_trending_queries` suggestions
//   total.
//  - up to 1 clipboard suggestion.
//  - up to 15 personalized suggestions.
//  - up to 5 trending search suggestions.
class AndroidNTPZpsSection : public ZpsSection {
 public:
  explicit AndroidNTPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Android ZPS limits and grouping for the SRP.
// - up to 15 suggestions total.
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 1 most visited carousel.
//  - up to 15 previous search related suggestions.
//  - up to 15 personalized suggestions.
class AndroidSRPZpsSection : public ZpsSection {
 public:
  explicit AndroidSRPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Android ZPS limits and grouping for the Web.
// - up to 15 suggestions total.
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 1 most visited carousel.
//  - up to 8 page related suggestions.
//  - up to 15 personalized suggestions.
class AndroidWebZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit AndroidWebZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the NTP.
// - up to 8 suggestions total or 7 total if the ZPS IPH is enabled (the 8th
// suggestion being the IPH).
//  - up to 8 personalized suggestions.
//  - up to 8 trending search suggestions.
class DesktopNTPZpsSection : public ZpsSection {
 public:
  explicit DesktopNTPZpsSection(omnibox::GroupConfigMap& group_configs,
                                size_t limit);
};

// Section expressing the Desktop ZPS limits and grouping for the IPH suggestion
// on the NTP.
// - Up to 1 IPH suggestion total
class DesktopNTPZpsIPHSection : public ZpsSection {
 public:
  explicit DesktopNTPZpsIPHSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop secondary ZPS limits and grouping for the NTP.
// - up to 4 suggestions total.
//  - up to 3 previous search related suggestion chips.
// - up to 4 previous search related text suggestions.
// - up to 4 trending suggestions.
class DesktopSecondaryNTPZpsSection : public ZpsSection {
 public:
  explicit DesktopSecondaryNTPZpsSection(
      omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the SRP.
// - up to 8 suggestions total.
//  - up to 8 previous search related suggestions.
//  - up to 8 personalized suggestions.
class DesktopSRPZpsSection : public ZpsSection {
 public:
  explicit DesktopSRPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the Web.
// - up to 8 suggestions total.
//  - up to 8 page related suggestions.
//  - up to 8 personalized suggestions.
class DesktopWebZpsSection : public ZpsSection {
 public:
  explicit DesktopWebZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the Lens
// contextual searchbox.
// - up to 8 suggestions total.
//  - up to 8 page related suggestions.
class DesktopLensContextualZpsSection : public ZpsSection {
 public:
  explicit DesktopLensContextualZpsSection(
      omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the Lens
// multimodal searchbox.
// - up to 8 suggestions total.
//  - up to 8 multimodal suggestions.
class DesktopLensMultimodalZpsSection : public ZpsSection {
 public:
  explicit DesktopLensMultimodalZpsSection(
      omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop, non-ZPS limits and grouping.
// - up to 10 suggestions total.
//  - up to 1 default, 10 starer packs, 10 search, 8 nav, and 1 history cluster
//   suggestions.
// - Only allow more than 8 suggestions if the section does not contain navs.
// - Only allow more than 7 navs if there are no non-navs to show.
// - The history cluster suggestion should count against the search limit.
// - The default suggestion should count against either the search or nav limit.
// - Group defaults 1st, then searches and history clusters, then navs.
class DesktopNonZpsSection : public Section {
 public:
  explicit DesktopNonZpsSection(omnibox::GroupConfigMap& group_configs);
  // Section:
  void InitFromMatches(ACMatches& matches) override;
};

// Section expressing the iPhone ZPS limits and grouping for the NTP.
// - up to `total_count` suggestions total.
//  - up to 1 clipboard suggestion.
//  - up to `psuggest_count` personalized suggestions.
//  - up to `max_trending_queries` trending suggestions.
class IOSNTPZpsSection : public ZpsSection {
 public:
  explicit IOSNTPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iPhone ZPS limits and grouping for the SRP.
// - up to 20 suggestions total (where all MV Tiles are counted for 1).
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 10 most visited in a carousel.
//  - up to 8 previous search related suggestions.
//  - up to 20 personalized suggestions.
class IOSSRPZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit IOSSRPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iPhone ZPS limits and grouping for the Web.
// - up to 20 suggestions total (but all MV Tiles are counted for 1).
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 10 most visited in a carousel.
//  - up to 8 page related suggestions.
//  - up to 20 personalized suggestions.
class IOSWebZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit IOSWebZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iOS ZPS limits and grouping for the Lens mutimodal
// searchbox.
// - up to 10 suggestions total.
//  - up to 10 search suggestions.
class IOSLensMultimodalZpsSection : public ZpsSection {
 public:
  explicit IOSLensMultimodalZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iPad ZPS limits and grouping for the NTP.
// - up to 10 suggestions total.
//  - up to 1 clipboard suggestion.
//  - up to 10 personalized suggestions.
class IOSIpadNTPZpsSection : public ZpsSection {
 public:
  explicit IOSIpadNTPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iPad ZPS limits and grouping for the SRP.
// - up to 10 suggestions total (but all MV Tiles are counted for 1).
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 10 most visited in a carousel.
//  - up to 8 previous search related suggestions.
//  - up to 10 personalized suggestions.
class IOSIpadSRPZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit IOSIpadSRPZpsSection(omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iPad ZPS limits and grouping for the Web.
// - up to 10 suggestions total (but all MV Tiles are counted for 1).
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 10 most visited in a carousel.
//  - up to 8 page related suggestions.
//  - up to 10 personalized suggestions.
class IOSIpadWebZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit IOSIpadWebZpsSection(omnibox::GroupConfigMap& group_configs);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
