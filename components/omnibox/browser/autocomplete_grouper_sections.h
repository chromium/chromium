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
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/aim_tools_and_models.pb.h"

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
                   const omnibox::GroupConfigMap& group_configs,
                   omnibox::GroupConfig_SideType side_type =
                       omnibox::GroupConfig_SideType_DEFAULT_PRIMARY);
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

// Base section for ZPS limits and grouping. Asserts that matches are sorted by
// their `Group`s position.
class ZpsSection : public Section {
 public:
  ZpsSection(size_t limit,
             Groups groups,
             const omnibox::GroupConfigMap& group_configs,
             omnibox::GroupConfig_SideType side_type =
                 omnibox::GroupConfig_SideType_DEFAULT_PRIMARY);
  // Section:
  void InitFromMatches(ACMatches& matches) override;
};

// Base section for ZPS limits and grouping where local history zero-prefix
// suggestions are enabled. Sorts the matches by their `Group`s position to
// ensure zero-prefix suggestions from local history backfill remote
// personalized zero-prefix suggestions.
// TODO(crbug.com/409810808): Find a more general solution for accommodating
// local history backfill and remove this class.
class ZpsSectionWithLocalHistory : public ZpsSection {
 protected:
  explicit ZpsSectionWithLocalHistory(
      size_t limit,
      Groups groups,
      const omnibox::GroupConfigMap& group_configs);
  // Section:
  void InitFromMatches(ACMatches& matches) override;
};

// A ZpsSection that automatically counts all MV Tiles as one suggestion when
// applying the total limit.
class ZpsSectionWithMVTiles : public ZpsSection {
 public:
  explicit ZpsSectionWithMVTiles(size_t limit,
                                 Groups groups,
                                 const omnibox::GroupConfigMap& group_configs);
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
                                const omnibox::GroupConfigMap& group_configs);

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

// Android section for a single default match suggestion when there is one or
// more composebox attachment.
class AndroidComposeboxNonZPSSection : public Section {
 public:
  explicit AndroidComposeboxNonZPSSection(
      const omnibox::GroupConfigMap& group_configs);

  // TODO(crbug.com/464014032): split by mode.
  // Number of contextual attachments
  static size_t num_attachments_;
  static omnibox::ChromeAimToolsAndModels tool_mode_;
};

// Android prefix section for Hub search (ZPS).
class AndroidHubZPSSection : public Section {
 public:
  explicit AndroidHubZPSSection(const omnibox::GroupConfigMap& group_configs);
};

// Android prefix section for Hub search (non-ZPS).
class AndroidHubNonZPSSection : public Section {
 public:
  explicit AndroidHubNonZPSSection(
      const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Android ZPS limits and grouping for the NTP.
// - up to 15 + `max_related_queries` + `max_trending_queries` suggestions
//   total.
//  - up to 1 clipboard suggestion.
//  - up to 15 MIA or personalized suggestions.
//  - up to 5 trending search suggestions.
class AndroidNTPZpsSection : public ZpsSectionWithLocalHistory {
 public:
  AndroidNTPZpsSection(const omnibox::GroupConfigMap& group_configs,
                       bool mia_enabled);

  void InitFromMatches(ACMatches& matches) override;
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
  explicit AndroidSRPZpsSection(const omnibox::GroupConfigMap& group_configs);
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
  explicit AndroidWebZpsSection(const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the NTP.
// - up to 8 suggestions total or 7 total if the ZPS IPH is enabled (the 8th
// suggestion being the IPH).
//  - up to 8 MIA or personalized suggestions.
//  - up to 8 trending search suggestions.
//  - up to 5 contextual search suggestions.
class DesktopNTPZpsSection : public ZpsSectionWithLocalHistory {
 public:
  DesktopNTPZpsSection(const omnibox::GroupConfigMap& group_configs,
                       size_t limit,
                       bool mia_enabled);
};

// Section expressing the Desktop ZPS limits and grouping for unscoped
// extensions.
// - Up to 8 unscoped extension suggestions total.
//  - Up to 4 from the first extension.
//  - Up to 4 from the second extension.
class DesktopZpsUnscopedExtensionSection : public ZpsSection {
 public:
  explicit DesktopZpsUnscopedExtensionSection(
      const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the IPH suggestion
// on the NTP.
// - Up to 1 IPH suggestion total
class DesktopNTPZpsIPHSection : public ZpsSection {
 public:
  explicit DesktopNTPZpsIPHSection(
      const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop secondary ZPS limits and grouping for the NTP.
// - up to 4 suggestions total.
//  - up to 3 previous search related suggestion chips.
// - up to 4 previous search related text suggestions.
// - up to 4 trending suggestions.
class DesktopSecondaryNTPZpsSection : public ZpsSection {
 public:
  explicit DesktopSecondaryNTPZpsSection(
      const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the SRP.
// - up to `max_suggestions` suggestions total.
//  - up to `search_limit` previous search related suggestions.
//  - up to `search_limit` personalized suggestions.
//  - up to `url_limit` most visited tiles suggestions
class DesktopSRPZpsSection : public ZpsSection {
 public:
  explicit DesktopSRPZpsSection(const omnibox::GroupConfigMap& group_configs,
                                size_t max_suggestions,
                                size_t search_limit,
                                size_t url_limit,
                                size_t contextual_action_limit);
};

// Section expressing the Desktop URL ZPS limits and grouping for the Web.
// - up to `limit` most visited tiles suggestions.
class DesktopWebURLZpsSection : public ZpsSection {
 public:
  explicit DesktopWebURLZpsSection(const omnibox::GroupConfigMap& group_configs,
                                   size_t limit);
};

// Section expressing the Desktop Search ZPS limits and grouping for the Web.
// - up to `limit` suggestions total.
//  - up to `limit` page related or personalized search suggestions.
//  - up to `contextual_action_limit` contextual search action suggestions.
//  - up to `contextual_search_limit` contextual search suggestions.
class DesktopWebSearchZpsSection : public Section {
 public:
  explicit DesktopWebSearchZpsSection(
      const omnibox::GroupConfigMap& group_configs,
      size_t limit,
      size_t contextual_action_limit,
      size_t contextual_search_limit);
};

// An experimental alternative for `DesktopWebSearchZpsSection` that excludes
// all but contextual matches. It's intended as a full replacement instead
// of modifying that section, for simplicity and ease of removal after
// experimentation.
// - up to `contextual_action_limit` + `contextual_search_limit` total.
//  - up to `contextual_action_limit` contextual search action suggestions.
//  - up to `contextual_search_limit` contextual search suggestions.
class DesktopWebSearchZpsContextualOnlySection : public Section {
 public:
  explicit DesktopWebSearchZpsContextualOnlySection(
      const omnibox::GroupConfigMap& group_configs,
      size_t contextual_action_limit,
      size_t contextual_search_limit);
};

// Section expressing the Desktop ZPS limits and grouping for the Lens
// contextual searchbox.
// - up to 8 suggestions total.
//  - up to 8 page related suggestions.
class DesktopLensContextualZpsSection : public ZpsSection {
 public:
  explicit DesktopLensContextualZpsSection(
      const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the Desktop ZPS limits and grouping for the Lens
// multimodal searchbox.
// - default to 8 multimodal suggestions.
class DesktopLensMultimodalZpsSection : public ZpsSection {
 public:
  explicit DesktopLensMultimodalZpsSection(
      const omnibox::GroupConfigMap& group_configs);

  explicit DesktopLensMultimodalZpsSection(
      const omnibox::GroupConfigMap& group_configs,
      size_t max_suggestions);
};

class AndroidComposeboxZpsSection : public ZpsSection {
 public:
  explicit AndroidComposeboxZpsSection(
      const omnibox::GroupConfigMap& group_configs,
      size_t max_suggestions,
      size_t max_aim_suggestions,
      size_t max_contextual_suggestions);

  // Number of contextual attachments
  static size_t num_attachments_;
};

class IOSComposeboxZpsSection : public ZpsSection {
 public:
  explicit IOSComposeboxZpsSection(const omnibox::GroupConfigMap& group_configs,
                                   size_t max_suggestions,
                                   size_t max_aim_suggestions,
                                   size_t max_contextual_suggestions);
};

class DesktopComposeboxZpsSection : public ZpsSection {
 public:
  explicit DesktopComposeboxZpsSection(
      const omnibox::GroupConfigMap& group_configs,
      size_t max_suggestions,
      size_t max_aim_suggestions,
      size_t max_contextual_suggestions);
};

// A ZPS section that includes only the toolbelt match.
class ToolbeltSection : public ZpsSection {
 public:
  explicit ToolbeltSection(const omnibox::GroupConfigMap& group_configs);
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
  explicit DesktopNonZpsSection(const omnibox::GroupConfigMap& group_configs);
  // Section:
  void InitFromMatches(ACMatches& matches) override;
};

// Section expressing the iOS ZPS limits and grouping for the NTP.
// - up to `total_count` suggestions total.
//  - up to 1 clipboard suggestion.
//  - up to `psuggest_count` MIA or personalized suggestions.
//  - up to `max_trending_queries` trending suggestions.
class IOSNTPZpsSection : public ZpsSectionWithLocalHistory {
 public:
  IOSNTPZpsSection(const omnibox::GroupConfigMap& group_configs,
                   bool mia_enabled);
  void InitFromMatches(ACMatches& matches) override;
};

// Section expressing the iOS ZPS limits and grouping for the SRP.
// - up to 20 suggestions total (where all MV Tiles are counted for 1).
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 10 most visited in a carousel.
//  - up to 8 previous search related suggestions.
//  - up to 20 personalized suggestions.
class IOSSRPZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit IOSSRPZpsSection(const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iOS ZPS limits and grouping for the Web.
// - up to 20 suggestions total (but all MV Tiles are counted for 1).
//  - up to 1 verbatim suggestion.
//  - up to 1 clipboard suggestion.
//  - up to 10 most visited in a carousel.
//  - up to 8 page related suggestions.
//  - up to 20 personalized suggestions.
class IOSWebZpsSection : public ZpsSectionWithMVTiles {
 public:
  explicit IOSWebZpsSection(const omnibox::GroupConfigMap& group_configs);
};

// Section expressing the iOS ZPS limits and grouping for the Lens mutimodal
// searchbox.
// - up to 10 suggestions total.
//  - up to 10 search suggestions.
class IOSLensMultimodalZpsSection : public ZpsSection {
 public:
  explicit IOSLensMultimodalZpsSection(
      const omnibox::GroupConfigMap& group_configs);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
