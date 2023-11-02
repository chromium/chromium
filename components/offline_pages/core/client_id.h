// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_ID_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_ID_H_

#include <iosfwd>
#include <string>

namespace offline_pages {

// Defines a namespace/id pair that allows offline page clients to uniquely
// identify their own items within adopting internal systems. It is the client's
// responsibility to keep id values unique within its assigned namespace, but it
// is not a requirement.
struct ClientId {
  ClientId();
  ClientId(const std::string& name_space, const std::string& id);

  bool operator==(const ClientId& client_id) const;

  bool operator<(const ClientId& client_id) const;

  std::string ToString() const;

  // The namespace that identifies the client (of course 'namespace' is a
  // reserved word, so...).
  std::string name_space;

  // The client specified id that allows it to uniquely identify entries within
  // its namespace. These values are opaque to offline page systems and not used
  // internally as an identifier.
  std::string id;
};

std::ostream& operator<<(std::ostream& out, const ClientId& cid);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_ID_H_