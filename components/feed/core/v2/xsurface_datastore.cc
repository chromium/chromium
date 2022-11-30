// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/xsurface_datastore.h"

#include <iterator>

#include "base/logging.h"
#include "base/observer_list.h"

namespace feed {

XsurfaceDatastoreSlice::XsurfaceDatastoreSlice() = default;
XsurfaceDatastoreSlice::~XsurfaceDatastoreSlice() = default;

void XsurfaceDatastoreSlice::UpdateDatastoreEntry(const std::string& key,
                                                  const std::string& value) {
  entries_[key] = value;
  for (auto& o : observers_) {
    o.DatastoreEntryUpdated(this, key);
  }
}

void XsurfaceDatastoreSlice::RemoveDatastoreEntry(const std::string& key) {
  if (!entries_.erase(key)) {
    DLOG(WARNING) << "RemoveDatastoreEntry(" << key
                  << "), removed key does not exist";
    return;
  }
  for (auto& o : observers_) {
    o.DatastoreEntryRemoved(this, key);
  }
}

const std::string* XsurfaceDatastoreSlice::FindEntry(
    const std::string& key) const {
  auto iter = entries_.find(key);
  return (iter != entries_.end()) ? &iter->second : nullptr;
}

std::map<std::string, std::string> XsurfaceDatastoreSlice::GetAllEntries()
    const {
  return entries_;
}

void XsurfaceDatastoreSlice::AddObserver(
    XsurfaceDatastoreDataReader::Observer* o) {
  observers_.AddObserver(o);
}

void XsurfaceDatastoreSlice::RemoveObserver(
    XsurfaceDatastoreDataReader::Observer* o) {
  observers_.RemoveObserver(o);
}

XsurfaceDatastoreAggregate::XsurfaceDatastoreAggregate(
    std::vector<raw_ptr<XsurfaceDatastoreDataReader>> sources)
    : sources_(std::move(sources)) {
  for (XsurfaceDatastoreDataReader* s : sources_) {
    s->AddObserver(this);
  }
}

XsurfaceDatastoreAggregate::~XsurfaceDatastoreAggregate() {
  for (XsurfaceDatastoreDataReader* s : sources_) {
    s->RemoveObserver(this);
  }
}
void XsurfaceDatastoreAggregate::AddObserver(
    XsurfaceDatastoreDataReader::Observer* o) {
  observers_.AddObserver(o);
}

void XsurfaceDatastoreAggregate::RemoveObserver(
    XsurfaceDatastoreDataReader::Observer* o) {
  observers_.RemoveObserver(o);
}

const std::string* XsurfaceDatastoreAggregate::FindEntry(
    const std::string& key) const {
  // It is assumed that each key provider provides a disjoint set of keys, so
  // any given key should have only one entry.
  for (XsurfaceDatastoreDataReader* s : sources_) {
    const std::string* result = s->FindEntry(key);
    if (result)
      return result;
  }
  return nullptr;
}

void XsurfaceDatastoreAggregate::DatastoreEntryUpdated(
    XsurfaceDatastoreDataReader* source,
    const std::string& key) {
  for (XsurfaceDatastoreDataReader::Observer& o : observers_) {
    o.DatastoreEntryUpdated(this, key);
  }
}

void XsurfaceDatastoreAggregate::DatastoreEntryRemoved(
    XsurfaceDatastoreDataReader* source,
    const std::string& key) {
  for (XsurfaceDatastoreDataReader::Observer& o : observers_) {
    o.DatastoreEntryRemoved(this, key);
  }
}

std::map<std::string, std::string> XsurfaceDatastoreAggregate::GetAllEntries()
    const {
  std::map<std::string, std::string> result;
  for (XsurfaceDatastoreDataReader* s : sources_) {
    std::map<std::string, std::string> sub_entries = s->GetAllEntries();
    result.insert(std::make_move_iterator(sub_entries.begin()),
                  std::make_move_iterator(sub_entries.end()));
  }
  return result;
}

}  // namespace feed
