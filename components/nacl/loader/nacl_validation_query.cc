// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_validation_query.h"

#include <stdint.h>
#include <string.h>

#include "base/check.h"
#include "components/nacl/loader/nacl_validation_db.h"
#include "crypto/nss_util.h"
#include "native_client/src/public/validation_cache.h"

NaClValidationQueryContext::NaClValidationQueryContext(
    NaClValidationDB* db,
    const std::string& profile_key,
    const std::string& nacl_version)
    : db_(db),
      profile_key_(profile_key),
      nacl_version_(nacl_version) {

  // Sanity checks.
  CHECK(profile_key.length() >= 8);
  CHECK(nacl_version.length() >= 4);
}

NaClValidationQuery* NaClValidationQueryContext::CreateQuery() {
  NaClValidationQuery* query = new NaClValidationQuery(db_, profile_key_);
  // Changing the version effectively invalidates existing hashes.
  query->AddData(nacl_version_);
  return query;
}

NaClValidationQuery::NaClValidationQuery(NaClValidationDB* db,
                                         const std::string& profile_key)
    : state_(READY),
      hasher_(crypto::HMAC::SHA256),
      db_(db),
      buffer_length_(0) {
  CHECK(hasher_.Init(profile_key));
}

void NaClValidationQuery::AddData(const char* data, size_t length) {
  CHECK(state_ == READY);
  CHECK(buffer_length_ <= sizeof(buffer_));
  // Chrome's HMAC class doesn't support incremental signing.  Work around
  // this by using a (small) temporary buffer to accumulate data.
  // Check if there is space in the buffer.
  if (buffer_length_ + kDigestLength > sizeof(buffer_)) {
    // Hash the buffer to make space.
    CompressBuffer();
  }
  // Hash the input data into the buffer.  Assumes that sizeof(buffer_) >=
  // kDigestLength * 2 (the buffer can store at least two digests.)
  CHECK(hasher_.Sign(base::StringPiece(data, length),
                     reinterpret_cast<unsigned char*>(buffer_ + buffer_length_),
                     kDigestLength));
  buffer_length_ += kDigestLength;
}

void NaClValidationQuery::AddData(const unsigned char* data, size_t length) {
  AddData(reinterpret_cast<const char*>(data), length);
}

void NaClValidationQuery::AddData(const base::StringPiece& data) {
  AddData(data.data(), data.length());
}

int NaClValidationQuery::QueryKnownToValidate() {
  CHECK(state_ == READY);
  // It is suspicious if we have less than a digest's worth of data.
  CHECK(buffer_length_ >= kDigestLength);
  CHECK(buffer_length_ <= sizeof(buffer_));
  state_ = GET_CALLED;
  // Ensure the buffer contains only one digest worth of data.
  CompressBuffer();
  return db_->QueryKnownToValidate(std::string(buffer_, buffer_length_));
}

void NaClValidationQuery::SetKnownToValidate() {
  CHECK(state_ == GET_CALLED);
  CHECK(buffer_length_ == kDigestLength);
  state_ = SET_CALLED;
  db_->SetKnownToValidate(std::string(buffer_, buffer_length_));
}

// Reduce the size of the data in the buffer by hashing it and writing it back
// to the buffer.
void NaClValidationQuery::CompressBuffer() {
  // Calculate the digest into a temp buffer.  It is likely safe to calculate it
  // directly back into the buffer, but this is an "accidental" semantic we're
  // avoiding depending on.
  unsigned char temp[kDigestLength];
  CHECK(hasher_.Sign(base::StringPiece(buffer_, buffer_length_), temp,
                     kDigestLength));
  memcpy(buffer_, temp, kDigestLength);
  buffer_length_ = kDigestLength;
}

// OO wrappers

static void* CreateQuery(void* handle) {
  return static_cast<NaClValidationQueryContext*>(handle)->CreateQuery();
}

static void AddData(void* query, const uint8_t* data, size_t length) {
  static_cast<NaClValidationQuery*>(query)->AddData(data, length);
}

static int QueryKnownToValidate(void* query) {
  return static_cast<NaClValidationQuery*>(query)->QueryKnownToValidate();
}

static void SetKnownToValidate(void* query) {
  static_cast<NaClValidationQuery*>(query)->SetKnownToValidate();
}

static void DestroyQuery(void* query) {
  delete static_cast<NaClValidationQuery*>(query);
}

struct NaClValidationCache* CreateValidationCache(
    NaClValidationDB* db, const std::string& profile_key,
    const std::string& nacl_version) {
  NaClValidationCache* cache =
      static_cast<NaClValidationCache*>(malloc(sizeof(NaClValidationCache)));
  // Make sure any fields introduced in a cross-repo change are zeroed.
  memset(cache, 0, sizeof(*cache));
  cache->handle = new NaClValidationQueryContext(db, profile_key, nacl_version);
  cache->CreateQuery = CreateQuery;
  cache->AddData = AddData;
  cache->QueryKnownToValidate = QueryKnownToValidate;
  cache->SetKnownToValidate = SetKnownToValidate;
  cache->DestroyQuery = DestroyQuery;
  return cache;
}
