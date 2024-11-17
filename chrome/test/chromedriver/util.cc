// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/command_listener.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/key_converter.h"
#include "chrome/test/chromedriver/session.h"
#include "third_party/zlib/google/zip.h"

std::string GenerateId() {
  uint64_t msb = base::RandUint64();
  uint64_t lsb = base::RandUint64();
  return base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
}

namespace {
const double kCentimetersPerInch = 2.54;

Status FlattenStringArray(const base::Value::List* src, std::u16string* dest) {
  std::u16string keys;
  for (const base::Value& i : *src) {
    if (!i.is_string())
      return Status(kUnknownError, "keys should be a string");

    std::u16string keys_list_part = base::UTF8ToUTF16(i.GetString());

    for (char16_t ch : keys_list_part) {
      if (CBU16_IS_SURROGATE(ch)) {
        return Status(
            kUnknownError,
            base::StringPrintf("%s only supports characters in the BMP",
                              kChromeDriverProductShortName));
      }
    }

    keys.append(keys_list_part);
  }
  *dest = keys;
  return Status(kOk);
}

}  // namespace

Status SendKeysOnWindow(WebView* web_view,
                        const base::Value::List* key_list,
                        bool release_modifiers,
                        int* sticky_modifiers) {
  std::u16string keys;
  Status status = FlattenStringArray(key_list, &keys);
  if (status.IsError())
    return status;
  std::vector<KeyEvent> events;
  int sticky_modifiers_tmp = *sticky_modifiers;
  status = ConvertKeysToKeyEvents(
      keys, release_modifiers, &sticky_modifiers_tmp, &events);
  if (status.IsError())
    return status;
  status = web_view->DispatchKeyEvents(events, false);
  if (status.IsOk())
    *sticky_modifiers = sticky_modifiers_tmp;
  return status;
}

bool Base64Decode(const std::string& base64,
                  std::string* bytes) {
  std::string copy = base64;
  // Some WebDriver client base64 encoders follow RFC 1521, which require that
  // 'encoded lines be no more than 76 characters long'. Just remove any
  // newlines.
  base::RemoveChars(copy, "\n", &copy);
  return base::Base64Decode(copy, bytes);
}

namespace {

Status UnzipArchive(const base::FilePath& unzip_dir,
                    const std::string& bytes) {
  base::ScopedTempDir dir;
  if (!dir.CreateUniqueTempDir())
    return Status(kUnknownError, "unable to create temp dir");

  base::FilePath archive = dir.GetPath().AppendASCII("temp.zip");
  if (!base::WriteFile(archive, bytes)) {
    return Status(kUnknownError, "could not write file to temp dir");
  }

  if (!zip::Unzip(archive, unzip_dir))
    return Status(kUnknownError, "could not unzip archive");
  return Status(kOk);
}

// Stream for writing binary data.
class DataOutputStream {
 public:
  DataOutputStream() {}
  ~DataOutputStream() {}

  void WriteUInt16(uint16_t data) { WriteBytes(&data, sizeof(data)); }

  void WriteUInt32(uint32_t data) { WriteBytes(&data, sizeof(data)); }

  void WriteString(const std::string& data) {
    WriteBytes(data.c_str(), data.length());
  }

  void WriteBytes(const void* bytes, int size) {
    if (!size)
      return;
    size_t next = buffer_.length();
    buffer_.resize(next + size);
    memcpy(&buffer_[next], bytes, size);
  }

  const std::string& buffer() const { return buffer_; }

 private:
  std::string buffer_;
};

// Stream for reading binary data.
class DataInputStream {
 public:
  DataInputStream(const char* data, int size)
      : data_(data), size_(size), iter_(0) {}
  ~DataInputStream() {}

  bool ReadUInt16(uint16_t* data) { return ReadBytes(data, sizeof(*data)); }

  bool ReadUInt32(uint32_t* data) { return ReadBytes(data, sizeof(*data)); }

  bool ReadString(std::string* data, int length) {
    if (length < 0)
      return false;
    // Check here to make sure we don't allocate wastefully.
    if (iter_ + length > size_)
      return false;
    data->resize(length);
    if (length == 0)
      return true;
    return ReadBytes(&(*data)[0], length);
  }

  bool ReadBytes(void* bytes, int size) {
    if (iter_ + size > size_)
      return false;
    memcpy(bytes, &data_[iter_], size);
    iter_ += size;
    return true;
  }

  int remaining() const { return size_ - iter_; }

 private:
  const char* data_;
  int size_;
  int iter_;
};

// A file entry within a zip archive. This may be incomplete and is not
// guaranteed to be able to parse all types of zip entries.
// See http://www.pkware.com/documents/casestudies/APPNOTE.TXT for the zip
// file format.
struct ZipEntry {
  // The given bytes must contain the whole zip entry and only the entry,
  // although the entry may include a data descriptor.
  static bool FromBytes(const std::string& bytes, ZipEntry* zip,
                        std::string* error_msg) {
    DataInputStream stream(bytes.c_str(), bytes.length());

    uint32_t signature;
    if (!stream.ReadUInt32(&signature) || signature != kFileHeaderSignature) {
      *error_msg = "invalid file header signature";
      return false;
    }
    if (!stream.ReadUInt16(&zip->version_needed)) {
      *error_msg = "invalid version";
      return false;
    }
    if (!stream.ReadUInt16(&zip->bit_flag)) {
      *error_msg = "invalid bit flag";
      return false;
    }
    if (!stream.ReadUInt16(&zip->compression_method)) {
      *error_msg = "invalid compression method";
      return false;
    }
    if (!stream.ReadUInt16(&zip->mod_time)) {
      *error_msg = "invalid file last modified time";
      return false;
    }
    if (!stream.ReadUInt16(&zip->mod_date)) {
      *error_msg = "invalid file last modified date";
      return false;
    }
    if (!stream.ReadUInt32(&zip->crc)) {
      *error_msg = "invalid crc";
      return false;
    }
    uint32_t compressed_size;
    if (!stream.ReadUInt32(&compressed_size)) {
      *error_msg = "invalid compressed size";
      return false;
    }
    if (!stream.ReadUInt32(&zip->uncompressed_size)) {
      *error_msg = "invalid compressed size";
      return false;
    }
    uint16_t name_length;
    if (!stream.ReadUInt16(&name_length)) {
      *error_msg = "invalid name length";
      return false;
    }
    uint16_t field_length;
    if (!stream.ReadUInt16(&field_length)) {
      *error_msg = "invalid field length";
      return false;
    }
    if (!stream.ReadString(&zip->name, name_length)) {
      *error_msg = "invalid name";
      return false;
    }
    if (!stream.ReadString(&zip->fields, field_length)) {
      *error_msg = "invalid fields";
      return false;
    }
    if (zip->bit_flag & 0x8) {
      // Has compressed data and a separate data descriptor.
      if (stream.remaining() < 16) {
        *error_msg = "too small for data descriptor";
        return false;
      }
      compressed_size = stream.remaining() - 16;
      if (!stream.ReadString(&zip->compressed_data, compressed_size)) {
        *error_msg = "invalid compressed data before descriptor";
        return false;
      }
      if (!stream.ReadUInt32(&signature) ||
          signature != kDataDescriptorSignature) {
        *error_msg = "invalid data descriptor signature";
        return false;
      }
      if (!stream.ReadUInt32(&zip->crc)) {
        *error_msg = "invalid crc";
        return false;
      }
      if (!stream.ReadUInt32(&compressed_size)) {
        *error_msg = "invalid compressed size";
        return false;
      }
      if (compressed_size != zip->compressed_data.length()) {
        *error_msg = "compressed data does not match data descriptor";
        return false;
      }
      if (!stream.ReadUInt32(&zip->uncompressed_size)) {
        *error_msg = "invalid compressed size";
        return false;
      }
    } else {
      // Just has compressed data.
      if (!stream.ReadString(&zip->compressed_data, compressed_size)) {
        *error_msg = "invalid compressed data";
        return false;
      }
      if (stream.remaining() != 0) {
        *error_msg = "leftover data after zip entry";
        return false;
      }
    }
    return true;
  }

  // Returns bytes for a valid zip file that just contains this zip entry.
  std::string ToZip() {
    // Write zip entry with no data descriptor.
    DataOutputStream stream;
    stream.WriteUInt32(kFileHeaderSignature);
    stream.WriteUInt16(version_needed);
    stream.WriteUInt16(bit_flag);
    stream.WriteUInt16(compression_method);
    stream.WriteUInt16(mod_time);
    stream.WriteUInt16(mod_date);
    stream.WriteUInt32(crc);
    stream.WriteUInt32(compressed_data.length());
    stream.WriteUInt32(uncompressed_size);
    stream.WriteUInt16(name.length());
    stream.WriteUInt16(fields.length());
    stream.WriteString(name);
    stream.WriteString(fields);
    stream.WriteString(compressed_data);
    uint32_t entry_size = stream.buffer().length();

    // Write central directory.
    stream.WriteUInt32(kCentralDirSignature);
    stream.WriteUInt16(0x14);  // Version made by. Unused at version 0.
    stream.WriteUInt16(version_needed);
    stream.WriteUInt16(bit_flag);
    stream.WriteUInt16(compression_method);
    stream.WriteUInt16(mod_time);
    stream.WriteUInt16(mod_date);
    stream.WriteUInt32(crc);
    stream.WriteUInt32(compressed_data.length());
    stream.WriteUInt32(uncompressed_size);
    stream.WriteUInt16(name.length());
    stream.WriteUInt16(fields.length());
    stream.WriteUInt16(0);  // Comment length.
    stream.WriteUInt16(0);  // Disk number where file starts.
    stream.WriteUInt16(0);  // Internal file attr.
    stream.WriteUInt32(0);  // External file attr.
    stream.WriteUInt32(0);  // Offset to file.
    stream.WriteString(name);
    stream.WriteString(fields);
    uint32_t cd_size = stream.buffer().length() - entry_size;

    // End of central directory.
    stream.WriteUInt32(kEndOfCentralDirSignature);
    stream.WriteUInt16(0);  // num of this disk
    stream.WriteUInt16(0);  // disk where cd starts
    stream.WriteUInt16(1);  // number of cds on this disk
    stream.WriteUInt16(1);  // total cds
    stream.WriteUInt32(cd_size);  // size of cd
    stream.WriteUInt32(entry_size);  // offset of cd
    stream.WriteUInt16(0);  // comment len

    return stream.buffer();
  }

  static const uint32_t kFileHeaderSignature;
  static const uint32_t kDataDescriptorSignature;
  static const uint32_t kCentralDirSignature;
  static const uint32_t kEndOfCentralDirSignature;
  uint16_t version_needed;
  uint16_t bit_flag;
  uint16_t compression_method;
  uint16_t mod_time;
  uint16_t mod_date;
  uint32_t crc;
  uint32_t uncompressed_size;
  std::string name;
  std::string fields;
  std::string compressed_data;
};

const uint32_t ZipEntry::kFileHeaderSignature = 0x04034b50;
const uint32_t ZipEntry::kDataDescriptorSignature = 0x08074b50;
const uint32_t ZipEntry::kCentralDirSignature = 0x02014b50;
const uint32_t ZipEntry::kEndOfCentralDirSignature = 0x06054b50;

Status UnzipEntry(const base::FilePath& unzip_dir,
                  const std::string& bytes) {
  ZipEntry entry;
  std::string zip_error_msg;
  if (!ZipEntry::FromBytes(bytes, &entry, &zip_error_msg))
    return Status(kUnknownError, zip_error_msg);
  std::string archive = entry.ToZip();
  return UnzipArchive(unzip_dir, archive);
}

}  // namespace

Status UnzipSoleFile(const base::FilePath& unzip_dir,
                     const std::string& bytes,
                     base::FilePath* file) {
  std::string archive_error, entry_error;
  Status status = UnzipArchive(unzip_dir, bytes);
  if (status.IsError()) {
    Status entry_status = UnzipEntry(unzip_dir, bytes);
    if (entry_status.IsError()) {
      return Status(kUnknownError, base::StringPrintf(
          "archive error: (%s), entry error: (%s)",
          status.message().c_str(), entry_status.message().c_str()));
    }
  }

  base::FileEnumerator enumerator(unzip_dir, false /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  base::FilePath first_file = enumerator.Next();
  if (first_file.empty())
    return Status(kUnknownError, "contained 0 files");

  base::FilePath second_file = enumerator.Next();
  if (!second_file.empty())
    return Status(kUnknownError, "contained multiple files");

  *file = first_file;
  return Status(kOk);
}

Status NotifyCommandListenersBeforeCommand(Session* session,
                                           const std::string& command_name) {
  for (const auto& listener : session->command_listeners) {
    Status status = listener->BeforeCommand(command_name);
    if (status.IsError()) {
      // Do not continue if an error is encountered. Mark session for deletion,
      // quit Chrome if necessary, and return a detailed error.
      if (!session->quit) {
        session->quit = true;
        std::string message = base::StringPrintf("session deleted because "
            "error encountered when notifying listeners of '%s' command",
            command_name.c_str());
        if (session->chrome && !session->detach) {
          Status quit_status = session->chrome->Quit();
          if (quit_status.IsError())
            message += ", but failed to kill browser:" + quit_status.message();
        }
        status = Status(kUnknownError, message, status);
      }
      if (session->chrome) {
        const BrowserInfo* browser_info = session->chrome->GetBrowserInfo();
        status.AddDetails("Session info: " + browser_info->browser_name + "=" +
                          browser_info->browser_version);
      }
      return status;
    }
  }
  return Status(kOk);
}

double ConvertCentimeterToInch(double centimeter) {
  return centimeter / kCentimetersPerInch;
}

namespace {

template <typename T>
bool GetOptionalValue(const base::Value::Dict& dict,
                      std::string_view path,
                      T* out_value,
                      bool* has_value,
                      std::optional<T> (base::Value::*getter)() const) {
  if (has_value != nullptr)
    *has_value = false;

  const base::Value* value = dict.FindByDottedPath(path);
  if (!value)
    return true;
  std::optional<T> maybe_value = (value->*getter)();
  if (maybe_value.has_value()) {
    *out_value = maybe_value.value();
    if (has_value != nullptr)
      *has_value = true;
    return true;
  }
  return false;
}

}  // namespace

bool GetOptionalBool(const base::Value::Dict& dict,
                     std::string_view path,
                     bool* out_value,
                     bool* has_value) {
  return GetOptionalValue(dict, path, out_value, has_value,
                          &base::Value::GetIfBool);
}

bool GetOptionalInt(const base::Value::Dict& dict,
                    std::string_view path,
                    int* out_value,
                    bool* has_value) {
  if (GetOptionalValue(dict, path, out_value, has_value,
                       &base::Value::GetIfInt)) {
    return true;
  }
  // See if we have a double that contains an int value.
  std::optional<double> maybe_decimal = dict.FindDoubleByDottedPath(path);
  if (!maybe_decimal.has_value() ||
      !base::IsValueInRangeForNumericType<int>(maybe_decimal.value())) {
    return false;
  }

  int i = static_cast<int>(maybe_decimal.value());
  if (i == maybe_decimal.value()) {
    *out_value = i;
    if (has_value != nullptr)
      *has_value = true;
    return true;
  }
  return false;
}

bool GetOptionalDouble(const base::Value::Dict& dict,
                       std::string_view path,
                       double* out_value,
                       bool* has_value) {
  return GetOptionalValue(dict, path, out_value, has_value,
                          &base::Value::GetIfDouble);
}

bool GetOptionalString(const base::Value::Dict& dict,
                       std::string_view path,
                       std::string* out_value,
                       bool* has_value) {
  if (has_value != nullptr)
    *has_value = false;

  const base::Value* value = dict.FindByDottedPath(path);
  if (!value)
    return true;

  if (value->is_string()) {
    *out_value = value->GetString();
    if (has_value != nullptr)
      *has_value = true;
    return true;
  }
  return false;
}

bool GetOptionalDictionary(const base::Value::Dict& dict,
                           std::string_view path,
                           const base::Value::Dict** out_value,
                           bool* has_value) {
  if (has_value != nullptr)
    *has_value = false;
  const base::Value* value = dict.FindByDottedPath(path);
  if (value == nullptr)
    return true;
  if (value->is_dict()) {
    *out_value = value->GetIfDict();
    if (has_value != nullptr)
      *has_value = true;
    return true;
  }
  return false;
}

bool GetOptionalList(const base::Value::Dict& dict,
                     std::string_view path,
                     const base::Value::List** out_value,
                     bool* has_value) {
  if (has_value != nullptr)
    *has_value = false;

  const base::Value* value = dict.FindByDottedPath(path);
  if (!value)
    return true;

  if (value->is_list()) {
    *out_value = &value->GetList();
    if (has_value != nullptr)
      *has_value = true;
    return true;
  }

  return false;
}

bool GetOptionalSafeInt(const base::Value::Dict& dict,
                        std::string_view path,
                        int64_t* out_value,
                        bool* has_value) {
  // Check if we have a normal int, which is always a safe int.
  int temp_int;
  bool temp_has_value;
  if (GetOptionalValue(dict, path, &temp_int, &temp_has_value,
                       &base::Value::GetIfInt)) {
    if (has_value != nullptr)
      *has_value = temp_has_value;
    if (temp_has_value)
      *out_value = temp_int;
    return true;
  }

  // Check if we have a double, which may or may not contain a safe int value.
  std::optional<double> maybe_decimal = dict.FindDoubleByDottedPath(path);
  if (!maybe_decimal.has_value())
    return false;

  // Verify that the value is an integer.
  int64_t temp_int64 = static_cast<int64_t>(maybe_decimal.value());
  if (temp_int64 != maybe_decimal.value())
    return false;

  // Verify that the value is in the range for safe integer.
  if (temp_int64 >= (1ll << 53) || temp_int64 <= -(1ll << 53))
    return false;

  // Got a good value.
  *out_value = temp_int64;
  if (has_value != nullptr)
    *has_value = true;
  return true;
}

bool SetSafeInt(base::Value::Dict& dict,
                std::string_view path,
                int64_t in_value_64) {
  int int_value = static_cast<int>(in_value_64);
  if (in_value_64 == int_value)
    return dict.SetByDottedPath(path, int_value);
  else
    return dict.SetByDottedPath(path, static_cast<double>(in_value_64));
}
