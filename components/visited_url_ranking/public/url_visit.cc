// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit.h"

#include <utility>

namespace visited_url_ranking {

URLVisit::URLVisit(const GURL& url_arg,
                   const std::u16string& title_arg,
                   const base::Time& last_modified_arg,
                   syncer::DeviceInfo::FormFactor device_type_arg,
                   Source source_arg)
    : url(url_arg),
      title(title_arg),
      last_modified(last_modified_arg),
      device_type(device_type_arg),
      source(source_arg) {}

URLVisit::URLVisit(const URLVisit&) = default;

URLVisit::~URLVisit() = default;

URLVisitAggregate::URLVisitAggregate() = default;

URLVisitAggregate::~URLVisitAggregate() = default;

URLVisitAggregate::URLVisitAggregate(URLVisitAggregate&& other) = default;

URLVisitAggregate& URLVisitAggregate::operator=(URLVisitAggregate&& other) =
    default;

URLVisitAggregate::Tab::Tab(const int32_t id_arg,
                            URLVisit visit_arg,
                            std::optional<std::string> session_tag_arg,
                            std::optional<std::string> session_name_arg)
    : id(id_arg),
      visit(std::move(visit_arg)),
      session_tag(session_tag_arg),
      session_name(session_name_arg) {}

URLVisitAggregate::Tab::Tab(const URLVisitAggregate::Tab&) = default;

URLVisitAggregate::Tab::~Tab() = default;

URLVisitAggregate::TabData::TabData(Tab tab)
    : last_active_tab(std::move(tab)) {}

URLVisitAggregate::TabData::TabData(const URLVisitAggregate::TabData&) =
    default;

URLVisitAggregate::TabData::~TabData() = default;

}  // namespace visited_url_ranking
