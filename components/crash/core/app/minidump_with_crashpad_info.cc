// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/minidump_with_crashpad_info.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"

namespace crash_reporter {

namespace {

using FilePosition = uint32_t;

// This class is a helper to edit minidump files written by MiniDumpWriteDump.
// It assumes the minidump file it operates on has a directory entry pointing to
// a CrashpadInfo entry, which it updates to point to the SimpleDictionary data
// it appends to the file contents.
class MinidumpUpdater {
 public:
  MinidumpUpdater();

  MinidumpUpdater(const MinidumpUpdater&) = delete;
  MinidumpUpdater& operator=(const MinidumpUpdater&) = delete;

  // Reads the existing directory from |file|.
  bool Initialize(base::File* file);

  // Appends the simple dictionary with |crash_keys| to the file, and updates
  // the CrashpadInfo with its location.
  bool AppendSimpleDictionary(const StringStringMap& crash_keys);

 private:
  // Writes |data_len| bytes from |data| to the file at the current location.
  bool WriteData(const void* data, size_t data_len);
  bool WriteAndAdvance(const void* data,
                       size_t data_len,
                       FilePosition* position);

  raw_ptr<base::File> file_;
  std::vector<MINIDUMP_DIRECTORY> directory_;
};

MinidumpUpdater::MinidumpUpdater() : file_(nullptr) {}

bool MinidumpUpdater::Initialize(base::File* file) {
  DCHECK(file && file->IsValid());
  DCHECK(!file_);

  // Read the file header.
  MINIDUMP_HEADER header = {};
  int bytes_read =
      file->Read(0, reinterpret_cast<char*>(&header), sizeof(header));
  if (bytes_read != sizeof(header))
    return false;
  if (header.Signature != MINIDUMP_SIGNATURE || header.NumberOfStreams == 0)
    return false;

  // Read the stream directory.
  directory_.resize(header.NumberOfStreams);
  int bytes_to_read = header.NumberOfStreams * sizeof(directory_[0]);
  bytes_read =
      file->Read(header.StreamDirectoryRva,
                 reinterpret_cast<char*>(&directory_[0]), bytes_to_read);
  if (bytes_read != bytes_to_read)
    return false;

  // Crashpad has some fairly unreasonable checking on the minidump header and
  // directory. Match with those checks for now to allow Crashpad to read the
  // CrashpadInfo and upload these dumps.

  // Start by removing any unused directory entries.
  // TODO(siggi): Fix Crashpad to ignore unused streams.
  std::erase_if(directory_, [](const MINIDUMP_DIRECTORY& entry) {
    return entry.StreamType == UnusedStream;
  });

  // Update the header.
  // TODO(siggi): Fix Crashpad's version checking.
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = base::saturated_cast<ULONG32>(directory_.size());

  // Write back the potentially shortened and packed dictionary.
  int bytes_to_write = header.NumberOfStreams * sizeof(directory_[0]);
  int bytes_written = file->Write(header.StreamDirectoryRva,
                                  reinterpret_cast<const char*>(&directory_[0]),
                                  bytes_to_write);
  if (bytes_written != bytes_to_write)
    return false;

  // Write back the header.
  bytes_written =
      file->Write(0, reinterpret_cast<const char*>(&header), sizeof(header));
  if (bytes_written != sizeof(header))
    return false;

  // Success, stash the file.
  file_ = file;

  return true;
}

bool MinidumpUpdater::AppendSimpleDictionary(
    const StringStringMap& crash_keys) {
  DCHECK(file_);

  // Start by finding the Crashpad directory entry and reading the CrashpadInfo.
  FilePosition crashpad_info_pos = 0;
  crashpad::MinidumpCrashpadInfo crashpad_info;
  for (const auto& entry : directory_) {
    if (entry.StreamType == crashpad::kMinidumpStreamTypeCrashpadInfo) {
      // This file is freshly written, so it must contain the same version
      // CrashpadInfo structure this code compiled against.
      if (entry.Location.DataSize != sizeof(crashpad_info))
        return false;

      crashpad_info_pos = entry.Location.Rva;
      break;
    }
  }

  // No CrashpadInfo directory entry found.
  if (crashpad_info_pos == 0)
    return false;

  int bytes_read =
      file_->Read(crashpad_info_pos, reinterpret_cast<char*>(&crashpad_info),
                  sizeof(crashpad_info));
  if (bytes_read != sizeof(crashpad_info))
    return false;

  if (crashpad_info.version != crashpad::MinidumpCrashpadInfo::kVersion)
    return false;

  // Seek to the tail of the file, where we're going to extend it.
  int64_t seek_position = file_->Seek(base::File::FROM_END, 0);

  if (seek_position == -1 ||
      seek_position > std::numeric_limits<FilePosition>::max()) {
    return false;
  }

  FilePosition next_available_byte = static_cast<FilePosition>(seek_position);

  // Write the key/value pairs and collect their locations.
  std::vector<crashpad::MinidumpSimpleStringDictionaryEntry> entries;
  for (const auto& kv : crash_keys) {
    // The key of a key/value pair should never be empty.
    DCHECK(!kv.first.empty());

    crashpad::MinidumpSimpleStringDictionaryEntry entry = {0};

    // Skip this key/value if the value is empty.
    if (!kv.second.empty()) {
      entry.key = next_available_byte;
      uint32_t key_len = base::saturated_cast<uint32_t>(kv.first.size());
      if (!WriteAndAdvance(&key_len, sizeof(key_len), &next_available_byte) ||
          !WriteAndAdvance(&kv.first[0], key_len, &next_available_byte)) {
        return false;
      }

      entry.value = next_available_byte;
      uint32_t value_len = base::saturated_cast<uint32_t>(kv.second.size());
      if (!WriteAndAdvance(&value_len, sizeof(value_len),
                           &next_available_byte) ||
          !WriteAndAdvance(&kv.second[0], value_len, &next_available_byte)) {
        return false;
      }

      entries.push_back(entry);
    }
  }

  // Write the dictionary array itself - note the array is count-prefixed.
  FilePosition dict_pos = next_available_byte;
  uint32_t entry_count = base::saturated_cast<uint32_t>(entries.size());
  if (!WriteAndAdvance(&entry_count, sizeof(entry_count),
                       &next_available_byte) ||
      !WriteAndAdvance(&entries[0], entry_count * sizeof(entries[0]),
                       &next_available_byte)) {
    return false;
  }

  // Touch up the CrashpadInfo and write it back to the file.
  crashpad_info.simple_annotations.DataSize = next_available_byte - dict_pos;
  crashpad_info.simple_annotations.Rva = dict_pos;

  int bytes_written = file_->Write(
      crashpad_info_pos, reinterpret_cast<const char*>(&crashpad_info),
      sizeof(crashpad_info));
  if (bytes_written != sizeof(crashpad_info))
    return false;

  return true;
}

bool MinidumpUpdater::WriteData(const void* data, size_t data_len) {
  DCHECK(file_);
  DCHECK(data);
  DCHECK_NE(0U, data_len);

  if (data_len > INT_MAX)
    return false;

  int bytes_to_write = static_cast<int>(data_len);
  int written_bytes = file_->WriteAtCurrentPos(
      reinterpret_cast<const char*>(data), bytes_to_write);
  if (written_bytes == -1)
    return false;

  return true;
}

bool MinidumpUpdater::WriteAndAdvance(const void* data,
                                      size_t data_len,
                                      FilePosition* position) {
  DCHECK(position);
  DCHECK_EQ(file_->Seek(base::File::FROM_CURRENT, 0), *position);

  if (!WriteData(data, data_len))
    return false;

  *position += base::saturated_cast<FilePosition>(data_len);
  return true;
}

// Writes a minidump file for |process| to |dump_file| with embedded
// CrashpadInfo, containing |crash_keys|, |client_id| and |report_id|.
// The |dump_file| must be open for read as well as write.
bool MiniDumpWriteDumpWithCrashpadInfo(const base::Process& process,
                                       uint32_t minidump_type,
                                       MINIDUMP_EXCEPTION_INFORMATION* exc_info,
                                       const StringStringMap& crash_keys,
                                       const crashpad::UUID& client_id,
                                       const crashpad::UUID& report_id,
                                       base::File* dump_file) {
  DCHECK(process.IsValid());
  DCHECK(dump_file && dump_file->IsValid());

  // The CrashpadInfo structure and its associated directory entry are injected
  // into the minidump, to minimize the work to patching up the dump.
  crashpad::MinidumpCrashpadInfo crashpad_info;
  crashpad_info.version = crashpad::MinidumpCrashpadInfo::kVersion;
  crashpad_info.client_id = client_id;
  crashpad_info.report_id = report_id;

  MINIDUMP_USER_STREAM crashpad_info_stream = {
      crashpad::kMinidumpStreamTypeCrashpadInfo,  // Type
      sizeof(crashpad_info),                      // BufferSize
      &crashpad_info                              // Buffer
  };
  MINIDUMP_USER_STREAM_INFORMATION user_stream_info = {
      1,                     // UserStreamCount
      &crashpad_info_stream  // UserStreamArray
  };

  // Write the minidump to the provided dump file.
  if (!MiniDumpWriteDump(
          process.Handle(),                           // Process handle.
          process.Pid(),                              // Process Id.
          dump_file->GetPlatformFile(),               // File handle.
          static_cast<MINIDUMP_TYPE>(minidump_type),  // Minidump type.
          exc_info,                                   // Exception Param
          &user_stream_info,                          // UserStreamParam,
          nullptr)) {                                 // CallbackParam
    return false;
  }

  // Retouch the minidump to make it Crashpad compatible.
  MinidumpUpdater updater;
  if (!updater.Initialize(dump_file))
    return false;
  if (!updater.AppendSimpleDictionary(crash_keys))
    return false;

  return true;
}

// Appends the full contents of |source| to |dest| from the current position
// of |dest|.
bool AppendFileContents(base::File* source, crashpad::FileWriter* dest) {
  DCHECK(source && source->IsValid());
  DCHECK(dest);

  // Rewind the source.
  if (source->Seek(base::File::FROM_BEGIN, 0) == -1)
    return false;

  std::vector<char> buf;
  buf.resize(1024);
  while (true) {
    int bytes_read =
        source->ReadAtCurrentPos(&buf[0], static_cast<int>(buf.size()));
    if (bytes_read < 0)
      return false;
    if (bytes_read == 0)
      break;

    if (!dest->Write(&buf[0], static_cast<size_t>(bytes_read))) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool DumpAndReportProcess(const base::Process& process,
                          uint32_t minidump_type,
                          MINIDUMP_EXCEPTION_INFORMATION* exc_info,
                          const StringStringMap& crash_keys,
                          const base::FilePath& crashpad_database_path) {
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(
          crashpad_database_path);

  if (!database)
    return false;

  std::unique_ptr<crashpad::CrashReportDatabase::NewReport> report;
  crashpad::CrashReportDatabase::OperationStatus status =
      database->PrepareNewCrashReport(&report);
  if (status != crashpad::CrashReportDatabase::kNoError)
    return false;

  crashpad::UUID client_id;
  crashpad::Settings* settings = database->GetSettings();
  if (settings) {
    // If GetSettings() or GetClientID() fails client_id will be left at its
    // default value, all zeroes, which is appropriate.
    settings->GetClientID(&client_id);
  }

  base::FilePath dump_file_path;
  if (!base::CreateTemporaryFile(&dump_file_path))
    return false;

  // Open the file with delete on close, to try and ensure it's cleaned up on
  // any kind of failure.
  base::File dump_file(dump_file_path, base::File::FLAG_OPEN |
                                           base::File::FLAG_READ |
                                           base::File::FLAG_WRITE |
                                           base::File::FLAG_DELETE_ON_CLOSE);
  if (!dump_file.IsValid())
    return false;

  // Write the minidump to the temp file, and then copy the data to the
  // Crashpad-provided handle, as the latter is only open for write.
  if (!MiniDumpWriteDumpWithCrashpadInfo(process, minidump_type, exc_info,
                                         crash_keys, client_id,
                                         report->ReportID(), &dump_file) ||
      !AppendFileContents(&dump_file, report->Writer())) {
    return false;
  }

  crashpad::UUID report_id = {};
  status = database->FinishedWritingCrashReport(std::move(report), &report_id);
  if (status != crashpad::CrashReportDatabase::kNoError)
    return false;

  return true;
}

}  // namespace crash_reporter
