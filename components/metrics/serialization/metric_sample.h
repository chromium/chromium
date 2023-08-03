// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERIALIZATION_METRIC_SAMPLE_H_
#define COMPONENTS_METRICS_SERIALIZATION_METRIC_SAMPLE_H_

#include <memory>
#include <string>

namespace metrics {

// This class is used by libmetrics (ChromeOS) to serialize
// and deserialize measurements to send them to a metrics sending service.
// It is meant to be a simple container with serialization functions.
class MetricSample {
 public:
  // Types of metric sample used.
  enum SampleType {
    CRASH,
    HISTOGRAM,
    LINEAR_HISTOGRAM,
    SPARSE_HISTOGRAM,
    USER_ACTION
  };

  // Use one of the static methods in this class instead of calling the
  // constructor directly.
  //
  // The constructor is exposed for std::make_unique.
  MetricSample(SampleType sample_type,
               const std::string& metric_name,
               const int sample,
               const int min,
               const int max,
               const int bucket_count,
               const int num_samples);

  MetricSample(const MetricSample&) = delete;
  MetricSample& operator=(const MetricSample&) = delete;

  ~MetricSample();

  // Returns true if the sample is valid (can be serialized without ambiguity).
  //
  // This function should be used to filter bad samples before serializing them.
  bool IsValid() const;

  // Getters for type, name, and num_samples. All types of metrics have these so
  // we do not need to check the type.
  SampleType type() const { return type_; }
  const std::string& name() const { return name_; }
  int num_samples() const { return num_samples_; }

  // Getters for sample, min, max, bucket_count.
  // Check the metric type to make sure the request make sense. (ex: a crash
  // sample does not have a bucket_count so we crash if we call bucket_count()
  // on it.)
  int sample() const;
  int min() const;
  int max() const;
  int bucket_count() const;

  // Returns a serialized version of the sample.
  //
  // The serialized message for each type is:
  // crash: crash\0|name_|\0
  // user action: useraction\0|name_|\0
  // histogram: histogram\0|name_| |sample_| |min_| |max_| |bucket_count_|\0
  // sparsehistogram: sparsehistogram\0|name_| |sample_|\0
  // linearhistogram: linearhistogram\0|name_| |sample_| |max_|\0
  //
  // Additionally, if num_samples is not 1, each type may have:
  // ` |num_samples_|` immediately before the final null terminator.
  std::string ToString() const;

  // Builds a crash sample.
  static std::unique_ptr<MetricSample> CrashSample(
      const std::string& crash_name,
      int num_samples);
  // Deserializes a crash sample.
  static std::unique_ptr<MetricSample> ParseCrash(
      const std::string& serialized);

  // Builds a histogram sample.
  static std::unique_ptr<MetricSample> HistogramSample(
      const std::string& histogram_name,
      int sample,
      int min,
      int max,
      int bucket_count,
      int num_samples);
  // Deserializes a histogram sample.
  static std::unique_ptr<MetricSample> ParseHistogram(
      const std::string& serialized);

  // Builds a sparse histogram sample.
  static std::unique_ptr<MetricSample> SparseHistogramSample(
      const std::string& histogram_name,
      int sample,
      int num_samples);
  // Deserializes a sparse histogram sample.
  static std::unique_ptr<MetricSample> ParseSparseHistogram(
      const std::string& serialized);

  // Builds a linear histogram sample.
  static std::unique_ptr<MetricSample> LinearHistogramSample(
      const std::string& histogram_name,
      int sample,
      int max,
      int num_samples);
  // Deserializes a linear histogram sample.
  static std::unique_ptr<MetricSample> ParseLinearHistogram(
      const std::string& serialized);

  // Builds a user action sample.
  static std::unique_ptr<MetricSample> UserActionSample(
      const std::string& action_name,
      int num_samples);
  // Deserializes a user action sample.
  static std::unique_ptr<MetricSample> ParseUserAction(
      const std::string& serialized);

  // Returns true if sample and this object represent the same sample (type,
  // name, sample, min, max, bucket_count match).
  bool IsEqual(const MetricSample& sample);

 private:
  const SampleType type_;
  const std::string name_;
  const int sample_;
  const int min_;
  const int max_;
  const int bucket_count_;
  const int num_samples_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SERIALIZATION_METRIC_SAMPLE_H_
