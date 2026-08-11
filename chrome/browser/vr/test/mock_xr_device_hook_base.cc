// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/xr_test_utils.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/test/controller_frame_data.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/gfx/geometry/decomposed_transform.h"

#if BUILDFLAG(ENABLE_OPENXR)
#include "device/vr/openxr/test/openxr_mock_helper.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/webxr/android/openxr_device_provider.h"
#endif

MockXRDeviceHookBase::MockXRDeviceHookBase() {
  thread_ = std::make_unique<base::Thread>("MockXRDeviceHookThread");
  thread_->Start();

  // By default, `mock_device_sequence_` is bound to the constructing thread
  // (i.e. the main test thread). We must detach it so it can be bound to
  // our internal `thread_` the first time a checked method is called.
  DETACH_FROM_SEQUENCE(mock_device_sequence_);

  // TODO(https://crbug.com/381913614): Instead of this pattern, consider
  // spinning up/holding onto and setting the test hook on the XrRuntimeManager,
  // which could pass on to providers.
#if BUILDFLAG(IS_WIN)
  content::GetXRDeviceServiceForTesting()->BindTestHook(
      service_test_hook_.BindNewPipeAndPassReceiver());

  mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
  service_test_hook_->SetTestHook(
      receiver_.BindNewPipeAndPassRemote(thread_->task_runner()));
#elif BUILDFLAG(IS_ANDROID)
  mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
  // On Windows we have to rely on the ServiceTestHook to initialize the
  // trampoline, since the device code is embedded in the utility process.
  // However, on Android since the device code is embedded in our process/the
  // browser process we need to ensure that we initialize the trampoline.
#if BUILDFLAG(ENABLE_OPENXR)
  InitializeOpenXrMockTrampoline();
#endif
  webxr::OpenXrDeviceProvider::SetTestHook(
      receiver_.BindNewPipeAndPassRemote(thread_->task_runner()));
#endif
}

MockXRDeviceHookBase::~MockXRDeviceHookBase() {
  StopHooking();

  if (thread_->IsRunning()) {
    thread_->Stop();
  }
}

void MockXRDeviceHookBase::StopHooking() {
  // Ensure that this is being called from our main thread, and not the mock
  // device thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  // We don't call service_test_hook_->SetTestHook(mojo::NullRemote()), since
  // that will potentially deadlock with reentrant or crossing synchronous mojo
  // calls.
  service_test_hook_.reset();
  // Unretained is safe here because we are going to block until this message
  // has been processed.
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojo::Receiver<device_test::mojom::XRTestHook>::reset,
                     base::Unretained(&receiver_)));
  // Mojo messages and this destruction task are the only thing that should be
  // posted to the thread. Since we're destroying the mojo pipe, we can safely
  // block here.
  thread_->FlushForTesting();
}

void MockXRDeviceHookBase::WaitNumFrames(uint32_t num_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  WaitForTotalFrameCount(frame_count_ + num_frames);
}

void MockXRDeviceHookBase::WaitForTotalFrameCount(uint32_t total_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  target_frame_count_ = total_count;

  // No need to wait if we've already had at least the requested number of
  // frames submitted.
  if (frame_count_ >= target_frame_count_) {
    return;
  }
  base::RunLoop wait_loop(base::RunLoop::Type::kNestableTasksAllowed);
  {
    base::AutoLock lock(lock_);
    wait_loop_quit_closure_ = wait_loop.QuitClosure();
  }

  wait_loop.Run();

  {
    base::AutoLock lock(lock_);
    wait_loop_quit_closure_.Reset();
  }
}

void MockXRDeviceHookBase::OnFrameSubmitted(
    const std::vector<device::ViewData>& views,
    const std::vector<device::LayerData>& layers,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  frame_count_++;
  ProcessSubmittedFrameUnlocked(views, layers);
  if (frame_count_ >= target_frame_count_) {
    base::RepeatingClosure quit_closure;
    {
      base::AutoLock lock(lock_);
      quit_closure = wait_loop_quit_closure_;
    }
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  std::move(callback).Run();
}

void MockXRDeviceHookBase::WaitGetDeviceConfig(
    device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  device::DeviceConfig ret = {.interpupillary_distance = 0.1f};
  std::move(callback).Run(std::move(ret));
}

void MockXRDeviceHookBase::WaitGetPresentingPose(
    device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  gfx::Transform pose;
  {
    base::AutoLock lock(lock_);
    if (head_pose_) {
      pose = *head_pose_;
    }
  }
  std::move(callback).Run(pose);
}

void MockXRDeviceHookBase::WaitGetMagicWindowPose(
    device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  std::move(callback).Run(gfx::Transform());
}

void MockXRDeviceHookBase::WaitGetAllControllerData(
    device_test::mojom::XRTestHook::WaitGetAllControllerDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  std::vector<device::ControllerFrameData> ret;
  {
    base::AutoLock lock(lock_);
    ret.reserve(input_sources_.size());
    for (const auto& source : input_sources_) {
      ret.push_back(source->GetFrameData());
    }
  }
  std::move(callback).Run(std::move(ret));
}

void MockXRDeviceHookBase::WaitGetEventData(
    device_test::mojom::XRTestHook::WaitGetEventDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  device_test::mojom::EventDataPtr ret = device_test::mojom::EventData::New();
  ret->type = device_test::mojom::EventType::kNoEvent;
  {
    base::AutoLock lock(lock_);
    if (!event_data_queue_.empty()) {
      ret = device_test::mojom::EventData::New(event_data_queue_.front());
      event_data_queue_.pop();
    }
  }
  std::move(callback).Run(std::move(ret));
}

MockXRInputSource& MockXRDeviceHookBase::CreateInputSource(
    device::mojom::XRHandedness handedness,
    bool has_hand_tracking) {
  base::AutoLock lock(lock_);
  auto source =
      std::make_unique<MockXRInputSource>(this, handedness, has_hand_tracking);
  MockXRInputSource* ptr = source.get();
  input_sources_.push_back(std::move(source));
  return *ptr;
}

MockXRInputSource& MockXRDeviceHookBase::CreateMinimalGamepad(
    device::mojom::XRHandedness handedness) {
  auto& source = CreateInputSource(handedness);
  source.SetSupportedButtons({device::XrButtonId::kAxisTrigger});
  return source;
}

void MockXRDeviceHookBase::SetHeadPose(const gfx::Transform& pose) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  head_pose_ = pose;
}

void MockXRDeviceHookBase::SimulateSessionLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  device_test::mojom::EventData event;
  event.type = device_test::mojom::EventType::kSessionLost;
  PopulateEvent(event);
}

void MockXRDeviceHookBase::SimulateVisibilityBlurred() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  device_test::mojom::EventData event;
  event.type = device_test::mojom::EventType::kVisibilityVisibleBlurred;
  PopulateEvent(event);
}

void MockXRDeviceHookBase::SimulateInteractionProfileChanged(
    device::mojom::OpenXrInteractionProfileType profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  device_test::mojom::EventData event;
  event.type = device_test::mojom::EventType::kInteractionProfileChanged;
  event.interaction_profile = profile;
  PopulateEvent(event);
}

void MockXRDeviceHookBase::SimulateInstanceLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  device_test::mojom::EventData event;
  event.type = device_test::mojom::EventType::kInstanceLost;
  PopulateEvent(event);
}

void MockXRDeviceHookBase::PopulateEvent(device_test::mojom::EventData data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  event_data_queue_.push(data);
}

void MockXRDeviceHookBase::WaitGetCanCreateSession(
    device_test::mojom::XRTestHook::WaitGetCanCreateSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  std::move(callback).Run(can_create_session_);
}

void MockXRDeviceHookBase::SetCanCreateSession(bool can_create_session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  can_create_session_ = can_create_session;
}

void MockXRDeviceHookBase::SetVisibilityMaskForTesting(
    uint32_t view_index,
    device::mojom::XRVisibilityMaskPtr mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  visibility_masks_[view_index] = std::move(mask);
}

void MockXRDeviceHookBase::WaitGetVisibilityMask(
    uint32_t view_index,
    device_test::mojom::XRTestHook::WaitGetVisibilityMaskCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  device::mojom::XRVisibilityMaskPtr mask;
  {
    base::AutoLock lock(lock_);
    if (auto it = visibility_masks_.find(view_index);
        it != visibility_masks_.end()) {
      mask = it->second.Clone();
    }
  }

  std::move(callback).Run(std::move(mask));
}

void MockXRDeviceHookBase::TerminateDeviceServiceProcessForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
  service_test_hook_->TerminateDeviceServiceProcessForTesting();
}
