// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_uma_recorder.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
// Generated JNI header must be included.
#include "components/cronet/android/cronet_jni_headers/CronetUmaRecorder_jni.h"

namespace cronet {

namespace {
base::NoDestructor<std::optional<CronetUmaRecorder>> g_instance;
void CronetUmaCallback(std::string_view histogram_name,
                       uint64_t name_hash,
                       base::HistogramBase::Sample32 sample,
                       std::optional<uint64_t> event_id) {
  CronetUmaRecorder::GetInstance().AddSample(name_hash, sample);
}

std::optional<absl::flat_hash_set<uint64_t>> ParseAllowlist(
    const std::string& allowlist) {
  if (allowlist == "*") {
    return std::nullopt;
  }

  absl::flat_hash_set<uint64_t> allowed_hashes;
  const std::vector<std::string_view> tokens = base::SplitStringPiece(
      allowlist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& token : tokens) {
    uint64_t parsed_hash;
    CHECK(base::StringToUint64(token, &parsed_hash))
        << "Failed to parse name hash in UMA allowlist: " << token;
    allowed_hashes.insert(parsed_hash);
  }
  return allowed_hashes;
}
}  // namespace

static void JNI_CronetUmaRecorder_InitNative(JNIEnv* env,
                                             const std::string& allowlist) {
  // Eagerly initialize the singleton at startup to parse the allowlist and
  // set up the background sequenced runner. Doing this now prevents the
  // overhead of lazy initialization and allowlist parsing from happening on a
  // critical, performance-sensitive thread when the first UMA sample is logged.
  CronetUmaRecorder::InitializeWithAllowlist(allowlist);
  base::StatisticsRecorder::SetGlobalSampleCallback(CronetUmaCallback);
}

static void JNI_CronetUmaRecorder_TriggerUmaHistogramForTesting(
    JNIEnv* env,
    const std::string& histogram_name,
    int32_t value) {
  base::UmaHistogramExactLinear(histogram_name, value, 100);
}

void CronetUmaRecorder::InitializeWithAllowlist(const std::string& allowlist) {
  CHECK(!g_instance->has_value());
  g_instance->emplace(base::PassKey<CronetUmaRecorder>(), allowlist);
}

CronetUmaRecorder& CronetUmaRecorder::GetInstance() {
  CHECK(g_instance->has_value())
      << "CronetUmaRecorder accessed before initialization!";
  return g_instance->value();
}

CronetUmaRecorder::CronetUmaRecorder(base::PassKey<CronetUmaRecorder>,
                                     const std::string& allowlist)
    : allowed_name_hashes_(ParseAllowlist(allowlist)) {}

CronetUmaRecorder::~CronetUmaRecorder() = default;

bool CronetUmaRecorder::IsHashAllowed(uint64_t name_hash) const {
  return !allowed_name_hashes_.has_value() ||
         allowed_name_hashes_->contains(name_hash);
}

void CronetUmaRecorder::AddSample(uint64_t name_hash, int32_t value) {
  if (!IsHashAllowed(name_hash)) {
    return;
  }

  base::AutoLock auto_lock(lock_);
  if (queue_.size() >= kMaxPendingTasks) {
    // TODO(crbug.com/514272664): Keep track of the number of dropped samples
    // and report them later so we are not blind to data loss.
    return;
  }

  const bool was_empty = queue_.empty();
  queue_.push_back({name_hash, value});

  if (was_empty) {
    // Always schedule with a 100ms delay to enforce the rate limit boundary
    // safely.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CronetUmaRecorder::ProcessQueue,
                       base::Unretained(this)),
        kRateLimitPeriod);
  }
}

void CronetUmaRecorder::ProcessQueue() {
  TRACE_EVENT0("cronet", "CronetUmaRecorder::ProcessQueue");
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  std::vector<Sample> samples_to_process;
  bool should_reschedule;
  {
    base::AutoLock auto_lock(lock_);
    const size_t num_to_move = std::min(queue_.size(), kMaxSamplesPerPeriod);
    samples_to_process.insert(samples_to_process.end(), queue_.begin(),
                              queue_.begin() + num_to_move);
    queue_.erase(queue_.begin(), queue_.begin() + num_to_move);
    should_reschedule = !queue_.empty();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  for (const auto& sample : samples_to_process) {
    Java_CronetUmaRecorder_logCronetUmaHistogram(
        env, static_cast<int64_t>(sample.hash), sample.value);
  }

  // If we have more samples left in the shared queue, schedule the next batch
  // to be processed after the rate limit period (100ms).
  // Note: We reschedule based on the queue state captured under the lock
  // (`should_reschedule`). If the queue was empty when checked, we won't
  // reschedule. If another thread calls RecordUma() in the meantime (e.g.,
  // while we are performing JNI calls), it will detect that
  // `queue_` is empty, add the sample, and post a new ProcessQueue
  // task itself. Relying on the local `should_reschedule` here prevents us from
  // double-posting in that scenario.
  if (should_reschedule) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CronetUmaRecorder::ProcessQueue,
                       base::Unretained(this)),
        kRateLimitPeriod);
  }
}

}  // namespace cronet

DEFINE_JNI(CronetUmaRecorder)
