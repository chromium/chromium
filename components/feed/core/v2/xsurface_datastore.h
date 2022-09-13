// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_XSURFACE_DATASTORE_H_
#define COMPONENTS_FEED_CORE_V2_XSURFACE_DATASTORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/observer_list.h"

// This file defines some types related to the XSurface datastore.
// The XSurface datastore is a generic key-value store, which can affect the
// display of UI elements. In some cases, we populate data store with
// information source by the Feed component.
namespace feed {

// Write-access to something that stores data store elements.
class XsurfaceDatastoreDataWriter {
 public:
  virtual void UpdateDatastoreEntry(const std::string& key,
                                    const std::string& value) = 0;
  virtual void RemoveDatastoreEntry(const std::string& key) = 0;
};

// Read-access to something that stores data store elements.
class XsurfaceDatastoreDataReader {
 public:
  // Observes data that can be read.
  class Observer : public base::CheckedObserver {
   public:
    // An entry has been added or updated.
    virtual void DatastoreEntryUpdated(XsurfaceDatastoreDataReader* source,
                                       const std::string& key) = 0;
    // An entry has been removed.
    virtual void DatastoreEntryRemoved(XsurfaceDatastoreDataReader* source,
                                       const std::string& key) = 0;
  };

  // Begin observing. Data already present before `AddObserver()` is called can
  // be read using `GetAllEntries()`.
  virtual void AddObserver(XsurfaceDatastoreDataReader::Observer* o) = 0;
  virtual void RemoveObserver(XsurfaceDatastoreDataReader::Observer* o) = 0;
  // Find an entry with the given key. Returns nullptr if the entry does not
  // exist.
  virtual const std::string* FindEntry(const std::string& key) const = 0;
  // Return all stored entries.
  virtual std::map<std::string, std::string> GetAllEntries() const = 0;
};

// A set of data exported to the XSurface datastore.
class XsurfaceDatastoreSlice : public XsurfaceDatastoreDataReader,
                               public XsurfaceDatastoreDataWriter {
 public:
  XsurfaceDatastoreSlice();
  ~XsurfaceDatastoreSlice();
  XsurfaceDatastoreSlice(const XsurfaceDatastoreSlice&) = delete;
  XsurfaceDatastoreSlice& operator=(const XsurfaceDatastoreSlice&) = delete;

  // XsurfaceDatastoreDataWriter.
  void UpdateDatastoreEntry(const std::string& key,
                            const std::string& value) override;
  void RemoveDatastoreEntry(const std::string& key) override;

  // XsurfaceDatastoreDataReader.
  void AddObserver(Observer* o) override;
  void RemoveObserver(Observer* o) override;
  const std::string* FindEntry(const std::string& key) const override;
  std::map<std::string, std::string> GetAllEntries() const override;

 private:
  base::ObserverList<Observer> observers_;
  std::map<std::string, std::string> entries_;
};

// Combines multiple `XsurfaceDatastoreDataReader`s, to provide access to all
// stored data. This implementation assumes that each contained
// `XsurfaceDatastoreDataReader` provides a disjoint set of keys, so that there
// can be no key collision between `sources`.
class XsurfaceDatastoreAggregate
    : public XsurfaceDatastoreDataReader,
      public XsurfaceDatastoreDataReader::Observer {
 public:
  explicit XsurfaceDatastoreAggregate(
      std::vector<raw_ptr<XsurfaceDatastoreDataReader>> sources);
  ~XsurfaceDatastoreAggregate() override;
  XsurfaceDatastoreAggregate(const XsurfaceDatastoreAggregate&) = delete;
  XsurfaceDatastoreAggregate& operator=(const XsurfaceDatastoreAggregate&) =
      delete;

  // XsurfaceDatastoreDataReader.
  void AddObserver(XsurfaceDatastoreDataReader::Observer* o) override;
  void RemoveObserver(XsurfaceDatastoreDataReader::Observer* o) override;
  const std::string* FindEntry(const std::string& key) const override;
  std::map<std::string, std::string> GetAllEntries() const override;

  // XsurfaceDatastoreDataReader::Observer.
  void DatastoreEntryUpdated(XsurfaceDatastoreDataReader* source,
                             const std::string& key) override;
  void DatastoreEntryRemoved(XsurfaceDatastoreDataReader* source,
                             const std::string& key) override;

 private:
  base::ObserverList<XsurfaceDatastoreDataReader::Observer> observers_;
  std::vector<raw_ptr<XsurfaceDatastoreDataReader>> sources_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_XSURFACE_DATASTORE_H_
