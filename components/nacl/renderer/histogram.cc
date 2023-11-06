// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/histogram.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "build/build_config.h"

namespace nacl {

void HistogramCustomCounts(const std::string& name,
                           int32_t sample,
                           int32_t min,
                           int32_t max,
                           uint32_t bucket_count) {
  base::HistogramBase* counter =
      base::Histogram::FactoryGet(
          name,
          min,
          max,
          bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter)
    counter->Add(sample);
}

void HistogramEnumerate(const std::string& name,
                        int32_t sample,
                        int32_t boundary_value) {
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void HistogramEnumerateLoadStatus(PP_NaClError error_code,
                                  bool is_installed) {
  HistogramEnumerate("NaCl.LoadStatus.Plugin", error_code, PP_NACL_ERROR_MAX);

  // Gather data to see if being installed changes load outcomes.
  const char* name = is_installed ?
      "NaCl.LoadStatus.Plugin.InstalledApp" :
      "NaCl.LoadStatus.Plugin.NotInstalledApp";
  HistogramEnumerate(name, error_code, PP_NACL_ERROR_MAX);
}

void HistogramEnumerateOsArch(const std::string& sandbox_isa) {
  enum NaClOSArch {
    kNaClLinux32 = 0,
    kNaClLinux64,
    kNaClLinuxArm,
    kNaClMac32,
    kNaClMac64,
    kNaClMacArm,
    kNaClWin32,
    kNaClWin64,
    kNaClWinArm,
    kNaClLinuxMips,
    kNaClOSArchMax
  };

  NaClOSArch os_arch = kNaClOSArchMax;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  os_arch = kNaClLinux32;
#endif

  if (sandbox_isa == "x86-64")
    os_arch = static_cast<NaClOSArch>(os_arch + 1);
  if (sandbox_isa == "arm")
    os_arch = static_cast<NaClOSArch>(os_arch + 2);
  if (sandbox_isa == "mips32")
    os_arch = kNaClLinuxMips;

  HistogramEnumerate("NaCl.Client.OSArch", os_arch, kNaClOSArchMax);
}

// Records values up to 20 seconds.
void HistogramTimeSmall(const std::string& name, int64_t sample) {
  if (sample < 0)
    sample = 0;
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(1), base::Milliseconds(20000), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(base::Milliseconds(sample));
}

// Records values up to 3 minutes, 20 seconds.
void HistogramTimeMedium(const std::string& name, int64_t sample) {
  if (sample < 0)
    sample = 0;
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(10), base::Milliseconds(200000), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(base::Milliseconds(sample));
}

// Records values up to 33 minutes.
void HistogramTimeLarge(const std::string& name, int64_t sample) {
  if (sample < 0)
    sample = 0;
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(100), base::Milliseconds(2000000), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(base::Milliseconds(sample));
}

// Records values up to 12 minutes.
void HistogramTimeTranslation(const std::string& name, int64_t sample_ms) {
  if (sample_ms < 0)
    sample_ms = 0;
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(10), base::Milliseconds(720000), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(base::Milliseconds(sample_ms));
}

void HistogramStartupTimeSmall(const std::string& name,
                               base::TimeDelta td,
                               int64_t nexe_size) {
  HistogramTimeSmall(name, static_cast<int64_t>(td.InMilliseconds()));
  if (nexe_size > 0) {
    float size_in_MB = static_cast<float>(nexe_size) / (1024.f * 1024.f);
    HistogramTimeSmall(name + "PerMB",
                       static_cast<int64_t>(td.InMilliseconds() / size_in_MB));
  }
}

void HistogramStartupTimeMedium(const std::string& name,
                                base::TimeDelta td,
                                int64_t nexe_size) {
  HistogramTimeMedium(name, static_cast<int64_t>(td.InMilliseconds()));
  if (nexe_size > 0) {
    float size_in_MB = static_cast<float>(nexe_size) / (1024.f * 1024.f);
    HistogramTimeMedium(name + "PerMB",
                        static_cast<int64_t>(td.InMilliseconds() / size_in_MB));
  }
}

void HistogramSizeKB(const std::string& name, int32_t sample) {
  if (sample < 0) return;
  HistogramCustomCounts(name,
                        sample,
                        1,
                        512 * 1024,  // A very large .nexe.
                        100);
}

void HistogramHTTPStatusCode(const std::string& name,
                             int32_t status) {
  // Log the status codes in rough buckets - 1XX, 2XX, etc.
  int sample = status / 100;
  // HTTP status codes only go up to 5XX, using "6" to indicate an internal
  // error.
  // Note: installed files may have "0" for a status code.
  if (status < 0 || status >= 600)
    sample = 6;
  HistogramEnumerate(name, sample, 7);
}

void HistogramEnumerateManifestIsDataURI(bool is_data_uri) {
  HistogramEnumerate("NaCl.Manifest.IsDataURI", is_data_uri, 2);
}

void HistogramKBPerSec(const std::string& name, int64_t kb, int64_t us) {
  if (kb < 0 || us <= 0) return;
  static const double kMaxRate = 30 * 1000.0;  // max of 30MB/sec.
  int32_t rate = std::min(kb / (us / 1000000.0), kMaxRate);
  HistogramCustomCounts(name,
                        rate,
                        1,
                        30 * 1000,  // max of 30 MB/sec.
                        100);
}

void HistogramRatio(const std::string& name,
                    int64_t numerator,
                    int64_t denominator) {
  static const int32_t kRatioMin = 10;
  static const int32_t kRatioMax = 10 * 100;  // max of 10x difference.
  static const uint32_t kRatioBuckets = 100;
  if (numerator < 0 || denominator <= 0)
    return;
  HistogramCustomCounts(name,
                        static_cast<int32_t>(100 * numerator / denominator),
                        kRatioMin, kRatioMax, kRatioBuckets);
}

}  // namespace nacl
