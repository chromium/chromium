// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/file_change.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/drive/drive.pb.h"

namespace drive {

FileChange::Change::Change(ChangeType change, FileType file_type)
    : change_(change), file_type_(file_type) {
}

FileChange::Change::Change(ChangeType change,
                           FileType file_type,
                           const std::string& team_drive_id)
    : change_(change), file_type_(file_type), team_drive_id_(team_drive_id) {}

std::string FileChange::Change::DebugString() const {
  const char* change_string = nullptr;
  switch (change()) {
    case CHANGE_TYPE_ADD_OR_UPDATE:
      change_string = "ADD_OR_UPDATE";
      break;
    case CHANGE_TYPE_DELETE:
      change_string = "DELETE";
      break;
  }
  const char* type_string = "NO_INFO";
  switch (file_type()) {
    case FileChange::FILE_TYPE_FILE:
      type_string = "FILE";
      break;
    case FileChange::FILE_TYPE_DIRECTORY:
      type_string = "DIRECTORY";
      break;
    case FILE_TYPE_NO_INFO:
      // Keeps it as "no_info".
      break;
  }
  return base::StringPrintf("%s:%s", change_string, type_string);
}

FileChange::ChangeList::ChangeList() = default;
FileChange::ChangeList::ChangeList(const ChangeList& other) = default;
FileChange::ChangeList::~ChangeList() = default;

void FileChange::ChangeList::Update(const Change& new_change) {
  if (list_.empty()) {
    list_.push_back(new_change);
    return;
  }

  Change& last = list_.back();
  if (last.IsFile() != new_change.IsFile()) {
    list_.push_back(new_change);
    return;
  }

  if (last.team_drive_id() != new_change.team_drive_id()) {
    list_.push_back(new_change);
    return;
  }

  if (last.change() == new_change.change())
    return;

  // ADD + DELETE on directory -> revert
  if (!last.IsFile() && last.IsAddOrUpdate() && new_change.IsDelete()) {
    list_.pop_back();
    return;
  }

  // DELETE + ADD/UPDATE -> ADD/UPDATE
  // ADD/UPDATE + DELETE -> DELETE
  last = new_change;
}

FileChange::ChangeList FileChange::ChangeList::PopAndGetNewList() const {
  ChangeList changes;
  changes.list_ = this->list_;
  changes.list_.pop_front();
  return changes;
}

std::string FileChange::ChangeList::DebugString() const {
  std::ostringstream ss;
  ss << "{ ";
  for (size_t i = 0; i < list_.size(); ++i)
    ss << list_[i].DebugString() << ", ";
  ss << "}";
  return ss.str();
}

FileChange::FileChange() = default;
FileChange::FileChange(const FileChange& other) = default;
FileChange::~FileChange() = default;

void FileChange::Update(const base::FilePath file_path,
                        const FileChange::Change& new_change) {
  map_[file_path].Update(new_change);
}

void FileChange::Update(const base::FilePath file_path,
                        const FileChange::ChangeList& new_change) {
  for (ChangeList::List::const_iterator it = new_change.list().begin();
       it != new_change.list().end();
       it++) {
    map_[file_path].Update(*it);
  }
}

void FileChange::Update(const base::FilePath file_path,
                        FileType file_type,
                        FileChange::ChangeType change) {
  Update(file_path, FileChange::Change(change, file_type));
}

void FileChange::Update(const base::FilePath file_path,
                        const ResourceEntry& entry,
                        FileChange::ChangeType change) {
  FileType type = FILE_TYPE_NO_INFO;
  std::string team_drive_id;
  if (entry.has_file_info()) {
    type =
        entry.file_info().is_directory() ? FILE_TYPE_DIRECTORY : FILE_TYPE_FILE;
    if (entry.file_info().is_team_drive_root()) {
      team_drive_id = entry.resource_id();
    }
  }
  Update(file_path, FileChange::Change(change, type, team_drive_id));
}

void FileChange::Apply(const FileChange& new_changed_files) {
  for (auto it = new_changed_files.map().begin();
       it != new_changed_files.map().end(); it++) {
    Update(it->first, it->second);
  }
}

size_t FileChange::CountDirectory(const base::FilePath& directory_path) const {
  size_t count = 0;
  for (auto it = map_.begin(); it != map_.end(); it++) {
    if (it->first.DirName() == directory_path)
      count++;
  }
  return count;
}

std::string FileChange::DebugString() const {
  std::ostringstream ss;
  ss << "{ ";
  for (auto it = map_.begin(); it != map_.end(); it++) {
    ss << it->first.value() << ": " << it->second.DebugString() << ", ";
  }
  ss << "}";
  return ss.str();
}

}  // namespace drive
