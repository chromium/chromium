// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_NOTIFICATION_DETAILS_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_NOTIFICATION_DETAILS_H_

#include "ui/gfx/geometry/rect.h"

class FindNotificationDetails {
 public:
  FindNotificationDetails(int request_id,
                          int number_of_matches,
                          const gfx::Rect& selection_rect,
                          int active_match_ordinal,
                          bool final_update)
    : request_id_(request_id),
      number_of_matches_(number_of_matches),
      selection_rect_(selection_rect),
      active_match_ordinal_(active_match_ordinal),
      final_update_(final_update) {}

  FindNotificationDetails()
    : request_id_(0),
      number_of_matches_(-1),
      active_match_ordinal_(-1),
      final_update_(false) {}

  ~FindNotificationDetails() {}

  int request_id() const { return request_id_; }

  int number_of_matches() const { return number_of_matches_; }

  gfx::Rect selection_rect() const { return selection_rect_; }

  int active_match_ordinal() const { return active_match_ordinal_; }

  bool final_update() const { return final_update_; }

  bool operator==(const FindNotificationDetails& other) const {
    return std::tie(request_id_, number_of_matches_, selection_rect_,
                    active_match_ordinal_, final_update_) ==
           std::tie(other.request_id_, other.number_of_matches_,
                    other.selection_rect_, other.active_match_ordinal_,
                    other.final_update_);
  }

 private:
  int request_id_;    // The find-in-page request whose results we're returning.
  int number_of_matches_;  // How many matches were found.
  gfx::Rect selection_rect_;  // Where selection occurred (screen coordinate).
  int active_match_ordinal_;  // The ordinal of the currently selected match.
  bool final_update_;  // Whether this is the last Find Result update.
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_NOTIFICATION_DETAILS_H_
