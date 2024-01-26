// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERIALIZATION_SERIALIZATION_UTILS_H_
#define COMPONENTS_METRICS_SERIALIZATION_SERIALIZATION_UTILS_H_

#include <memory>
#include <string>
#include <vector>

namespace metrics {

class MetricSample;

// Metrics helpers to serialize and deserialize metrics collected by
// ChromeOS.
namespace SerializationUtils {

// If there are more than 100,000 messages in the file, discard the remaining
// messages to avoid running out of memory.
extern const int kMaxMessagesPerRead;

// Deserializes a sample passed as a string and return a sample.
// The return value will either be a scoped_ptr to a Metric sample (if the
// deserialization was successful) or a nullptr scoped_ptr.
std::unique_ptr<MetricSample> ParseSample(const std::string& sample);

// Reads all samples from a file and truncates the file when done.
void ReadAndTruncateMetricsFromFile(
    const std::string& filename,
    std::vector<std::unique_ptr<MetricSample>>* metrics);

// Reads all samples from a file and deletes the file when done.
void ReadAndDeleteMetricsFromFile(
    const std::string& filename,
    std::vector<std::unique_ptr<MetricSample>>* metrics);

// Serializes a sample and write it to filename.
// The format for the message is:
//  message_size, serialized_message
// where
//  * message_size is the total length of the message (message_size +
//    serialized_message) on 4 bytes
//  * serialized_message is the serialized version of sample (using ToString)
//
//  NB: the file will never leave the device so message_size will be written
//  with the architecture's endianness.
bool WriteMetricToFile(const MetricSample& sample, const std::string& filename);

// Maximum length of a serialized message
static const size_t kMessageMaxLength = 1024;

}  // namespace SerializationUtils
}  // namespace metrics

#endif  // COMPONENTS_METRICS_SERIALIZATION_SERIALIZATION_UTILS_H_
