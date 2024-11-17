// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_builder.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/call_stacks/call_stack_profile_encoding.h"
#include "components/sampling_profiler/call_stack_profile_params.h"

namespace metrics {

namespace {

// Only used by child processes. This returns a unique_ptr so that it can be
// reset during tests.
std::unique_ptr<ChildCallStackProfileCollector>&
GetChildCallStackProfileCollector() {
  static base::NoDestructor<std::unique_ptr<ChildCallStackProfileCollector>>
      instance(std::make_unique<ChildCallStackProfileCollector>());
  return *instance;
}

base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
GetBrowserProcessReceiverCallbackInstance() {
  static base::NoDestructor<
      base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>>
      instance;
  return *instance;
}

// Convert |filename| to its MD5 hash.
uint64_t HashModuleFilename(const base::FilePath& filename) {
  const base::FilePath::StringType basename = filename.BaseName().value();
  // Copy the bytes in basename into a string buffer.
  size_t basename_length_in_bytes =
      basename.size() * sizeof(base::FilePath::CharType);
  std::string name_bytes(basename_length_in_bytes, '\0');
  memcpy(&name_bytes[0], &basename[0], basename_length_in_bytes);
  return base::HashMetricName(name_bytes);
}

}  // namespace

CallStackProfileBuilder::CallStackProfileBuilder(
    const sampling_profiler::CallStackProfileParams& profile_params,
    const WorkIdRecorder* work_id_recorder,
    base::OnceClosure completed_callback)
    : work_id_recorder_(work_id_recorder) {
  completed_callback_ = std::move(completed_callback);
  sampled_profile_.set_process(
      ToExecutionContextProcess(profile_params.process));
  sampled_profile_.set_thread(ToExecutionContextThread(profile_params.thread));
  sampled_profile_.set_trigger_event(
      ToSampledProfileTriggerEvent(profile_params.trigger));
  if (!profile_params.time_offset.is_zero()) {
    DCHECK(profile_params.time_offset.is_positive());
    CallStackProfile* call_stack_profile =
        sampled_profile_.mutable_call_stack_profile();
    call_stack_profile->set_profile_time_offset_ms(
        profile_params.time_offset.InMilliseconds());
  }
}

CallStackProfileBuilder::~CallStackProfileBuilder() = default;

base::ModuleCache* CallStackProfileBuilder::GetModuleCache() {
  return &module_cache_;
}

// This function is invoked on the profiler thread while the target thread is
// suspended so must not take any locks, including indirectly through use of
// heap allocation, LOG, CHECK, or DCHECK.
void CallStackProfileBuilder::RecordMetadata(
    const base::MetadataRecorder::MetadataProvider& metadata_provider) {
  if (work_id_recorder_) {
    unsigned int work_id = work_id_recorder_->RecordWorkId();
    // A work id of 0 indicates that the message loop has not yet started.
    if (work_id != 0) {
      is_continued_work_ = (last_work_id_ == work_id);
      last_work_id_ = work_id;
    }
  }

  metadata_.RecordMetadata(metadata_provider);
}

void CallStackProfileBuilder::ApplyMetadataRetrospectively(
    base::TimeTicks period_start,
    base::TimeTicks period_end,
    const base::MetadataRecorder::Item& item) {
  CHECK_LE(period_start, period_end);
  CHECK_LE(period_end, base::TimeTicks::Now());

  // We don't set metadata if the period extends before the start of the
  // sampling, to avoid biasing against the unobserved execution. This will
  // introduce bias due to dropping periods longer than the sampling time, but
  // that bias is easier to reason about and account for.
  if (period_start < profile_start_time_)
    return;

  CallStackProfile* call_stack_profile =
      sampled_profile_.mutable_call_stack_profile();
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>* samples =
      call_stack_profile->mutable_stack_sample();

  CHECK_EQ(sample_timestamps_.size(), static_cast<size_t>(samples->size()));

  const ptrdiff_t start_offset =
      std::lower_bound(sample_timestamps_.begin(), sample_timestamps_.end(),
                       period_start) -
      sample_timestamps_.begin();
  const ptrdiff_t end_offset =
      std::upper_bound(sample_timestamps_.begin(), sample_timestamps_.end(),
                       period_end) -
      sample_timestamps_.begin();

  metadata_.ApplyMetadata(item, samples->begin() + start_offset,
                          samples->begin() + end_offset, samples,
                          call_stack_profile->mutable_metadata_name_hash());
}

void CallStackProfileBuilder::AddProfileMetadata(
    const base::MetadataRecorder::Item& item) {
  CallStackProfile* call_stack_profile =
      sampled_profile_.mutable_call_stack_profile();

  metadata_.SetMetadata(item,
                        call_stack_profile->mutable_profile_metadata()->Add(),
                        call_stack_profile->mutable_metadata_name_hash());
}

void CallStackProfileBuilder::OnSampleCompleted(
    std::vector<base::Frame> frames,
    base::TimeTicks sample_timestamp) {
  OnSampleCompleted(std::move(frames), sample_timestamp, 1, 1);
}

void CallStackProfileBuilder::OnSampleCompleted(
    std::vector<base::Frame> frames,
    base::TimeTicks sample_timestamp,
    size_t weight,
    size_t count) {
  // Write CallStackProfile::Stack protobuf message.
  CallStackProfile::Stack stack;

  for (const auto& frame : frames) {
    // keep the frame information even if its module is invalid so we have
    // visibility into how often this issue is happening on the server.
    CallStackProfile::Location* location = stack.add_frame();
    if (!frame.module)
      continue;

    // Dedup modules.
    auto module_loc = module_index_.find(frame.module);
    if (module_loc == module_index_.end()) {
      modules_.push_back(frame.module.get());
      size_t index = modules_.size() - 1;
      module_loc = module_index_.emplace(frame.module, index).first;
    }

    // Write CallStackProfile::Location protobuf message.
    uintptr_t instruction_pointer = frame.instruction_pointer;
#if BUILDFLAG(IS_IOS)
#if !TARGET_IPHONE_SIMULATOR
    // Some iOS devices enable pointer authentication, which uses the
    // higher-order bits of pointers to store a signature. Strip that signature
    // off before computing the module_offset.
    // TODO(crbug.com/40131654): Use the ptrauth_strip() macro once it is
    // available.
    instruction_pointer &= 0xFFFFFFFFF;
#endif  // !TARGET_IPHONE_SIMULATOR
#endif  // BUILDFLAG(IS_IOS)

    ptrdiff_t module_offset =
        reinterpret_cast<const char*>(instruction_pointer) -
        reinterpret_cast<const char*>(frame.module->GetBaseAddress());
    DCHECK_GE(module_offset, 0);
    location->set_address(static_cast<uint64_t>(module_offset));
    location->set_module_id_index(module_loc->second);

    if (!frame.function_name.empty()) {
      location->set_function_name(frame.function_name);
    }
  }

  CallStackProfile* call_stack_profile =
      sampled_profile_.mutable_call_stack_profile();

  // Dedup Stacks.
  auto stack_loc = stack_index_.find(&stack);
  if (stack_loc == stack_index_.end()) {
    *call_stack_profile->add_stack() = std::move(stack);
    int stack_index = call_stack_profile->stack_size() - 1;
    // It is safe to store the Stack pointer because the repeated message
    // representation ensures pointer stability.
    stack_loc = stack_index_
                    .emplace(call_stack_profile->mutable_stack(stack_index),
                             stack_index)
                    .first;
  }

  // Write CallStackProfile::StackSample protobuf message.
  CallStackProfile::StackSample* stack_sample_proto =
      call_stack_profile->add_stack_sample();
  stack_sample_proto->set_stack_index(stack_loc->second);
  if (weight != 1)
    stack_sample_proto->set_weight(weight);
  if (count != 1)
    stack_sample_proto->set_count(count);
  if (is_continued_work_)
    stack_sample_proto->set_continued_work(is_continued_work_);

  *stack_sample_proto->mutable_metadata() = metadata_.CreateSampleMetadata(
      call_stack_profile->mutable_metadata_name_hash());

  if (profile_start_time_.is_null())
    profile_start_time_ = sample_timestamp;

  sample_timestamps_.push_back(sample_timestamp);
}

void CallStackProfileBuilder::OnProfileCompleted(
    base::TimeDelta profile_duration,
    base::TimeDelta sampling_period) {
  // Build the SampledProfile protobuf message.
  CallStackProfile* call_stack_profile =
      sampled_profile_.mutable_call_stack_profile();
  call_stack_profile->set_profile_duration_ms(
      profile_duration.InMilliseconds());
  call_stack_profile->set_sampling_period_ms(sampling_period.InMilliseconds());

  // Write CallStackProfile::ModuleIdentifier protobuf message.
  for (const base::ModuleCache::Module* module : modules_) {
    CallStackProfile::ModuleIdentifier* module_id =
        call_stack_profile->add_module_id();
    module_id->set_build_id(module->GetId());
    module_id->set_name_md5_prefix(
        HashModuleFilename(module->GetDebugBasename()));
  }
  // sampled_profile_ cannot be reused after it is cleared by this function.
  // Check we still have the information from the constructor.
  CHECK(sampled_profile_.has_process());
  CHECK(sampled_profile_.has_thread());
  CHECK(sampled_profile_.has_trigger_event());

  PassProfilesToMetricsProvider(profile_start_time_,
                                std::move(sampled_profile_));
  // Protobuffers are in an uncertain state after moving from; clear to get
  // back to known state.
  sampled_profile_.Clear();

  // Run the completed callback if there is one.
  if (!completed_callback_.is_null())
    std::move(completed_callback_).Run();

  // Clear the caches.
  stack_index_.clear();
  module_index_.clear();
  modules_.clear();
  sample_timestamps_.clear();
  work_id_recorder_ = nullptr;
}

// static
void CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
    const base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
        callback) {
  GetBrowserProcessReceiverCallbackInstance() = callback;
}

// static
void CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector>
        browser_interface) {
  GetChildCallStackProfileCollector()->SetParentProfileCollector(
      std::move(browser_interface));
}

// static
void CallStackProfileBuilder::ResetChildCallStackProfileCollectorForTesting() {
  GetChildCallStackProfileCollector() =
      std::make_unique<ChildCallStackProfileCollector>();
}

void CallStackProfileBuilder::PassProfilesToMetricsProvider(
    base::TimeTicks profile_start_time,
    SampledProfile sampled_profile) {
  if (sampled_profile.process() == BROWSER_PROCESS) {
    GetBrowserProcessReceiverCallbackInstance().Run(profile_start_time,
                                                    std::move(sampled_profile));
  } else {
    GetChildCallStackProfileCollector()->Collect(profile_start_time,
                                                 std::move(sampled_profile));
  }
}

bool CallStackProfileBuilder::StackComparer::operator()(
    const CallStackProfile::Stack* stack1,
    const CallStackProfile::Stack* stack2) const {
  return std::lexicographical_compare(
      stack1->frame().begin(), stack1->frame().end(), stack2->frame().begin(),
      stack2->frame().end(),
      [](const CallStackProfile::Location& loc1,
         const CallStackProfile::Location& loc2) {
        return std::make_pair(loc1.address(), loc1.module_id_index()) <
               std::make_pair(loc2.address(), loc2.module_id_index());
      });
}

}  // namespace metrics
