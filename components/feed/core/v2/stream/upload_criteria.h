// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_UPLOAD_CRITERIA_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_UPLOAD_CRITERIA_H_

class PrefService;

namespace feed {
namespace feed_stream {

// Determines whether we can upload actions.
class UploadCriteria {
 public:
  explicit UploadCriteria(PrefService* profile_prefs);
  UploadCriteria(const UploadCriteria&) = delete;
  UploadCriteria& operator=(const UploadCriteria&) = delete;

  bool CanUploadActions() const;

  // Events to update criteria.
  void SurfaceOpenedOrClosed();
  void Clear();

 private:
  bool HasReachedConditionsToUploadActionsWithNoticeCard();
  void UpdateCanUploadActionsWithNoticeCard();

  PrefService* profile_prefs_;
  // Whether the feed stream can upload actions with the notice card in the
  // feed. This is cached so that we enable uploads in the session after the
  // criteria was met.
  bool can_upload_actions_with_notice_card_ = false;
};

}  // namespace feed_stream
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_UPLOAD_CRITERIA_H_
