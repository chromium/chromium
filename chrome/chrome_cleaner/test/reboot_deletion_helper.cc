// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/reboot_deletion_helper.h"

#include <windows.h>

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry_util.h"

namespace chrome_cleaner {

namespace {

// The moves-pending-reboot is a MULTISZ registry value in the HKLM part of the
// registry.
const wchar_t kSessionManagerKey[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager";
const wchar_t kPendingFileRenameOps[] = L"PendingFileRenameOperations";

const wchar_t kDoubleNullEntry[] = L"\0\0";

// Convert the strings found in |buffer| to a list of wstrings that is returned
// in |value|. |buffer| is a string which contains a series of pairs of
// null-terminated wstrings followed by a terminating null character. |value|
// is a pointer to an empty vector of pending moves. On success, this vector
// contains all of the strings extracted from |buffer|.
// Returns false if buffer does not meet the above specification.
bool MultiSZToStringArray(const std::wstring& buffer,
                          std::vector<PendingMove>* value) {
  DCHECK(value);
  DCHECK(value->empty());

  const wchar_t* data = buffer.c_str();
  const wchar_t* data_end = data + buffer.size();

  // Put null-terminated strings into the sequence.
  while (data < data_end) {
    // Parse the source path.
    std::wstring str_from(data);
    data += str_from.length() + 1;

    // Parse the destination path. If the offset is at the end of the string,
    // we assume the destination is empty. This may happen with corrupt registry
    // values where there are missing trailing null characters.
    std::wstring str_to;
    if (data > data_end)
      return false;
    if (data < data_end)
      str_to = data;

    // Move to the next entry.
    data += str_to.length() + 1;
    value->push_back(std::make_pair(str_from, str_to));
  }
  return true;
}

// The inverse of MultiSZToStringArray, this function converts a list
// of string pairs into a byte array format suitable for writing to the
// kPendingFileRenameOps registry value. It concatenates the strings and
// appends an additional terminating null character.
void StringArrayToMultiSZ(const std::vector<PendingMove>& pending_moves,
                          std::wstring* buffer) {
  DCHECK(buffer);
  buffer->clear();

  if (pending_moves.empty()) {
    // Leave buffer empty if there are no strings.
    return;
  }

  size_t total_wchars = 0;
  std::vector<PendingMove>::const_iterator iter(pending_moves.begin());
  for (; iter != pending_moves.end(); ++iter) {
    total_wchars += iter->first.length();
    ++total_wchars;  // Space for the null char.
    total_wchars += iter->second.length();
    ++total_wchars;  // Space for the null char.
  }
  ++total_wchars;  // Space for the extra terminating null char.

  buffer->resize(total_wchars);
  wchar_t* write_pointer = reinterpret_cast<wchar_t*>(&((*buffer)[0]));
  // Keep an end pointer around for sanity checking.
  wchar_t* end_pointer = write_pointer + total_wchars;

  std::vector<PendingMove>::const_iterator copy_iter(pending_moves.begin());
  for (; copy_iter != pending_moves.end() && write_pointer < end_pointer;
       ++copy_iter) {
    // First copy the source string.
    size_t string_length = copy_iter->first.length() + 1;
    ::memcpy(write_pointer, copy_iter->first.c_str(),
             string_length * sizeof(wchar_t));
    write_pointer += string_length;
    // Now copy the destination string.
    string_length = copy_iter->second.length() + 1;
    ::memcpy(write_pointer, copy_iter->second.c_str(),
             string_length * sizeof(wchar_t));
    write_pointer += string_length;

    // We should never run off the end while in this loop.
    DCHECK(write_pointer < end_pointer);
  }
  *write_pointer = L'\0';  // Explicitly set the final null char.
  DCHECK(++write_pointer == end_pointer);
}

// A helper function for the win32 GetShortPathName that more conveniently
// returns a FilePath. Note that if |path| is not present on the file system
// then GetShortPathName will return |path| unchanged, unlike the win32
// GetShortPathName which will return an error.
base::FilePath GetShortPathName(const base::FilePath& path) {
  std::wstring short_path;
  DWORD length = ::GetShortPathName(
      path.value().c_str(), base::WriteInto(&short_path, MAX_PATH), MAX_PATH);
  DPLOG_IF(WARNING, length == 0 && ::GetLastError() != ERROR_PATH_NOT_FOUND)
      << "GetShortPathName an unexpected result.";
  if (length == 0) {
    // GetShortPathName fails if the path is no longer present. Instead of
    // returning an empty string, just return the original string. This will
    // serve our purpose.
    return path;
  }

  short_path.resize(length);
  return base::FilePath(short_path);
}

base::FilePath GetShortPathNameWithoutPrefix(const base::FilePath& reg_path) {
  // Stores the path stored in each entry.
  std::wstring match_path(reg_path.value());

  // First chomp the prefix since that will mess up GetShortPathName.
  std::wstring prefix(L"\\??\\");
  if (base::StartsWith(match_path, prefix,
                       base::CompareCase::INSENSITIVE_ASCII))
    match_path = match_path.substr(4);

  // Get the short path name of the entry.
  return GetShortPathName(base::FilePath(match_path));
}

}  // namespace

bool IsFileRegisteredForPostRebootRemoval(const base::FilePath& file_path) {
  base::FilePath short_path = GetShortPathName(NormalizePath(file_path));

  PendingMoveVector pending_moves;
  // This can only be called in tests, so CHECKing is ok since that ensures the
  // test fails.
  CHECK(GetPendingMoves(&pending_moves)) << "Unable to get pending moves";

  for (PendingMoveVector::const_iterator iter(pending_moves.begin());
       iter != pending_moves.end(); ++iter) {
    if (!iter->second.empty()) {
      // If the destination string is not empty, the pending operation is a move
      // so this isn't a removal.
      continue;
    }

    base::FilePath pending_path(
        GetShortPathName(NormalizePath(base::FilePath(iter->first))));
    pending_path = GetShortPathNameWithoutPrefix(pending_path);
    if (base::FilePath::CompareEqualIgnoreCase(pending_path.value(),
                                               short_path.value())) {
      return true;
    }
  }

  return false;
}

bool UnregisterPostRebootRemovals(const FilePathSet& paths) {
  // Retrieve pending moves from the registry.
  PendingMoveVector pending_moves;
  if (!GetPendingMoves(&pending_moves))
    return false;

  // Build an index of short paths to remove.
  UnorderedFilePathSet short_paths;
  for (const auto& path : paths.file_paths()) {
    short_paths.insert(GetShortPathName(path));
  }

  // Filter paths pending for deletion with the short paths index.
  PendingMoveVector moves_to_keep;
  for (PendingMoveVector::const_iterator iter(pending_moves.begin());
       iter != pending_moves.end(); ++iter) {
    if (!iter->second.empty()) {
      // If the destination string is not empty, the pending operation is a move
      // and must not be removed.
      moves_to_keep.push_back(*iter);
      continue;
    }
    base::FilePath pending_path(
        GetShortPathName(NormalizePath(base::FilePath(iter->first))));
    pending_path = GetShortPathNameWithoutPrefix(pending_path);
    if (short_paths.find(pending_path) == short_paths.end())
      moves_to_keep.push_back(*iter);
  }

  // Update the remaining pending moves to the registry.
  if (!SetPendingMoves(moves_to_keep))
    return false;

  return true;
}

// Retrieves the list of pending moves from the registry and returns a vector
// containing pairs of strings that represent the operations. If the list
// contains only deletes then every other element will be an empty string
// as per http://msdn.microsoft.com/en-us/library/aa365240(VS.85).aspx.
bool GetPendingMoves(PendingMoveVector* pending_moves) {
  DCHECK(pending_moves);
  pending_moves->clear();

  // Get the current value of the key.
  base::win::RegKey session_manager_key(HKEY_LOCAL_MACHINE, kSessionManagerKey,
                                        KEY_QUERY_VALUE);
  HKEY session_manager_handle = session_manager_key.Handle();
  if (!session_manager_handle ||
      !session_manager_key.HasValue(kPendingFileRenameOps)) {
    // If the key or the value is missing, that's totally acceptable.
    return true;
  }

  // Read the content of the registry value to retrieve the pending moves.
  std::wstring pending_moves_value;
  uint32_t pending_moves_value_type;
  if (!ReadRegistryValue(session_manager_key, kPendingFileRenameOps,
                         &pending_moves_value, &pending_moves_value_type,
                         nullptr)) {
    DLOG(ERROR) << "Cannot read PendingRename registry value.";
    return false;
  }

  if (pending_moves_value_type != REG_MULTI_SZ) {
    DLOG(ERROR) << "Found PendingRename value of unexpected type.";
    return false;
  }

  // We now have a buffer of bytes that is actually a sequence of
  // null-terminated wide strings terminated by an additional null character.
  // Stick this into a vector of strings for clarity.
  if (!MultiSZToStringArray(pending_moves_value, pending_moves)) {
    DLOG(ERROR) << "Cannot decode PendingRename registry value.";
    return false;
  }

  // Remove the last pending moves entry if it is empty. This entry is found on
  // Vista+ but not on XP.
  if (!pending_moves->empty() && pending_moves->back().first.empty() &&
      pending_moves->back().second.empty()) {
    pending_moves->pop_back();
  }

  return true;
}

bool SetPendingMoves(const PendingMoveVector& pending_moves) {
  // Retrieve the key content into a buffer.
  base::win::RegKey session_manager_key(HKEY_LOCAL_MACHINE, kSessionManagerKey,
                                        KEY_CREATE_SUB_KEY | KEY_SET_VALUE);
  if (!session_manager_key.Handle()) {
    // Couldn't open / create the key.
    LOG(ERROR) << "Failed to open session manager key for writing.";
    return false;
  }

  if (pending_moves.empty()) {
    // No remaining moves. Don't bother writing that.
    LONG delete_result = session_manager_key.DeleteValue(kPendingFileRenameOps);
    return (delete_result == ERROR_SUCCESS ||
            delete_result == ERROR_FILE_NOT_FOUND);
  }

  // Serialize pending moves as a sequence of bytes.
  std::wstring buffer;
  StringArrayToMultiSZ(pending_moves, &buffer);
  if (buffer.empty())
    return false;

  // The pending moves format needs a null entry at the end which consists of
  // two MULTISZ empty string.
  std::wstring last_entry(kDoubleNullEntry, std::size(kDoubleNullEntry) - 1);
  buffer = buffer + last_entry;

  // Write back the serialized values into the registry key.
  uint32_t size_in_bytes = buffer.size() * sizeof(wchar_t);
  if (session_manager_key.WriteValue(kPendingFileRenameOps, buffer.c_str(),
                                     size_in_bytes,
                                     REG_MULTI_SZ) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to update the " << kPendingFileRenameOps << " key.";
    return false;
  }

  return true;
}

}  // namespace chrome_cleaner
