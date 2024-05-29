// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_INDEX_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_INDEX_STORAGE_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "chromeos/ash/components/file_manager/indexing/file_info.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "url/gurl.h"

namespace ash::file_manager {

// Represents an abstract interface that maintains information necessary
// for an inverted index. This class exists so that we can offer multiple
// implementation of the inverted index: ephemeral and persistent. The first
// type is implemented in RAM and offers the highest level of performance, but
// needs to be rebuilt every time before use. The other may be implemented on
// top of SQL. It offers a slower performance, but keeps the state between
// device restarts.
//
// Please note that this class is optimized for performance. Therefore it
// takes certain shortcuts. For example, when adding term IDs it
// allows us to specify the term_id of text_bytes() part of the term. This
// term_id must be the same as generated from text_bytes(). However, for
// performance reasons this task is left to the class that implements FileIndex.
class IndexStorage {
 public:
  IndexStorage();
  virtual ~IndexStorage();

  IndexStorage(const IndexStorage&) = delete;
  IndexStorage& operator=(IndexStorage&) = delete;

  // Initializes the storage. Returns whether or not the initialization was
  // successful. No other public method may be called until this method finishes
  // and returns true.
  virtual bool Init();

  // Closes the storage. Returns true if successful.
  virtual bool Close();

  // For the given `term_id` this method returns all known URL IDs
  // that are associated with that term.
  virtual const std::set<int64_t> GetUrlIdsForTermId(int64_t term_id) const = 0;

  // Returns term IDs for the given URL.
  virtual const std::set<int64_t> GetTermIdsForUrl(int64_t url_id) const = 0;

  // Adds association between terms and the file. This method assumes that the
  // term list is not empty. Returns the number of terms added.
  virtual size_t AddTermIdsForUrl(const std::set<int64_t>& term_ids,
                                  int64_t url_id) = 0;

  // Removes association between terms and the file. Returns the number of
  // terms deleted.
  virtual size_t DeleteTermIdsForUrl(const std::set<int64_t>& term_ids,
                                     int64_t url_id) = 0;

  // Adds to the posting list of the given `term_id` the given
  // `url_id`. This may be no-op if the `url_id` already is associated with the
  // given term_id. Returns the number of URL Ids added (1 or 0).
  virtual size_t AddToPostingList(int64_t term_id, int64_t url_id) = 0;

  // This method removes the `url_id` from the posting lists of the specified
  // `term_id`. This may be a no-op if the url_id is not present on
  // the posting list for the given term. Returns the number of URLs removed.
  virtual size_t DeleteFromPostingList(int64_t term_id, int64_t url_id) = 0;

  // Returns the ID corresponding to the given term. If the term cannot be
  // located, the method returns -1.
  virtual int64_t GetTermId(const Term& term) const = 0;

  // Returns the ID corresponding to the term. If the term cannot be located,
  // a new ID is allocated and returned.
  virtual int64_t GetOrCreateTermId(const Term& term) = 0;

  // Returns the ID corresponding to the given term bytes. If the term bytes
  // cannot be located, the method returns -1.
  virtual int64_t GetTokenId(const std::string& term_bytes) const = 0;

  // Returns the ID corresponding to the given term bytes. If the term bytes
  // cannot be located, a new ID is allocated and returned.
  virtual int64_t GetOrCreateTokenId(const std::string& term_bytes) = 0;

  // Returns the ID corresponding to the given file URL. If this is the first
  // time we see this file URL, we return -1.
  virtual int64_t GetUrlId(const GURL& url) const = 0;

  // Changes URL from `from` URL to `to` URL. This keeps the same URL ID, just
  // changes the string associated with it.
  virtual int64_t MoveUrl(const GURL& from, const GURL& to) = 0;

  // Returns the ID corresponding to the given GURL. If this is the first
  // time we see this URL, a new ID is created and returned.
  virtual int64_t GetOrCreateUrlId(const GURL& url) = 0;

  // Deletes the given URL and returns its ID. If the URL was not
  // seen before, this method returns -1.
  virtual int64_t DeleteUrl(const GURL& url) = 0;

  // Stores FileInfo. If successful, returns the ID generated from `file_url`
  // field of the `file_info`. Otherwise, it returns -1.
  virtual int64_t PutFileInfo(const FileInfo& file_info) = 0;

  // Attempts to retrieve the unique FileInfo associated with the given URL.
  // If found, it is returned as the value of the optional object.
  // NO CHECK is performed whether the url_id corresponds to the `file_url`
  // field in the `info` object.
  virtual std::optional<FileInfo> GetFileInfo(int64_t url_id) const = 0;

  // Removes the given file info from the storage. If it was not stored, this
  // method returns -1. Otherwise, it returns the ID of the `url` parameter.
  virtual int64_t DeleteFileInfo(int64_t url_id) = 0;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_INDEX_STORAGE_H_
