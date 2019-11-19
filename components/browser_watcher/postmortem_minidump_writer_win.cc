// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note: aside from using windows headers to obtain the definitions of minidump
//     structures, nothing here is windows specific. This seems like the best
//     approach given this code is for temporary experimentation on Windows.
//     Longer term, Crashpad will take over the minidump writing in this case as
//     well.

#include "components/browser_watcher/postmortem_minidump_writer.h"

#include <windows.h>  // NOLINT
#include <dbghelp.h>

#include <map>
#include <type_traits>
#include <vector>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_piece.h"
#include "components/browser_watcher/minidump_user_streams.h"
#include "components/browser_watcher/stability_data_names.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"

namespace browser_watcher {

namespace {

struct ProductDetails {
  std::string product;
  std::string channel;
  std::string platform;
  std::string version;
};

bool GetStringFromTypedValueMap(
    const google::protobuf::Map<std::string, TypedValue>& map,
    const std::string& key,
    std::string* out) {
  DCHECK(out);

  auto it = map.find(key);
  if (it == map.end())
    return false;

  const TypedValue& value = it->second;
  if (value.value_case() != TypedValue::kStringValue)
    return false;

  *out = value.string_value();
  return true;
}

bool GetProductDetails(
    const google::protobuf::Map<std::string, TypedValue>& global_data,
    ProductDetails* product_details) {
  DCHECK(product_details);

  if (!GetStringFromTypedValueMap(global_data, kStabilityProduct,
                                  &(product_details->product)))
    return false;
  if (!GetStringFromTypedValueMap(global_data, kStabilityChannel,
                                  &(product_details->channel)))
    return false;
  if (!GetStringFromTypedValueMap(global_data, kStabilityPlatform,
                                  &(product_details->platform)))
    return false;
  return GetStringFromTypedValueMap(global_data, kStabilityVersion,
                                    &(product_details->version));
}

// A class with functionality for writing minimal minidump containers to wrap
// postmortem stability reports.
// TODO(manzagop): remove this class once Crashpad takes over writing postmortem
//     minidumps.
// TODO(manzagop): revisit where the module information should be transported,
//     in the protocol buffer or in a module stream.
class PostmortemMinidumpWriter {
 public:
  // DO NOT CHANGE VALUES. This is logged persistently in a histogram.
  enum WriteStatus {
    SUCCESS = 0,
    FAILED = 1,
    FAILED_MISSING_PRODUCT_DETAILS = 2,
    WRITE_STATUS_MAX = 3
  };

  // |minidump_file| is expected to be empty and a binary stream.
  PostmortemMinidumpWriter(crashpad::FileWriterInterface* minidump_file);
  ~PostmortemMinidumpWriter();

  // Write to |minidump_file_| a minimal minidump that wraps |report|. Returns
  // true on success, false otherwise.
  bool WriteDump(const crashpad::UUID& client_id,
                 const crashpad::UUID& report_id,
                 StabilityReport* report);

 private:
  // An offset within a minidump file. Note: using this type to avoid including
  // windows.h and relying on the RVA type.
  using FilePosition = uint32_t;

  // The minidump header is always located at the head.
  static const FilePosition kHeaderPos = 0U;

  bool WriteDumpImpl(const StabilityReport& report,
                     const crashpad::UUID& client_id,
                     const crashpad::UUID& report_id,
                     const ProductDetails& product_details);

  bool AppendCrashpadInfo(const crashpad::UUID& client_id,
                          const crashpad::UUID& report_id,
                          const std::map<std::string, std::string>& crash_keys);

  bool AppendCrashpadDictionaryEntry(
      const std::string& key,
      const std::string& value,
      std::vector<crashpad::MinidumpSimpleStringDictionaryEntry>* entries);

  // Allocate |size_bytes| within the minidump. On success, |pos| contains the
  // location of the allocation. Returns true on success, false otherwise.
  bool Allocate(size_t size_bytes, FilePosition* pos);

  // Seeks |cursor_|. The seek operation is kept separate from the write in
  // order to make the call explicit. Seek operations can be costly and should
  // be avoided.
  bool SeekCursor(FilePosition destination);

  // Write to pre-allocated space.
  // Note: |pos| must match |cursor_|.
  template <class DataType>
  bool Write(FilePosition pos, const DataType& data);
  bool WriteBytes(FilePosition pos, size_t size_bytes, const char* data);

  // Allocate space for and write the contents of |data|. On success, |pos|
  // contains the location of the write. Returns true on success, false
  // otherwise.
  template <class DataType>
  bool Append(const DataType& data, FilePosition* pos);
  template <class DataType>
  bool AppendVec(const std::vector<DataType>& data, FilePosition* pos);
  bool AppendUtf8String(base::StringPiece data, FilePosition* pos);
  bool AppendBytes(base::StringPiece data, FilePosition* pos);

  void RegisterDirectoryEntry(uint32_t stream_type,
                              FilePosition pos,
                              uint32_t size);

  // The next allocatable FilePosition.
  FilePosition next_available_byte_;

  // Storage for the directory during writes.
  std::vector<MINIDUMP_DIRECTORY> directory_;

  // The file to write to.
  crashpad::FileWriterInterface* minidump_file_;

  DISALLOW_COPY_AND_ASSIGN(PostmortemMinidumpWriter);
};

void RecordWriteDumpStatus(PostmortemMinidumpWriter::WriteStatus status) {
  base::UmaHistogramEnumeration("ActivityTracker.Collect.WriteDumpStatus",
                                status,
                                PostmortemMinidumpWriter::WRITE_STATUS_MAX);
}

PostmortemMinidumpWriter::PostmortemMinidumpWriter(
    crashpad::FileWriterInterface* minidump_file)
    : next_available_byte_(0U), minidump_file_(minidump_file) {
  DCHECK_NE(nullptr, minidump_file_);
  DCHECK_EQ(0LL, minidump_file_->SeekGet());
  DCHECK_EQ(0LL, minidump_file_->Seek(0LL, SEEK_END));
}

PostmortemMinidumpWriter::~PostmortemMinidumpWriter() {}

bool PostmortemMinidumpWriter::WriteDump(
    const crashpad::UUID& client_id,
    const crashpad::UUID& report_id,
    StabilityReport* report) {
  DCHECK(report);

  DCHECK_EQ(0U, next_available_byte_);
  DCHECK(directory_.empty());

  // Ensure the report contains the crasher's product details.
  ProductDetails product_details = {};
  if (!GetProductDetails(report->global_data(), &product_details)) {
    // The report is missing the basic information to determine the affected
    // version. Ignore the report.
    RecordWriteDumpStatus(FAILED_MISSING_PRODUCT_DETAILS);
    return false;
  }

  // No need to keep the version details inside the report.
  report->mutable_global_data()->erase(kStabilityProduct);
  report->mutable_global_data()->erase(kStabilityChannel);
  report->mutable_global_data()->erase(kStabilityPlatform);
  report->mutable_global_data()->erase(kStabilityVersion);

  // Write the minidump, then reset members.
  bool success = WriteDumpImpl(*report, client_id, report_id, product_details);
  next_available_byte_ = 0U;
  directory_.clear();

  RecordWriteDumpStatus(success ? SUCCESS : FAILED);
  return success;
}

bool PostmortemMinidumpWriter::WriteDumpImpl(
    const StabilityReport& report,
    const crashpad::UUID& client_id,
    const crashpad::UUID& report_id,
    const ProductDetails& product_details) {
  // Allocate space for the header and seek the cursor.
  FilePosition pos = 0U;
  if (!Allocate(sizeof(MINIDUMP_HEADER), &pos))
    return false;
  if (!SeekCursor(sizeof(MINIDUMP_HEADER)))
    return false;
  DCHECK_EQ(kHeaderPos, pos);

  // Write the proto to the file.
  std::string serialized_report;
  if (!report.SerializeToString(&serialized_report))
    return false;
  FilePosition report_pos = 0U;
  if (!AppendBytes(serialized_report, &report_pos))
    return false;

  // The directory entry for the stability report's stream.
  RegisterDirectoryEntry(kStabilityReportStreamType, report_pos,
                         serialized_report.length());

  // Write mandatory crash keys. These will be read by crashpad and used as
  // http request parameters for the upload. Keys and values should match
  // server side configuration.
  // TODO(manzagop): use product and version from the stability report. The
  // current executable's values are an (imperfect) proxy.
  std::map<std::string, std::string> crash_keys = {
      {"prod", product_details.product + "_Postmortem"},
      {"ver", product_details.version},
      {"channel", product_details.channel},
      {"plat", product_details.platform}};
  if (!AppendCrashpadInfo(client_id, report_id, crash_keys))
    return false;

  // Write the directory.
  FilePosition directory_pos = 0U;
  if (!AppendVec(directory_, &directory_pos))
    return false;

  // Write the header.
  MINIDUMP_HEADER header;
  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = directory_.size();
  header.StreamDirectoryRva = directory_pos;
  if (!SeekCursor(0U))
    return false;
  return Write(kHeaderPos, header);
}

bool PostmortemMinidumpWriter::AppendCrashpadInfo(
    const crashpad::UUID& client_id,
    const crashpad::UUID& report_id,
    const std::map<std::string, std::string>& crash_keys) {
  // Write the crash keys as the contents of a crashpad dictionary.
  std::vector<crashpad::MinidumpSimpleStringDictionaryEntry> entries;
  for (const auto& crash_key : crash_keys) {
    if (!AppendCrashpadDictionaryEntry(crash_key.first, crash_key.second,
                                       &entries)) {
      return false;
    }
  }

  // Write the dictionary's index.
  FilePosition dict_pos = 0U;
  uint32_t entry_count = entries.size();
  if (entry_count > 0) {
    if (!Append(entry_count, &dict_pos))
      return false;
    FilePosition unused_pos = 0U;
    if (!AppendVec(entries, &unused_pos))
      return false;
  }

  MINIDUMP_LOCATION_DESCRIPTOR simple_annotations = {0};
  simple_annotations.DataSize = 0U;
  if (entry_count > 0)
    simple_annotations.DataSize = next_available_byte_ - dict_pos;
  // Note: an RVA of 0 indicates the absence of a dictionary.
  simple_annotations.Rva = dict_pos;

  // Write the crashpad info.
  crashpad::MinidumpCrashpadInfo crashpad_info;
  crashpad_info.version = crashpad::MinidumpCrashpadInfo::kVersion;
  crashpad_info.report_id = report_id;
  crashpad_info.client_id = client_id;
  crashpad_info.simple_annotations = simple_annotations;
  // Note: module_list is left at 0, which means none.

  FilePosition crashpad_pos = 0U;
  if (!Append(crashpad_info, &crashpad_pos))
    return false;

  // Append a directory entry for the crashpad info stream.
  RegisterDirectoryEntry(crashpad::kMinidumpStreamTypeCrashpadInfo,
                         crashpad_pos, sizeof(crashpad::MinidumpCrashpadInfo));

  return true;
}

bool PostmortemMinidumpWriter::AppendCrashpadDictionaryEntry(
    const std::string& key,
    const std::string& value,
    std::vector<crashpad::MinidumpSimpleStringDictionaryEntry>* entries) {
  DCHECK_NE(nullptr, entries);

  FilePosition key_pos = 0U;
  if (!AppendUtf8String(key, &key_pos))
    return false;
  FilePosition value_pos = 0U;
  if (!AppendUtf8String(value, &value_pos))
    return false;

  crashpad::MinidumpSimpleStringDictionaryEntry entry = {0};
  entry.key = key_pos;
  entry.value = value_pos;
  entries->push_back(entry);

  return true;
}

bool PostmortemMinidumpWriter::Allocate(size_t size_bytes, FilePosition* pos) {
  DCHECK(pos);
  *pos = next_available_byte_;

  base::CheckedNumeric<FilePosition> next = next_available_byte_;
  next += size_bytes;
  if (!next.IsValid())
    return false;

  next_available_byte_ += size_bytes;
  return true;
}

bool PostmortemMinidumpWriter::SeekCursor(FilePosition destination) {
  // Validate the write does not extend past the allocated space.
  if (destination > next_available_byte_)
    return false;

  return minidump_file_->SeekSet(destination);
}

template <class DataType>
bool PostmortemMinidumpWriter::Write(FilePosition pos, const DataType& data) {
  static_assert(std::is_trivially_copyable<DataType>::value,
                "restricted to trivially copyable");
  return WriteBytes(pos, sizeof(data), reinterpret_cast<const char*>(&data));
}

bool PostmortemMinidumpWriter::WriteBytes(FilePosition pos,
                                          size_t size_bytes,
                                          const char* data) {
  DCHECK(data);
  DCHECK_EQ(static_cast<int64_t>(pos), minidump_file_->SeekGet());

  // Validate the write does not extend past the next available byte.
  base::CheckedNumeric<FilePosition> pos_end = pos;
  pos_end += size_bytes;
  if (!pos_end.IsValid() || pos_end.ValueOrDie() > next_available_byte_)
    return false;

  return minidump_file_->Write(data, size_bytes);
}

template <class DataType>
bool PostmortemMinidumpWriter::Append(const DataType& data, FilePosition* pos) {
  static_assert(std::is_trivially_copyable<DataType>::value,
                "restricted to trivially copyable");
  DCHECK(pos);
  if (!Allocate(sizeof(data), pos))
    return false;
  return Write(*pos, data);
}

template <class DataType>
bool PostmortemMinidumpWriter::AppendVec(const std::vector<DataType>& data,
                                         FilePosition* pos) {
  static_assert(std::is_trivially_copyable<DataType>::value,
                "restricted to trivially copyable");
  DCHECK(!data.empty());
  DCHECK(pos);

  size_t size_bytes = sizeof(DataType) * data.size();
  if (!Allocate(size_bytes, pos))
    return false;
  return WriteBytes(*pos, size_bytes,
                    reinterpret_cast<const char*>(&data.at(0)));
}

bool PostmortemMinidumpWriter::AppendUtf8String(base::StringPiece data,
                                                FilePosition* pos) {
  DCHECK(pos);
  uint32_t string_size = data.size();
  if (!Append(string_size, pos))
    return false;

  FilePosition unused_pos = 0U;
  return AppendBytes(data, &unused_pos);
}

bool PostmortemMinidumpWriter::AppendBytes(base::StringPiece data,
                                           FilePosition* pos) {
  DCHECK(pos);
  if (!Allocate(data.length(), pos))
    return false;
  return WriteBytes(*pos, data.length(), data.data());
}

void PostmortemMinidumpWriter::RegisterDirectoryEntry(uint32_t stream_type,
                                                      FilePosition pos,
                                                      uint32_t size) {
  MINIDUMP_DIRECTORY entry = {0};
  entry.StreamType = stream_type;
  entry.Location.Rva = pos;
  entry.Location.DataSize = size;
  directory_.push_back(entry);
}

}  // namespace

bool WritePostmortemDump(crashpad::FileWriterInterface* minidump_file,
                         const crashpad::UUID& client_id,
                         const crashpad::UUID& report_id,
                         StabilityReport* report) {
  DCHECK(report);

  PostmortemMinidumpWriter writer(minidump_file);
  return writer.WriteDump(client_id, report_id, report);
}

}  // namespace browser_watcher
