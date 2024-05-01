// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_ITEM_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_ITEM_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_items_collection/core/offline_item_filter.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "components/offline_items_collection/core/pending_state.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace offline_items_collection {

// An id that uniquely represents a piece of offline content.
struct ContentId {
  // The namespace for the offline content.  This will be used to associate this
  // id with a particular OfflineContentProvider.  A name_space can include
  // any characters except ','.  This is due to a serialization format
  // limitation.
  // TODO(dtrainor): Remove the 'no ,' limitation.
  std::string name_space;

  // The id of the offline item.
  std::string id;

  ContentId();
  ContentId(const ContentId& other);
  ContentId(const std::string& name_space, const std::string& id);

  ~ContentId();

  bool operator==(const ContentId& content_id) const;

  bool operator!=(const ContentId& content_id) const;

  bool operator<(const ContentId& content_id) const;
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
enum class OfflineItemProgressUnit {
  BYTES,
  FILES,
  PERCENTAGE,
};

// This struct holds the relevant pieces of information to represent an abstract
// offline item to the front end.  This is meant to be backed by components that
// need to both show content being offlined (downloading, saving, etc.) as well
// as content that should be exposed as available offline (downloads, pages,
// etc.).
//
// A new feature should expose these OfflineItems via an OfflineContentProvider.
struct OfflineItem {
  // This struct holds the essential pieces of information to compute the
  // download progress for an offline item to display in the UI.
  struct Progress {
    Progress();
    Progress(const Progress& other);
    ~Progress();

    bool operator==(const Progress& progress) const;

    // Current value of the download progress.
    int64_t value;

    // The maximum value of the download progress. Absence of the value implies
    // indeterminate progress.
    std::optional<int64_t> max;

    // The unit of progress to be displayed in the UI.
    OfflineItemProgressUnit unit;
  };

  OfflineItem();
  OfflineItem(const OfflineItem& other);
  OfflineItem& operator=(const OfflineItem& other);

  explicit OfflineItem(const ContentId& id);

  ~OfflineItem();

  bool operator==(const OfflineItem& offline_item) const;

  // Note: please update test_support/offline_item_test_support.cc
  // when adding members here.

  // The id of this OfflineItem.  Used to identify this item across all relevant
  // systems.
  ContentId id;

  // Display Metadata.
  // ---------------------------------------------------------------------------
  // The title of the OfflineItem to display in the UI.
  std::string title;

  // The description of the OfflineItem to display in the UI (may or may not be
  // displayed depending on the specific UI component).
  std::string description;

  // The type of offline item this is.  This can be used for filtering offline
  // items as well as for determining which default icon to use.
  OfflineItemFilter filter;

  // Whether or not this item is transient.  Transient items won't show up in
  // persistent UI spaces and will only show up as notifications.
  bool is_transient;

  // Whether this item should show up as a suggested item for the user.
  bool is_suggested;

  // Whether this item is going through accelerated download.
  bool is_accelerated;

  // Whether the origin should be displayed.
  bool promote_origin;

  // Whether or not this item can be renamed.
  bool can_rename;

  // Whether or not to bother querying for visuals.  Querying is not guaranteed
  // to return anything, but if this is |true| the UI layers can make
  // optimizations without waiting for the asynchronous query for visual
  // information.
  bool ignore_visuals;

  // A score in the range of 0 to 1.0 showing how relevant the content is for
  // the user. Higher score is an indication that the item will be ranked higher
  // in the UI surfaces.
  double content_quality_score;

  // TODO(dtrainor): Build out custom per-item icon support.

  // Content Metadata.
  // ---------------------------------------------------------------------------
  // The total size of the offline item as best known at the current time.
  int64_t total_size_bytes;

  // Whether or not this item has been removed externally (not by Chrome).
  bool externally_removed;

  // The time when the underlying offline content was created.
  base::Time creation_time;

  // The time when the underlying offline content finished downloading,
  // base::Time() if the download hasn't reached a completion state yet.
  base::Time completion_time;

  // The last time the underlying offline content was accessed.
  base::Time last_accessed_time;

  // Whether or not this item can be opened after it is done being downloaded.
  bool is_openable;

  // The target file path for this offline item.
  base::FilePath file_path;

  // The mime type for this offline item.
  std::string mime_type;

  // Request Metadata.
  // ---------------------------------------------------------------------------
  // The URL of the offline item, after all redirections.
  GURL url;

  // The URL that represents the original request (before any redirection).
  GURL original_url;

  // Whether or not this item is off the record.
  bool is_off_the_record;

  // The OTRProfileID of the profile, null if the profile is regular.
  std::string otr_profile_id;

  // Identifies the item's publisher.
  std::string attribution;

  // The URL of document that is considered the referrer for the original URL.
  GURL referrer_url;

  // Whether this item is triggered by user gesture.
  bool has_user_gesture;

  // In Progress Metadata.
  // ---------------------------------------------------------------------------
  // The current state of the OfflineItem.
  OfflineItemState state;

  // Reason OfflineItem failed to download.
  FailState fail_state;

  // Reason OfflineItem is pending.
  PendingState pending_state;

  // Whether or not the offlining of this content can be resumed if it was
  // paused or interrupted.
  bool is_resumable;

  // Whether or not this OfflineItem can be downloaded using a metered
  // connection.
  bool allow_metered;

  // The current amount of bytes received for this item.  This field is not used
  // if |state| is COMPLETE.
  int64_t received_bytes;

  // Represents the current progress of this item. This field is not used if
  // |state| is COMPLETE.
  Progress progress;

  // The estimated time remaining for the download in milliseconds.  -1
  // represents an unknown time remaining.  This field is not used if |state| is
  // COMPLETE.
  int64_t time_remaining_ms;

  // Whether the download might be dangerous and will require additional
  // validation from user.
  bool is_dangerous;
};

// Implemented for test-only. See test_support/offline_item_test_support.cc.
std::ostream& operator<<(std::ostream& os, const OfflineItem& item);

// This struct holds any potentially expensive visuals for an OfflineItem.  If
// the front end requires the visuals it will ask for them through the
// OfflineContentProvider interface asynchronously to give the backend time to
// generate them if necessary.
//
// It is not expected that these will change.  Currently the UI might cache the
// results of this call.
// TODO(dtrainor): If we run into a scenario where this changes, add a way for
// an OfflineItem update to let us know about an update to the visuals.
struct OfflineItemVisuals {
  OfflineItemVisuals();
  OfflineItemVisuals(const gfx::Image& icon, const gfx::Image& custom_favicon);
  OfflineItemVisuals(const OfflineItemVisuals& other);

  ~OfflineItemVisuals();

  // The icon to use for displaying this item.  The icon should be 64dp x 64dp.
  // TODO(dtrainor): Suggest icon size based on the icon size supported by the
  // current OS.
  gfx::Image icon;

  // Favicon to be shown with the item; optional.
  gfx::Image custom_favicon;
};

// This struct holds additional information related to sharing a particular
// OfflineItem.  This information doesn't necessarily exist within OfflineItem
// because it may be expensive/unnecessary to compute until the user attempts to
// share the item.
struct OfflineItemShareInfo {
  OfflineItemShareInfo();
  OfflineItemShareInfo(const OfflineItemShareInfo& other);

  ~OfflineItemShareInfo();

  // The local URI where the file can be accessed on disk.  This may be
  // different from |OfflineItem::file_path| depending on whether or not the
  // file can be accessed directly.
  // If this path is invalid the request data from OfflineItem will be used
  // to share the information instead (e.g. |OfflineItem::page_url|).
  base::FilePath uri;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_ITEM_H_
