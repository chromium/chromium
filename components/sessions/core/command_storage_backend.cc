// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_backend.h"

#include <stdint.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/sessions/core/session_constants.h"
#include "crypto/aead.h"

namespace sessions {

using SessionType = CommandStorageManager::SessionType;

namespace {

// File version number.
constexpr int32_t kFileCurrentVersion = 1;
constexpr int32_t kEncryptedFileCurrentVersion = 2;

// The signature at the beginning of the file = SSNS (Sessions).
constexpr int32_t kFileSignature = 0x53534E53;

// Length (in bytes) of the nonce (used when encrypting).
constexpr int kNonceLength = 12;

// The file header is the first bytes written to the file,
// and is used to identify the file as one written by us.
struct FileHeader {
  int32_t signature;
  int32_t version;
};

// SessionFileReader ----------------------------------------------------------

// SessionFileReader is responsible for reading the set of SessionCommands that
// describe a Session back from a file. SessionFileRead does minimal error
// checking on the file (pretty much only that the header is valid).

class SessionFileReader {
 public:
  typedef sessions::SessionCommand::id_type id_type;
  typedef sessions::SessionCommand::size_type size_type;

  SessionFileReader(const base::FilePath& path,
                    const std::vector<uint8_t>& crypto_key)
      : buffer_(CommandStorageBackend::kFileReadBufferSize, 0),
        crypto_key_(crypto_key) {
    if (!crypto_key.empty()) {
      aead_ = std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM);
      aead_->Init(base::make_span(crypto_key_));
    }
    file_ = std::make_unique<base::File>(
        path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }

  // Returns true if the file has a valid header. A return value of false
  // most likely means the file was not written by this code. This function
  // is implicitly called by Read(), but may be called separately for checking
  // if the file is valid.
  bool HasValidHeader();

  // Reads the contents of the file specified in the constructor.
  std::vector<std::unique_ptr<sessions::SessionCommand>> Read();

 private:
  // Reads a single command, returning it. A return value of null indicates
  // either there are no commands, or there was an error. Use errored_ to
  // distinguish the two. If null is returned, and there is no error, it means
  // the end of file was successfully reached.
  std::unique_ptr<sessions::SessionCommand> ReadCommand();

  // Decrypts a previously encrypted command. Returns the new command on
  // success.
  std::unique_ptr<sessions::SessionCommand> CreateCommandFromEncrypted(
      const char* data,
      size_type length);

  // Creates a command from the previously written value.
  std::unique_ptr<sessions::SessionCommand> CreateCommand(const char* data,
                                                          size_type length);

  // Shifts the unused portion of buffer_ to the beginning and fills the
  // remaining portion with data from the file. Returns false if the buffer
  // couldn't be filled. A return value of false only signals an error if
  // errored_ is set to true.
  bool FillBuffer();

  // Whether an error condition has been detected (
  bool errored_ = false;

  // As we read from the file, data goes here.
  std::string buffer_;

  const std::vector<uint8_t> crypto_key_;

  std::unique_ptr<crypto::Aead> aead_;

  // The file.
  std::unique_ptr<base::File> file_;

  // Position in buffer_ of the data.
  size_t buffer_position_ = 0;

  // Number of available bytes; relative to buffer_position_.
  size_t available_count_ = 0;

  // Count of the number of commands encountered.
  int command_counter_ = 0;

  bool did_check_header_ = false;

  DISALLOW_COPY_AND_ASSIGN(SessionFileReader);
};

bool SessionFileReader::HasValidHeader() {
  // This function advances |file| and should only be called once.
  DCHECK(!did_check_header_);
  did_check_header_ = true;

  if (!file_->IsValid())
    return false;
  FileHeader header;
  const int read_count =
      file_->ReadAtCurrentPos(reinterpret_cast<char*>(&header), sizeof(header));
  if (read_count != sizeof(header) || header.signature != kFileSignature)
    return false;
  const bool encrypt = aead_.get() != nullptr;
  return (encrypt && header.version == kEncryptedFileCurrentVersion) ||
         (!encrypt && header.version == kFileCurrentVersion);
}

std::vector<std::unique_ptr<sessions::SessionCommand>>
SessionFileReader::Read() {
  if (!HasValidHeader())
    return {};

  std::vector<std::unique_ptr<sessions::SessionCommand>> read_commands;
  for (std::unique_ptr<sessions::SessionCommand> command = ReadCommand();
       command && !errored_; command = ReadCommand())
    read_commands.push_back(std::move(command));
  if (errored_)
    return {};
  return read_commands;
}

std::unique_ptr<sessions::SessionCommand> SessionFileReader::ReadCommand() {
  // Make sure there is enough in the buffer for the size of the next command.
  if (available_count_ < sizeof(size_type)) {
    if (!FillBuffer())
      return nullptr;
    if (available_count_ < sizeof(size_type)) {
      VLOG(1) << "SessionFileReader::ReadCommand, file incomplete";
      // Still couldn't read a valid size for the command, assume write was
      // incomplete and return null.
      return nullptr;
    }
  }
  // Get the size of the command.
  size_type command_size;
  memcpy(&command_size, &(buffer_[buffer_position_]), sizeof(command_size));
  buffer_position_ += sizeof(command_size);
  available_count_ -= sizeof(command_size);

  if (command_size == 0) {
    VLOG(1) << "SessionFileReader::ReadCommand, empty command";
    // Empty command. Shouldn't happen if write was successful, fail.
    return nullptr;
  }

  // Make sure buffer has the complete contents of the command.
  if (command_size > available_count_) {
    if (command_size > buffer_.size())
      buffer_.resize((command_size / 1024 + 1) * 1024, 0);
    if (!FillBuffer() || command_size > available_count_) {
      // Again, assume the file was ok, and just the last chunk was lost.
      VLOG(1) << "SessionFileReader::ReadCommand, last chunk lost";
      return nullptr;
    }
  }
  std::unique_ptr<SessionCommand> command;
  if (aead_) {
    command = CreateCommandFromEncrypted(buffer_.c_str() + buffer_position_,
                                         command_size);
  } else {
    command = CreateCommand(buffer_.c_str() + buffer_position_, command_size);
  }
  ++command_counter_;
  buffer_position_ += command_size;
  available_count_ -= command_size;
  return command;
}

std::unique_ptr<sessions::SessionCommand>
SessionFileReader::CreateCommandFromEncrypted(const char* data,
                                              size_type length) {
  // This means the nonce overflowed and we're reusing a nonce.
  // CommandStorageBackend should never write enough commands to trigger this,
  // so assume we should stop.
  if (command_counter_ < 0)
    return nullptr;

  char nonce[kNonceLength];
  memset(nonce, 0, kNonceLength);
  memcpy(nonce, &command_counter_, sizeof(command_counter_));
  std::string plain_text;
  if (!aead_->Open(base::StringPiece(data, length),
                   base::StringPiece(nonce, kNonceLength), base::StringPiece(),
                   &plain_text)) {
    DVLOG(1) << "SessionFileReader::ReadCommand, decryption failed";
    return nullptr;
  }
  if (plain_text.size() < sizeof(id_type)) {
    DVLOG(1) << "SessionFileReader::ReadCommand, size too small";
    return nullptr;
  }
  return CreateCommand(plain_text.c_str(), plain_text.size());
}

std::unique_ptr<sessions::SessionCommand> SessionFileReader::CreateCommand(
    const char* data,
    size_type length) {
  // Callers should have checked the size.
  DCHECK_GE(length, sizeof(id_type));
  const id_type command_id = data[0];
  // NOTE: |length| includes the size of the id, which is not part of the
  // contents of the SessionCommand.
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(command_id,
                                                 length - sizeof(id_type));
  if (length > sizeof(id_type)) {
    memcpy(command->contents(), &(data[sizeof(id_type)]),
           length - sizeof(id_type));
  }
  return command;
}

bool SessionFileReader::FillBuffer() {
  if (available_count_ > 0 && buffer_position_ > 0) {
    // Shift buffer to beginning.
    memmove(&(buffer_[0]), &(buffer_[buffer_position_]), available_count_);
  }
  buffer_position_ = 0;
  DCHECK(buffer_position_ + available_count_ < buffer_.size());
  int to_read = static_cast<int>(buffer_.size() - available_count_);
  int read_count =
      file_->ReadAtCurrentPos(&(buffer_[available_count_]), to_read);
  if (read_count < 0) {
    errored_ = true;
    return false;
  }
  if (read_count == 0)
    return false;
  available_count_ += read_count;
  return true;
}

base::FilePath::StringType TimestampToString(const base::Time time) {
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  return base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
#elif defined(OS_WIN)
  return base::NumberToString16(
      time.ToDeltaSinceWindowsEpoch().InMicroseconds());
#endif
}

// Returns the directory the files are stored in.
base::FilePath GetSessionDirName(CommandStorageManager::SessionType type,
                                 const base::FilePath& supplied_path) {
  if (type == CommandStorageManager::kOther)
    return supplied_path.DirName();
  return supplied_path.Append(kSessionsDirectory);
}

base::FilePath::StringType GetSessionBaseName(
    CommandStorageManager::SessionType type,
    const base::FilePath& supplied_path) {
  switch (type) {
    case CommandStorageManager::kTabRestore:
      return kTabSessionFileNamePrefix;
    case CommandStorageManager::kSessionRestore:
      return kSessionFileNamePrefix;
    case CommandStorageManager::kOther:
      return supplied_path.BaseName().value();
  }
}

base::FilePath::StringType GetSessionFilename(
    CommandStorageManager::SessionType type,
    const base::FilePath& supplied_path,
    const base::FilePath::StringType& timestamp_str) {
  return base::JoinString(
      {GetSessionBaseName(type, supplied_path), timestamp_str},
      kTimestampSeparator);
}

base::FilePath GetLegacySessionPath(CommandStorageManager::SessionType type,
                                    const base::FilePath& base_path,
                                    bool current) {
  switch (type) {
    case CommandStorageManager::kTabRestore:
      return base_path.Append(current ? kLegacyCurrentTabSessionFileName
                                      : kLegacyLastTabSessionFileName);
    case CommandStorageManager::kSessionRestore:
      return base_path.Append(current ? kLegacyCurrentSessionFileName
                                      : kLegacyLastSessionFileName);
    case CommandStorageManager::kOther:
      return base_path;
  }
}

}  // namespace

// CommandStorageBackend
// -------------------------------------------------------------

// static
const int CommandStorageBackend::kFileReadBufferSize = 1024;

// static
const SessionCommand::size_type
    CommandStorageBackend::kEncryptionOverheadInBytes = 16;

CommandStorageBackend::CommandStorageBackend(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
    const base::FilePath& path,
    SessionType type)
    : RefCountedDeleteOnSequence(owning_task_runner),
      type_(type),
      supplied_path_(path),
      timestamp_(base::Time::Now()) {
  // This is invoked on the main thread, don't do file access here.
  current_path_ = FilePathFromTime(type, path, timestamp_);
}

// static
bool CommandStorageBackend::IsValidFile(const base::FilePath& path) {
  SessionFileReader file_reader(path, {});
  return file_reader.HasValidHeader();
}

void CommandStorageBackend::AppendCommands(
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands,
    bool truncate,
    const std::vector<uint8_t>& crypto_key) {
  InitIfNecessary();

  if (truncate) {
    const bool was_encrypted = IsEncrypted();
    const bool encrypt = !crypto_key.empty();
    if (was_encrypted != encrypt) {
      // The header is different when encrypting, so the file needs to be
      // recreated.
      CloseFile();
    }
    if (encrypt) {
      aead_ = std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM);
      crypto_key_ = crypto_key;
      aead_->Init(base::make_span(crypto_key_));
    } else {
      aead_.reset();
    }
  } else {
    // |crypto_key| is only used when |truncate| is true.
    DCHECK(crypto_key.empty());
  }

  // Make sure and check |file_|, if opening the file failed |file_| will be
  // null.
  if (truncate || !file_ || !file_->IsValid())
    TruncateFile();

  // Check |file_| again as TruncateFile() may fail.
  if (file_ && file_->IsValid() &&
      !AppendCommandsToFile(file_.get(), commands)) {
    file_.reset();
  }
}

// static
bool CommandStorageBackend::TimestampFromPath(const base::FilePath& path,
                                              base::Time& timestamp_result) {
  auto parts =
      base::SplitString(path.BaseName().value(), kTimestampSeparator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 2u)
    return false;

  int64_t result = 0u;
  if (!base::StringToInt64(parts[1], &result))
    return false;

  timestamp_result = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(result));
  return true;
}

// static
std::set<base::FilePath> CommandStorageBackend::GetSessionFilePaths(
    const base::FilePath& path,
    CommandStorageManager::SessionType type) {
  std::set<base::FilePath> result;
  for (const auto& info : GetSessionFiles(path, type))
    result.insert(info.path);
  return result;
}

std::vector<std::unique_ptr<SessionCommand>>
CommandStorageBackend::ReadLastSessionCommands(
    const std::vector<uint8_t>& crypto_key) {
  InitIfNecessary();

  if (last_session_info_)
    return ReadCommandsFromFile(last_session_info_->path, crypto_key);
  return {};
}

void CommandStorageBackend::DeleteLastSession() {
  InitIfNecessary();
  if (last_session_info_) {
    base::DeleteFile(last_session_info_->path);
    last_session_info_.reset();
  }
}

void CommandStorageBackend::MoveCurrentSessionToLastSession() {
  // TODO(sky): make this work for kOther.
  DCHECK_NE(CommandStorageManager::SessionType::kOther, type_);

  InitIfNecessary();
  CloseFile();
  DeleteLastSession();

  // Move current session to last.
  if (base::PathExists(current_path_))
    last_session_info_ = SessionInfo{current_path_, timestamp_};
  else
    last_session_info_.reset();

  // Create new file, ensuring the timestamp is after the previous.
  // Due to clock changes, there might be an existing session with a later
  // time. This is especially true on Windows, which uses the local time as
  // the system clock.
  base::Time new_timestamp = base::Time::Now();
  if (!last_session_info_ || last_session_info_->timestamp < new_timestamp) {
    timestamp_ = new_timestamp;
  } else {
    timestamp_ =
        last_session_info_->timestamp + base::TimeDelta::FromMicroseconds(1);
  }
  SetPath(FilePathFromTime(type_, supplied_path_, timestamp_));

  // Create and open the file for the current session.
  DCHECK(!base::PathExists(current_path_));
  TruncateFile();
}

bool CommandStorageBackend::AppendCommandsToFile(
    base::File* file,
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& commands) {
  for (auto& command : commands) {
    if (IsEncrypted()) {
      if (!AppendEncryptedCommandToFile(file, *(command.get())))
        return false;
    } else if (!AppendCommandToFile(file, *(command.get()))) {
      return false;
    }
    commands_written_++;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  file->Flush();
#endif
  return true;
}

CommandStorageBackend::~CommandStorageBackend() = default;

void CommandStorageBackend::InitIfNecessary() {
  if (inited_)
    return;

  inited_ = true;
  base::CreateDirectory(current_path_.DirName());

  DetermineLastSessionFile();
  if (last_session_info_) {
    // Check that the last session's timestamp is before the current file's.
    // This might not be true if the system clock has changed.
    if (last_session_info_->timestamp > timestamp_) {
      timestamp_ =
          last_session_info_->timestamp + base::TimeDelta::FromMicroseconds(1);
      SetPath(FilePathFromTime(type_, supplied_path_, timestamp_));
    }
  }

  // Best effort delete all sessions except the current & last.
  DeleteLastSessionFiles();

  // Create and open the file for the current session.
  DCHECK(!base::PathExists(current_path_));
  TruncateFile();
}

// static
base::FilePath CommandStorageBackend::FilePathFromTime(
    const SessionType type,
    const base::FilePath& path,
    base::Time time) {
  return GetSessionDirName(type, path)
      .Append(GetSessionFilename(type, path, TimestampToString(time)));
}

void CommandStorageBackend::SetPath(const base::FilePath& path) {
  // Do not change the path if the file is open
  DCHECK(!file_);
  current_path_ = path;
}

// static
std::vector<std::unique_ptr<sessions::SessionCommand>>
CommandStorageBackend::ReadCommandsFromFile(
    const base::FilePath& path,
    const std::vector<uint8_t>& crypto_key) {
  SessionFileReader file_reader(path, crypto_key);
  return file_reader.Read();
}

void CommandStorageBackend::CloseFile() {
  file_.reset();
}

void CommandStorageBackend::TruncateFile() {
  DCHECK(inited_);
  if (file_) {
    // File is already open, truncate it. We truncate instead of closing and
    // reopening to avoid the possibility of scanners locking the file out
    // from under us once we close it. If truncation fails, we'll try to
    // recreate.
    const int header_size = static_cast<int>(sizeof(FileHeader));
    if (file_->Seek(base::File::FROM_BEGIN, header_size) != header_size ||
        !file_->SetLength(header_size))
      file_.reset();
  }
  if (!file_)
    file_ = OpenAndWriteHeader(current_path_);
  commands_written_ = 0;
}

std::unique_ptr<base::File> CommandStorageBackend::OpenAndWriteHeader(
    const base::FilePath& path) {
  DCHECK(!path.empty());
  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_EXCLUSIVE_WRITE |
                base::File::FLAG_EXCLUSIVE_READ);
  if (!file->IsValid())
    return nullptr;
  FileHeader header;
  header.signature = kFileSignature;
  header.version =
      IsEncrypted() ? kEncryptedFileCurrentVersion : kFileCurrentVersion;
  if (file->WriteAtCurrentPos(reinterpret_cast<char*>(&header),
                              sizeof(header)) != sizeof(header)) {
    return nullptr;
  }
  commands_written_ = 0;
  return file;
}

bool CommandStorageBackend::AppendCommandToFile(
    base::File* file,
    const sessions::SessionCommand& command) {
  const size_type total_size = command.GetSerializedSize();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&total_size),
                              sizeof(total_size)) != sizeof(total_size)) {
    DVLOG(1) << "error writing";
    return false;
  }
  id_type command_id = command.id();
  if (file->WriteAtCurrentPos(reinterpret_cast<char*>(&command_id),
                              sizeof(command_id)) != sizeof(command_id)) {
    DVLOG(1) << "error writing";
    return false;
  }

  const size_type content_size = total_size - sizeof(id_type);
  if (content_size == 0)
    return true;

  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(command.contents()),
                              content_size) != content_size) {
    DVLOG(1) << "error writing";
    return false;
  }
  return true;
}

bool CommandStorageBackend::AppendEncryptedCommandToFile(
    base::File* file,
    const sessions::SessionCommand& command) {
  // This means the nonce overflowed and we're reusing a nonce. This class
  // should never write enough commands to trigger this, so assume we should
  // stop.
  if (commands_written_ < 0)
    return false;
  DCHECK(IsEncrypted());
  char nonce[kNonceLength];
  memset(nonce, 0, kNonceLength);
  memcpy(nonce, &commands_written_, sizeof(commands_written_));

  // Encryption adds overhead, resulting in a slight reduction in the available
  // space for each command. Chop any contents beyond the available size.
  const size_type command_size = std::min(
      command.size(),
      static_cast<size_type>(std::numeric_limits<size_type>::max() -
                             sizeof(id_type) - kEncryptionOverheadInBytes));
  std::vector<char> command_and_id(command_size + sizeof(id_type));
  const id_type command_id = command.id();
  memcpy(&command_and_id.front(), reinterpret_cast<const char*>(&command_id),
         sizeof(id_type));
  memcpy(&(command_and_id.front()) + sizeof(id_type), command.contents(),
         command_size);

  std::string cipher_text;
  aead_->Seal(base::StringPiece(&command_and_id.front(), command_and_id.size()),
              base::StringPiece(nonce, kNonceLength), base::StringPiece(),
              &cipher_text);
  DCHECK_LE(cipher_text.size(), std::numeric_limits<size_type>::max());
  const size_type command_and_id_size =
      static_cast<size_type>(cipher_text.size());

  int wrote = file->WriteAtCurrentPos(
      reinterpret_cast<const char*>(&command_and_id_size),
      sizeof(command_and_id_size));
  if (wrote != sizeof(command_and_id_size)) {
    DVLOG(1) << "error writing";
    return false;
  }
  wrote = file->WriteAtCurrentPos(cipher_text.c_str(), cipher_text.size());
  if (wrote != static_cast<int>(cipher_text.size())) {
    DVLOG(1) << "error writing";
    return false;
  }
  return true;
}

void CommandStorageBackend::DetermineLastSessionFile() {
  last_session_info_.reset();

  // Determine the session with the most recent timestamp that
  // does not match the current session path.
  for (const SessionInfo& session : GetSessionFiles()) {
    if (session.path != current_path_ &&
        (!last_session_info_ ||
         session.timestamp > last_session_info_->timestamp)) {
      last_session_info_ = session;
    }
  }

  // If no last session was found, use the legacy session if present.
  // The legacy session is considered to have a timestamp of 0, before any
  // new session.
  if (!last_session_info_) {
    base::FilePath legacy_session =
        GetLegacySessionPath(type_, supplied_path_, true);
    if (base::PathExists(legacy_session))
      last_session_info_ = SessionInfo{legacy_session, base::Time()};
  }
}

void CommandStorageBackend::DeleteLastSessionFiles() {
  // Delete session files whose paths do not match the current or last session
  // path.
  for (const SessionInfo& session : GetSessionFiles()) {
    if (session.path != current_path_ &&
        (!last_session_info_ || session.path != last_session_info_->path)) {
      base::DeleteFile(session.path);
    }
  }

  // Delete legacy session files, unless they are being used.
  const base::FilePath legacy_current_session_path =
      GetLegacySessionPath(type_, supplied_path_, true);
  if (last_session_info_ &&
      legacy_current_session_path != last_session_info_->path &&
      base::PathExists(legacy_current_session_path)) {
    base::DeleteFile(legacy_current_session_path);
  }

  // `kOther` does not differentiate between last and current.
  if (type_ != CommandStorageManager::kOther) {
    const base::FilePath legacy_last_session_path =
        GetLegacySessionPath(type_, supplied_path_, false);
    if (base::PathExists(legacy_last_session_path))
      base::DeleteFile(legacy_last_session_path);
  }
}

// static
std::vector<CommandStorageBackend::SessionInfo>
CommandStorageBackend::GetSessionFiles(
    const base::FilePath& path,
    CommandStorageManager::SessionType type) {
  std::vector<SessionInfo> sessions;
  base::FileEnumerator file_enum(
      GetSessionDirName(type, path), false, base::FileEnumerator::FILES,
      GetSessionFilename(type, path, FILE_PATH_LITERAL("*")));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::Time file_time;
    if (TimestampFromPath(name, file_time))
      sessions.push_back(SessionInfo{name, file_time});
  }
  return sessions;
}

}  // namespace sessions
