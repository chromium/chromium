// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_TYPE_SET_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_TYPE_SET_H_

#include "base/check.h"
#include "components/content_settings/core/common/content_settings_types.h"

// This class either contains a single type or represents all types.
// It enforces correct type handling of content_setting Observers.
class ContentSettingsTypeSet {
 public:
  explicit ContentSettingsTypeSet(ContentSettingsType type) : type_(type) {}

  static ContentSettingsTypeSet AllTypes() {
    return ContentSettingsTypeSet(ContentSettingsType::DEFAULT);
  }

  // Returns true if type is in this set.
  bool Contains(ContentSettingsType type) const {
    return ContainsAllTypes() || (type == type_);
  }

  // Returns true if this set represents all content settings types.
  bool ContainsAllTypes() const {
    return type_ == ContentSettingsType::DEFAULT;
  }

  // Returns the type in this set. Can only be called if ContainsAllTypes() is
  // false.
  ContentSettingsType GetType() const {
    DCHECK(!ContainsAllTypes());
    return type_;
  }

  // Returns the content settings type or DEFAULT if this set contains all
  // types. Avoid usage if possible.
  ContentSettingsType GetTypeOrDefault() const { return type_; }

  bool operator==(const ContentSettingsTypeSet other) const {
    return type_ == other.type_;
  }

 private:
  ContentSettingsType type_;
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_TYPE_SET_H_
