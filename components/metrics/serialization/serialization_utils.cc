// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/metrics/serialization/serialization_utils.h"

#include <errno.h>
#include <stdint.h>
#include <sys/file.h>
#include <unistd.h>

#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/metrics/serialization/metric_sample.h"

#define READ_WRITE_ALL_FILE_FLAGS \
  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

namespace metrics {
namespace {
// Reads the next message from |file_descriptor| into |message|.
//
// |message| will be set to the empty string if no message could be read (EOF)
// or the message was badly constructed.
//
// Returns false if no message can be read from this file anymore (EOF or
// unrecoverable error).
bool ReadMessage(int fd, std::string* message) {
  CHECK(message);

  int result;
  uint32_t encoded_size;
  const size_t message_header_size = sizeof(uint32_t);
  // The file containing the metrics does not leave the device so the writer and
  // the reader will always have the same endianness.
  result = HANDLE_EINTR(read(fd, &encoded_size, message_header_size));
  if (result < 0) {
    DPLOG(ERROR) << "reading metrics message header";
    return false;
  }
  if (result == 0) {
    // This indicates a normal EOF.
    return false;
  }
  if (base::checked_cast<size_t>(result) < message_header_size) {
    DLOG(ERROR) << "bad read size " << result << ", expecting "
                << message_header_size;
    return false;
  }

  // kMessageMaxLength applies to the entire message: the 4-byte
  // length field and the content.
  size_t message_size = base::checked_cast<size_t>(encoded_size);
  if (message_size > SerializationUtils::kMessageMaxLength) {
    DLOG(ERROR) << "message too long : " << message_size;
    if (HANDLE_EINTR(lseek(fd, message_size - message_header_size, SEEK_CUR)) ==
        -1) {
      DLOG(ERROR) << "error while skipping message. abort";
      return false;
    }
    // Badly formatted message was skipped. Treat the badly formatted sample as
    // an empty sample.
    message->clear();
    return true;
  }

  if (message_size < message_header_size) {
    DLOG(ERROR) << "message too short : " << message_size;
    return false;
  }

  message_size -= message_header_size;  // The message size includes itself.
  char buffer[SerializationUtils::kMessageMaxLength];
  if (!base::ReadFromFD(fd, base::make_span(buffer, message_size))) {
    DPLOG(ERROR) << "reading metrics message body";
    return false;
  }
  *message = std::string(buffer, message_size);
  return true;
}

// Reads all samples from a file and when done:
//  1) deletes the file if |delete_file| is true.
//  2) truncates the file if |delete_file| is false.
//
// This method is the implementation of ReadAndTruncateMetricsFromFile() and
// ReadAndDeleteMetricsFromFile().
void ReadAndTruncateOrDeleteMetricsFromFile(
    const std::string& filename,
    bool delete_file,
    std::vector<std::unique_ptr<MetricSample>>* metrics) {
  struct stat stat_buf;
  int result;

  result = stat(filename.c_str(), &stat_buf);
  if (result < 0) {
    if (errno == ENOENT) {
      // File doesn't exist, nothing to collect. This isn't an error, it just
      // means nothing on the ChromeOS side has written to the file yet.
    } else {
      DPLOG(ERROR) << "bad metrics file stat: " << filename;
    }
    return;
  }
  if (stat_buf.st_size == 0) {
    // Also nothing to collect.
    return;
  }
  // Only need to read/write if we're truncating.
  int flag = delete_file ? O_RDONLY : O_RDWR;
  base::ScopedFD fd(open(filename.c_str(), flag));
  if (fd.get() < 0) {
    DPLOG(ERROR) << "cannot open: " << filename;
    return;
  }
  result = flock(fd.get(), LOCK_EX);
  if (result < 0) {
    DPLOG(ERROR) << "cannot lock: " << filename;
    return;
  }

  // This processes all messages in the log. When all messages are
  // read and processed, or an error occurs, or we've read so many that the
  // buffer is at risk of overflowing, delete the file or truncate the file to
  // zero size according to |delete_file|. If we hit kMaxMessagesPerRead, don't
  // add them to the vector to avoid memory overflow.
  while (metrics->size() <
         static_cast<size_t>(SerializationUtils::kMaxMessagesPerRead)) {
    std::string message;

    if (!ReadMessage(fd.get(), &message)) {
      break;
    }

    std::unique_ptr<MetricSample> sample =
        SerializationUtils::ParseSample(message);
    if (sample) {
      metrics->push_back(std::move(sample));
    }
  }

  base::UmaHistogramCustomCounts(
      "Platform.ExternalMetrics.SamplesRead", metrics->size(), 1,
      SerializationUtils::kMaxMessagesPerRead - 1, 50);

  if (delete_file) {
    result = unlink(filename.c_str());
    if (result < 0) {
      DPLOG(ERROR) << "error deleting metrics log: " << filename;
    }
  } else {
    result = ftruncate(fd.get(), 0);
    if (result < 0) {
      DPLOG(ERROR) << "error truncating metrics log: " << filename;
    }
  }

  result = flock(fd.get(), LOCK_UN);
  if (result < 0) {
    DPLOG(ERROR) << "error unlocking metrics log: " << filename;
  }
}

}  // namespace

// This value is used as a max value in a histogram,
// Platform.ExternalMetrics.SamplesRead. If it changes, the histogram will need
// to be renamed.
const int SerializationUtils::kMaxMessagesPerRead = 100000;

std::unique_ptr<MetricSample> SerializationUtils::ParseSample(
    const std::string& sample) {
  if (sample.empty()) {
    return nullptr;
  }

  std::vector<std::string> parts =
      base::SplitString(sample, std::string(1, '\0'), base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  // We should have two null terminated strings so split should produce
  // three chunks.
  if (parts.size() != 3) {
    DLOG(ERROR) << "splitting message on \\0 produced " << parts.size()
                << " parts (expected 3)";
    return nullptr;
  }
  const std::string& name = parts[0];
  const std::string& value = parts[1];

  if (base::EqualsCaseInsensitiveASCII(name, "crash")) {
    return MetricSample::ParseCrash(value);
  }
  if (base::EqualsCaseInsensitiveASCII(name, "histogram")) {
    return MetricSample::ParseHistogram(value);
  }
  if (base::EqualsCaseInsensitiveASCII(name, "linearhistogram")) {
    return MetricSample::ParseLinearHistogram(value);
  }
  if (base::EqualsCaseInsensitiveASCII(name, "sparsehistogram")) {
    return MetricSample::ParseSparseHistogram(value);
  }
  if (base::EqualsCaseInsensitiveASCII(name, "useraction")) {
    return MetricSample::ParseUserAction(value);
  }
  DLOG(ERROR) << "invalid event type: " << name << ", value: " << value;
  return nullptr;
}

void SerializationUtils::ReadAndTruncateMetricsFromFile(
    const std::string& filename,
    std::vector<std::unique_ptr<MetricSample>>* metrics) {
  ReadAndTruncateOrDeleteMetricsFromFile(filename, /*delete_file=*/false,
                                         metrics);
}

void SerializationUtils::ReadAndDeleteMetricsFromFile(
    const std::string& filename,
    std::vector<std::unique_ptr<MetricSample>>* metrics) {
  ReadAndTruncateOrDeleteMetricsFromFile(filename, /*delete_file=*/true,
                                         metrics);
}

bool SerializationUtils::WriteMetricToFile(const MetricSample& sample,
                                           const std::string& filename) {
  if (!sample.IsValid()) {
    return false;
  }

  base::ScopedFD file_descriptor(open(filename.c_str(),
                                      O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC,
                                      READ_WRITE_ALL_FILE_FLAGS));

  if (file_descriptor.get() < 0) {
    DPLOG(ERROR) << "error opening the file: " << filename;
    return false;
  }

  fchmod(file_descriptor.get(), READ_WRITE_ALL_FILE_FLAGS);
  // Grab a lock to avoid chrome truncating the file underneath us. Keep the
  // file locked as briefly as possible. Freeing file_descriptor will close the
  // file and remove the lock IFF the process was not forked in the meantime,
  // which will leave the flock hanging and deadlock the reporting until the
  // forked process is killed otherwise. Thus we have to explicitly unlock the
  // file below.
  if (HANDLE_EINTR(flock(file_descriptor.get(), LOCK_EX)) < 0) {
    DPLOG(ERROR) << "error locking: " << filename;
    return false;
  }

  std::string msg = sample.ToString();
  size_t size = 0;
  if (!base::CheckAdd(msg.length(), sizeof(uint32_t)).AssignIfValid(&size) ||
      size > kMessageMaxLength) {
    DPLOG(ERROR) << "cannot write message: too long: " << filename;
    std::ignore = flock(file_descriptor.get(), LOCK_UN);
    return false;
  }

  // The file containing the metrics samples will only be read by programs on
  // the same device so we do not check endianness.
  uint32_t encoded_size = base::checked_cast<uint32_t>(size);
  if (!base::WriteFileDescriptor(file_descriptor.get(),
                                 base::byte_span_from_ref(encoded_size))) {
    DPLOG(ERROR) << "error writing message length: " << filename;
    std::ignore = flock(file_descriptor.get(), LOCK_UN);
    return false;
  }

  if (!base::WriteFileDescriptor(file_descriptor.get(), msg)) {
    DPLOG(ERROR) << "error writing message: " << filename;
    std::ignore = flock(file_descriptor.get(), LOCK_UN);
    return false;
  }

  return true;
}

}  // namespace metrics
