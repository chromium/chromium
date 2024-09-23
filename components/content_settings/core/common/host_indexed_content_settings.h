// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_

#include <iterator>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace content_settings {

// Class with maps indexed by a setting's host. If primary_pattern host is a
// wildcard, index by secondary host. Patterns with wildcard host for primary
// and secondary are handled separately. The index preserves the order of
// precedence of content settings.
class HostIndexedContentSettings {
 public:
  typedef std::map<std::string, Rules, ContentSettingsPattern::CompareDomains>
      HostToContentSettings;

  // Returns a vector of indices where each index contains entries from a
  // different |source|. This keeps the order of precedence of
  // ContentSettingsProviders.
  static std::vector<HostIndexedContentSettings> Create(
      const ContentSettingsForOneType& settings);

  HostIndexedContentSettings();

  explicit HostIndexedContentSettings(const base::Clock* clock);

  // Creates an index with additional metadata about the content settings
  // provider that the settings came from.
  HostIndexedContentSettings(ProviderType source, bool off_the_record);

  HostIndexedContentSettings(const HostIndexedContentSettings& other) = delete;
  HostIndexedContentSettings& operator=(const HostIndexedContentSettings&) =
      delete;
  HostIndexedContentSettings(HostIndexedContentSettings&& other);
  HostIndexedContentSettings& operator=(HostIndexedContentSettings&&);

  ~HostIndexedContentSettings();

  struct Iterator {
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = const RuleEntry;
    using pointer = const RuleEntry*;
    using reference = const RuleEntry&;

    Iterator(const HostIndexedContentSettings& index, bool begin);
    ~Iterator();

    Iterator(Iterator&&);
    Iterator& operator=(Iterator&&) = delete;
    Iterator(const Iterator& other);
    Iterator& operator=(const Iterator&) = delete;

    reference operator*() const { return *current_iterator_; }
    pointer operator->() { return &*current_iterator_; }

    Iterator& operator++();
    Iterator operator++(int);

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.current_iterator_ == b.current_iterator_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return !(a == b);
    }

   private:
    enum class Stage { kInvalid, kPrimaryHost, kSecondaryHost, kWildcard };

    void SetStage(Stage stage);

    const raw_ref<const HostIndexedContentSettings> index_;

    // This enum defines over which of the structure of index_ we are currently
    // iterating.
    Stage stage_ = Stage::kInvalid;

    // This iterator points into an index structure for the current stage.
    HostToContentSettings::const_iterator next_map_iterator_;
    HostToContentSettings::const_iterator next_map_end_;

    // This iterator is for a host bucket within the current structure.
    Rules::const_iterator current_iterator_;
    Rules::const_iterator current_end_;
  };

  Iterator begin() const;
  Iterator end() const;

  size_t size() const;
  bool empty() const;

  // Returns the source of the entries within this index.
  const ProviderType& source() const { return source_; }
  // Returns whether the index contains off the record entries.
  const std::optional<bool>& off_the_record() const { return off_the_record_; }

  // Finds the RuleEntry with highest precedence that matches both the primary
  // and secondary urls or returns nullptr if no match is found. The pointer is
  // only valid until the content of this index is modified.
  const RuleEntry* Find(const GURL& primary_url,
                        const GURL& secondary_url) const;

  // Add the setting to the index.
  // Returns true if something changed.
  bool SetValue(const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern,
                base::Value value,
                const RuleMetaData& metadata);

  // Deletes the index entry for the given |primary_pattern|,
  // |secondary_pattern|, |content_type| tuple.
  // Returns true if something changed.
  bool DeleteValue(const ContentSettingsPattern& primary_pattern,
                   const ContentSettingsPattern& secondary_pattern);

  // Clears the object information.
  void Clear();

  void SetClockForTesting(const base::Clock* clock);

 private:
  HostToContentSettings primary_host_indexed_;
  HostToContentSettings secondary_host_indexed_;
  Rules wildcard_settings_;
  ProviderType source_ = ProviderType::kNone;
  std::optional<bool> off_the_record_;
  raw_ptr<const base::Clock> clock_;
  mutable int iterating_ = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_HOST_INDEXED_CONTENT_SETTINGS_H_
