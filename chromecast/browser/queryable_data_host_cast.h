// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_QUERYABLE_DATA_HOST_CAST_H_
#define CHROMECAST_BROWSER_QUERYABLE_DATA_HOST_CAST_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chromecast/activity/queryable_data_host.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

// Sends queryable data to a non fuchsia Host.
class QueryableDataHostCast : public QueryableDataHost {
 public:
  explicit QueryableDataHostCast(content::WebContents* web_contents);
  ~QueryableDataHostCast() override;

  // chromecast::QueryableDataHost implementation:
  void SendQueryableValue(const std::string& key,
                          const base::Value& value) override;

 private:
  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(QueryableDataHostCast);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_QUERYABLE_DATA_HOST_CAST_H_
