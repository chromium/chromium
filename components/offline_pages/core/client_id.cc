// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_id.h"

#include <ostream>

namespace offline_pages {

ClientId::ClientId() = default;

ClientId::ClientId(const std::string& name_space, const std::string& id)
    : name_space(name_space), id(id) {}

bool ClientId::operator==(const ClientId& client_id) const {
  return name_space == client_id.name_space && id == client_id.id;
}

bool ClientId::operator<(const ClientId& client_id) const {
  if (name_space == client_id.name_space) {
    return (id < client_id.id);
  }

  return name_space < client_id.name_space;
}

std::string ClientId::ToString() const {
  return std::string("ClientId(")
      .append(name_space)
      .append(", ")
      .append(id)
      .append(")");
}

std::ostream& operator<<(std::ostream& out, const ClientId& cid) {
  return out << cid.ToString();
}

}  // namespace offline_pages
