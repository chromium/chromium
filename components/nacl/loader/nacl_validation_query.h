// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_VALIDATION_QUERY_H_
#define COMPONENTS_NACL_LOADER_NACL_VALIDATION_QUERY_H_

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "crypto/hmac.h"

struct NaClValidationCache;
class NaClValidationDB;
class NaClValidationQuery;

class NaClValidationQueryContext {
 public:
  NaClValidationQueryContext(NaClValidationDB* db,
                             const std::string& profile_key,
                             const std::string& nacl_version);

  NaClValidationQuery* CreateQuery();

 private:
  raw_ptr<NaClValidationDB> db_;

  // A key used by HMAC that is specific to this installation of Chrome.
  std::string profile_key_;

  // Bytes indicating the "version" of the validator being used.  This is used
  // to implicitly invalidate the cache - changing the version will change the
  // hashes that are produced.
  std::string nacl_version_;
};

class NaClValidationQuery {
 public:
  // SHA256 digest size.
  static const size_t kDigestLength = 32;

  NaClValidationQuery(NaClValidationDB* db, const std::string& profile_key);

  NaClValidationQuery(const NaClValidationQuery&) = delete;
  NaClValidationQuery& operator=(const NaClValidationQuery&) = delete;

  void AddData(const char* data, size_t length);
  void AddData(const unsigned char* data, size_t length);
  void AddData(std::string_view data);

  int QueryKnownToValidate();

  void SetKnownToValidate();

 private:
  enum QueryState {
    READY,
    GET_CALLED,
    SET_CALLED
  };

  // The HMAC interface currently does not support incremental signing.  To work
  // around this, each piece of data is signed and the signature is added to a
  // buffer.  If there is not enough space in the buffer to accommodate new
  // data, the buffer contents are signed and the new signature replaces the
  // contents of the buffer.  CompressBuffer performs this operation.  In
  // affect, a hash tree is constructed to emulate incremental signing.
  void CompressBuffer();

  // Track the state of the query to detect suspicious method calls.
  QueryState state_;

  crypto::HMAC hasher_;
  raw_ptr<NaClValidationDB> db_;

  // The size of buffer_ is a somewhat arbitrary choice.  It needs to be at
  // at least kDigestLength * 2, but it can be arbitrarily large.  In practice
  // there are 4 calls to AddData (version, architechture, cpu features, and
  // code), so 4 times digest length means the buffer will not need to be
  // compressed as an intermediate step in the expected use cases.
  char buffer_[kDigestLength * 4];
  size_t buffer_length_;
};

// Create a validation cache interface for use by sel_ldr.
struct NaClValidationCache* CreateValidationCache(
    NaClValidationDB* db, const std::string& profile_key,
    const std::string& nacl_version);

#endif  // COMPONENTS_NACL_LOADER_NACL_VALIDATION_QUERY_H_
