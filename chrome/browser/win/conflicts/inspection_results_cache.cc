// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/inspection_results_cache.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/md5.h"
#include "base/pickle.h"
#include "base/strings/string_piece.h"

namespace {

// The current version of the cache format. Must be incremented every time the
// format changes.
constexpr int kVersion = 1;

// Serializes a [ModuleInfoKey, ModuleInspectionResult, time stamp] tuple into
// |pickle|. The |time_stamp| represents the last time this specific inspection
// result was used, and was calculated using CalculateTimeStamp().
void SerializeInspectionResult(const ModuleInfoKey& module_key,
                               const ModuleInspectionResult& inspection_result,
                               uint32_t time_stamp,
                               base::Pickle* pickle) {
  // ModuleInfoKey:
  module_key.module_path.WriteToPickle(pickle);
  pickle->WriteUInt32(module_key.module_size);
  pickle->WriteUInt32(module_key.module_time_date_stamp);

  // ModuleInspectionResult:
  pickle->WriteString16(inspection_result.location);
  pickle->WriteString16(inspection_result.basename);
  pickle->WriteString16(inspection_result.product_name);
  pickle->WriteString16(inspection_result.description);
  pickle->WriteString16(inspection_result.version);
  pickle->WriteInt(static_cast<int>(inspection_result.certificate_info.type));
  inspection_result.certificate_info.path.WriteToPickle(pickle);
  pickle->WriteString16(inspection_result.certificate_info.subject);

  // Time stamp:
  pickle->WriteUInt32(time_stamp);
}

// Deserializes a [ModuleInfoKey, ModuleInspectionResult] pair into |result| by
// reading from |pickle_iterator|. Skips pairs whose time stamp is older than
// |min_time_stamp|. Returns true if |result| contains a valid inspection
// result.
bool DeserializeInspectionResult(uint32_t min_time_stamp,
                                 base::PickleIterator* pickle_iterator,
                                 InspectionResultsCache* result) {
  DCHECK(pickle_iterator);
  DCHECK(result);

  // ModuleInfoKey:
  base::FilePath module_path;
  uint32_t module_size = 0;
  uint32_t module_time_date_stamp = 0;
  if (!module_path.ReadFromPickle(pickle_iterator) ||
      !pickle_iterator->ReadUInt32(&module_size) ||
      !pickle_iterator->ReadUInt32(&module_time_date_stamp)) {
    return false;
  }

  std::pair<ModuleInfoKey, std::pair<ModuleInspectionResult, uint32_t>> value(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(module_path), module_size,
                            module_time_date_stamp),
      std::forward_as_tuple());

  // ModuleInspectionResult and time stamp:
  ModuleInspectionResult& inspection_result = value.second.first;
  uint32_t& time_stamp = value.second.second;

  if (!pickle_iterator->ReadString16(&inspection_result.location) ||
      !pickle_iterator->ReadString16(&inspection_result.basename) ||
      !pickle_iterator->ReadString16(&inspection_result.product_name) ||
      !pickle_iterator->ReadString16(&inspection_result.description) ||
      !pickle_iterator->ReadString16(&inspection_result.version) ||
      !pickle_iterator->ReadInt(
          reinterpret_cast<int*>(&inspection_result.certificate_info.type)) ||
      !inspection_result.certificate_info.path.ReadFromPickle(
          pickle_iterator) ||
      !pickle_iterator->ReadString16(
          &inspection_result.certificate_info.subject) ||
      !pickle_iterator->ReadUInt32(&time_stamp)) {
    return false;
  }

  // Only keep this element if it is not expired. An expired entry is not an
  // error.
  if (time_stamp >= min_time_stamp)
    result->insert(std::move(value));

  return true;
}

// Serializes an InspectionResultsCache into a base::Pickle. The serialized data
// contains a version number and a MD5 checksum at the end.
base::Pickle SerializeInspectionResultsCache(
    const InspectionResultsCache& inspection_results_cache) {
  base::Pickle pickle;

  pickle.WriteInt(kVersion);
  pickle.WriteUInt64(inspection_results_cache.size());

  for (const auto& inspection_result : inspection_results_cache) {
    SerializeInspectionResult(inspection_result.first,
                              inspection_result.second.first,
                              inspection_result.second.second, &pickle);
  }

  // Append the md5 digest of the data to detect serializations errors.
  base::MD5Digest md5_digest;
  base::MD5Sum(pickle.payload(), pickle.payload_size(), &md5_digest);
  pickle.WriteBytes(&md5_digest, sizeof(md5_digest));

  return pickle;
}

// Deserializes an InspectionResultsCache from |pickle|. This function ensures
// that both the version and the checksum of the data are valid. Returns a
// ReadCacheResult value indicating what failed if unsuccessful.
ReadCacheResult DeserializeInspectionResultsCache(
    uint32_t min_time_stamp,
    const base::Pickle& pickle,
    InspectionResultsCache* result) {
  DCHECK(result);

  base::PickleIterator pickle_iterator(pickle);

  // Check the version number.
  int version = 0;
  if (!pickle_iterator.ReadInt(&version))
    return ReadCacheResult::kFailDeserializeVersion;
  if (version != kVersion)
    return ReadCacheResult::kFailInvalidVersion;

  // Retrieve the count of inspection results.
  uint64_t inspection_result_count = 0;
  if (!pickle_iterator.ReadUInt64(&inspection_result_count))
    return ReadCacheResult::kFailDeserializeCount;
  if (inspection_result_count < 0)
    return ReadCacheResult::kFailInvalidCount;

  // Now deserialize all the ModuleInspectionResults. Failure to deserialize one
  // inspection result counts as an invalid cache.
  for (uint64_t i = 0; i < inspection_result_count; i++) {
    if (!DeserializeInspectionResult(min_time_stamp, &pickle_iterator, result))
      return ReadCacheResult::kFailDeserializeInspectionResult;
  }

  // Now check the md5 checksum.
  const base::MD5Digest* read_md5_digest = nullptr;
  if (!pickle_iterator.ReadBytes(
          reinterpret_cast<const char**>(&read_md5_digest),
          sizeof(*read_md5_digest))) {
    return ReadCacheResult::kFailDeserializeMD5;
  }

  // Check if the md5 checksum matches.
  base::MD5Digest md5_digest;
  base::MD5Sum(pickle.payload(), pickle.payload_size() - sizeof(md5_digest),
               &md5_digest);
  if (!std::equal(std::begin(read_md5_digest->a), std::end(read_md5_digest->a),
                  std::begin(md5_digest.a), std::end(md5_digest.a)))
    return ReadCacheResult::kFailInvalidMD5;

  return ReadCacheResult::kSuccess;
}

}  // namespace

void AddInspectionResultToCache(
    const ModuleInfoKey& module_key,
    const ModuleInspectionResult& inspection_result,
    InspectionResultsCache* inspection_results_cache) {
  auto insert_result = inspection_results_cache->insert(std::make_pair(
      module_key, std::make_pair(inspection_result,
                                 CalculateTimeStamp(base::Time::Now()))));
  // An insertion should always succeed because the user of this code never
  // tries to insert an existing value.
  DCHECK(insert_result.second);
}

base::Optional<ModuleInspectionResult> GetInspectionResultFromCache(
    const ModuleInfoKey& module_key,
    InspectionResultsCache* inspection_results_cache) {
  base::Optional<ModuleInspectionResult> inspection_result;

  auto it = inspection_results_cache->find(module_key);
  if (it != inspection_results_cache->end()) {
    // Update the time stamp.
    it->second.second = CalculateTimeStamp(base::Time::Now());
    inspection_result = it->second.first;
  }

  return inspection_result;
}

ReadCacheResult ReadInspectionResultsCache(
    const base::FilePath& file_path,
    uint32_t min_time_stamp,
    InspectionResultsCache* inspection_results_cache) {
  if (!base::FeatureList::IsEnabled(kInspectionResultsCache))
    return ReadCacheResult::kSuccess;

  std::string contents;
  if (!ReadFileToString(file_path, &contents))
    return ReadCacheResult::kFailReadFile;

  base::Pickle pickle(contents.data(), contents.length());
  InspectionResultsCache temporary_result;
  ReadCacheResult read_result = DeserializeInspectionResultsCache(
      min_time_stamp, pickle, &temporary_result);

  // Only update the output cache when successful.
  if (read_result == ReadCacheResult::kSuccess)
    *inspection_results_cache = std::move(temporary_result);

  return read_result;
}

bool WriteInspectionResultsCache(
    const base::FilePath& file_path,
    const InspectionResultsCache& inspection_results_cache) {
  if (!base::FeatureList::IsEnabled(kInspectionResultsCache))
    return true;

  base::Pickle pickle =
      SerializeInspectionResultsCache(inspection_results_cache);

  // TODO(1022041): Investigate if using WriteFileAtomically() in a
  // CONTINUE_ON_SHUTDOWN sequence can cause too many corrupted caches.
  return base::ImportantFileWriter::WriteFileAtomically(
      file_path, base::StringPiece(static_cast<const char*>(pickle.data()),
                                   pickle.size()));
}
