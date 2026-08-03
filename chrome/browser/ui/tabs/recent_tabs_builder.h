// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_RECENT_TABS_BUILDER_H_
#define CHROME_BROWSER_UI_TABS_RECENT_TABS_BUILDER_H_

#include <optional>
#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info.h"
#include "ui/actions/action_id.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

class Browser;

// Represents a single item in the Recent Tabs hierarchy (header, command,
// tab, window, group, split, or device).
class RecentTabItem {
 public:
  enum class Type {
    kHeader,   // Title / section header
    kCommand,  // Actionable command (History, side panel, sync promo, etc.)
    kTab,      // Local or remote tab
    kWindow,   // Recently closed window
    kGroup,    // Recently closed tab group
    kSplit,    // Recently closed split view
    kDevice,   // Remote device session
  };

  RecentTabItem(Type type, std::u16string title);
  RecentTabItem(const RecentTabItem&);
  RecentTabItem(RecentTabItem&&);
  RecentTabItem& operator=(const RecentTabItem&);
  RecentTabItem& operator=(RecentTabItem&&);
  ~RecentTabItem();

  Type type() const { return type_; }
  const std::u16string& title() const { return title_; }

  std::optional<actions::ActionId> action_id() const { return action_id_; }
  void set_action_id(std::optional<actions::ActionId> id) { action_id_ = id; }

  SessionID session_id() const { return session_id_; }
  void set_session_id(SessionID id) { session_id_ = id; }

  const std::string& session_tag() const { return session_tag_; }
  void set_session_tag(std::string tag) { session_tag_ = std::move(tag); }

  const GURL& url() const { return url_; }
  void set_url(GURL url) { url_ = std::move(url); }

  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

  bool is_local() const { return is_local_; }
  void set_is_local(bool is_local) { is_local_ = is_local; }

  const ui::ImageModel& icon() const { return icon_; }
  void set_icon(ui::ImageModel icon) { icon_ = std::move(icon); }

  const ui::ImageModel& minor_icon() const { return minor_icon_; }
  void set_minor_icon(ui::ImageModel icon) { minor_icon_ = std::move(icon); }

  syncer::DeviceInfo::FormFactor device_form_factor() const {
    return device_form_factor_;
  }
  void set_device_form_factor(syncer::DeviceInfo::FormFactor factor) {
    device_form_factor_ = factor;
  }

  const std::vector<RecentTabItem>& children() const { return children_; }
  std::vector<RecentTabItem>& children() { return children_; }
  void add_child(RecentTabItem child) { children_.push_back(std::move(child)); }

 private:
  Type type_;
  std::u16string title_;
  std::optional<actions::ActionId> action_id_;
  SessionID session_id_ = SessionID::InvalidValue();
  std::string session_tag_;
  GURL url_;
  bool enabled_ = true;
  bool is_local_ = true;
  ui::ImageModel icon_;
  ui::ImageModel minor_icon_;
  syncer::DeviceInfo::FormFactor device_form_factor_ =
      syncer::DeviceInfo::FormFactor::kUnknown;
  std::vector<RecentTabItem> children_;
};

class RecentTabsBuilder {
 public:
  // Builds and returns the complete list of recent tabs entries for a browser.
  static std::vector<RecentTabItem> BuildRecentTabs(Browser* browser);

  // Helper methods for building subsets of entries:
  static std::vector<RecentTabItem> BuildHistoryEntries(Browser* browser);
  static std::vector<RecentTabItem> BuildLocalEntries(Browser* browser);
  static std::vector<RecentTabItem> BuildRemoteEntries(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_TABS_RECENT_TABS_BUILDER_H_
