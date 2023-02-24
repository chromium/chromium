// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_OVERVIEW_H_
#define COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_OVERVIEW_H_

#include <cstddef>
#include <memory>

namespace power_bookmarks {

class Power;

// Class to encapsulate an overview of a Power. This class represents the
// "first" Power, what qualifies as first depends on the call, and the total
// count of the Powers of the same type.
class PowerOverview {
 public:
  PowerOverview(std::unique_ptr<Power> power, size_t count);
  PowerOverview(const PowerOverview&) = delete;
  PowerOverview& operator=(const PowerOverview&) = delete;

  ~PowerOverview();

  const Power* power() const { return power_.get(); }
  size_t count() const { return count_; }

 private:
  std::unique_ptr<Power> power_;
  size_t count_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_OVERVIEW_H_
