// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_CACHE_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_CACHE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_checker.h"
#include "components/search_provider_logos/logo_common.h"

namespace search_provider_logos {

// A file-based cache for the search provider's logo. This allows clients to
// store and retrieve a logo (of type EncodedLogo) and its associated metadata
// (of type LogoMetadata). Metadata can be updated independently from the logo
// to handle cases where, e.g. the expiration date changes, but the logo stays
// the same. If corruption is detected in the metadata or logo, the cache will
// be cleared.
//
// Note: this class must only be used on a single thread. All methods are
// are blocking, so this should not be used on the UI thread.
//
// The logo and its metadata are stored in files, so they persist even when
// Chrome is closed. Once loaded from disk, the metadata is kept in memory to
// enable quick retrieval. The logo is not kept around in memory because of its
// size.
class LogoCache {
 public:
  // Constructs a logo cache that stores data in |cache_directory|.
  // |cache_directory| will be created if it does not already exist.
  explicit LogoCache(const base::FilePath& cache_directory);

  LogoCache(const LogoCache&) = delete;
  LogoCache& operator=(const LogoCache&) = delete;

  virtual ~LogoCache();

  // Updates the metadata for the cached logo.
  virtual void UpdateCachedLogoMetadata(const LogoMetadata& metadata);

  // Returns metadata for the cached logo, or null if logo is cached.
  virtual const LogoMetadata* GetCachedLogoMetadata();

  // Sets the cached logo and metadata. |logo| may be null, in which case the
  // cached logo and metadata will be cleared.
  virtual void SetCachedLogo(const EncodedLogo* logo);

  // Returns the cached logo, or null if no logo is cached or the cached logo is
  // corrupt.
  virtual std::unique_ptr<EncodedLogo> GetCachedLogo();

 private:
  FRIEND_TEST_ALL_PREFIXES(LogoCacheSerializationTest, SerializeMetadata);
  FRIEND_TEST_ALL_PREFIXES(LogoCacheSerializationTest,
                           DeserializeCorruptMetadata);
  FRIEND_TEST_ALL_PREFIXES(LogoCacheTest, StoreAndRetrieveMetadata);
  FRIEND_TEST_ALL_PREFIXES(LogoCacheTest, RetrieveCorruptMetadata);
  FRIEND_TEST_ALL_PREFIXES(LogoCacheTest, RetrieveCorruptLogo);

  // Converts string |str| to a LogoMetadata object and returns it. Returns null
  // if |str| cannot be converted.
  static std::unique_ptr<LogoMetadata> LogoMetadataFromString(
      const std::string& str,
      int* logo_num_bytes,
      int* dark_logo_num_bytes);

  // Converts |metadata| to a string and stores it in |str|.
  static void LogoMetadataToString(const LogoMetadata& metadata,
                                   int logo_num_bytes,
                                   int dark_logo_num_bytes,
                                   std::string* str);

  // Returns the path where the cached logo will be saved.
  base::FilePath GetLogoPath();
  base::FilePath GetDarkLogoPath();

  // Returns the path where the metadata for the cached logo will be saved.
  base::FilePath GetMetadataPath();

  // Updates the in-memory metadata.
  void UpdateMetadata(std::unique_ptr<LogoMetadata> metadata);

  // If the cached logo's metadata isn't available in memory (i.e.
  // |metadata_is_valid_| is false), reads it from disk and stores it in
  // |metadata_|. If no logo is cached, |metadata_| will be updated to null.
  void ReadMetadataIfNeeded();

  // Writes the metadata for the cached logo to disk.
  void WriteMetadata();

  // Writes |metadata_| to the cached metadata file and |encoded_image| to the
  // cached logo file.
  void WriteLogo(scoped_refptr<base::RefCountedMemory> encoded_image,
                 scoped_refptr<base::RefCountedMemory> dark_encoded_image);

  // Deletes all the cache files.
  void DeleteLogoAndMetadata();

  // Tries to create the cache directory if it does not already exist. Returns
  // whether the cache directory exists.
  bool EnsureCacheDirectoryExists();

  // The directory in which the cached logo and metadata will be saved.
  base::FilePath cache_directory_;

  // The metadata describing the cached logo, or null if no logo is cached. This
  // value is meaningful iff |metadata_is_valid_| is true; otherwise, the
  // metadata must be read from file and |metadata_| will be null.
  // Note: Once read from file, metadata will be stored in memory indefinitely.
  std::unique_ptr<LogoMetadata> metadata_;
  bool metadata_is_valid_;

  // The number of bytes in the logo file, as recorded in the metadata file.
  // Valid iff |metadata_is_valid_|. This is used to verify that the logo file
  // is complete and corresponds to the current metadata file.
  int logo_num_bytes_ = 0;
  int dark_logo_num_bytes_ = 0;

  // Ensure LogoCache is only used sequentially.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_CACHE_H_
