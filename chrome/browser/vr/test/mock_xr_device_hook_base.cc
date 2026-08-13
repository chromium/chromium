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
#include "ui/gfx/geometry/decomposed_transform.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webxr/android/openxr_platform_helper_android.h"
#include "device/vr/openxr/test/openxr_test_helper.h"
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
  content::GetXRDeviceServiceForTesting()->BindHookForTesting(
      receiver_.BindNewPipeAndPassRemote(thread_->task_runner()).PassPipe());
#elif BUILDFLAG(IS_ANDROID)
  webxr::OpenXrPlatformHelperAndroid::SetXrHostActivityDisabledForTesting(true);
  OpenXrTestHelper::Get().SetTestHook(
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
  // Note: On Windows we do not attempt to clear the test hook in the service
  // process with a synchronous call, since that could deadlock with reentrant
  // or crossing synchronous Mojo calls from active frames. Instead, resetting
  // `receiver_` below will close the pipe and trigger disconnection handling
  // in the service process's OpenXrTestHelper.
#if BUILDFLAG(IS_ANDROID)
  OpenXrTestHelper::Get().SetTestHook(mojo::NullRemote());
#endif
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

  // This method is called synchronously by the mock device in the child
  // process. Run the Mojo reply callback before running `quit_closure`, so that
  // the child process rendering thread is released from its synchronous IPC
  // call before the test runner thread unblocks and starts subsequent actions.
  std::move(callback).Run();

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
}

void MockXRDeviceHookBase::SetDeviceConfig(const device::DeviceConfig& config) {
  base::AutoLock lock(lock_);
  config_ = config;
}

void MockXRDeviceHookBase::WaitGetDeviceConfig(
    device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  device::DeviceConfig ret;
  {
    base::AutoLock lock(lock_);
    ret = config_;
  }
  std::move(callback).Run(std::move(ret));
}

void MockXRDeviceHookBase::WaitGetFrameData(
    device_test::mojom::XRTestHook::WaitGetFrameDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  UpdateFrameDataUnlocked();
  auto frame_data = device_test::mojom::XRTestFrameData::New();
  {
    base::AutoLock lock(lock_);
    if (head_pose_) {
      frame_data->head_pose = *head_pose_;
    }
    frame_data->controllers.reserve(input_sources_.size());
    for (const auto& source : input_sources_) {
      frame_data->controllers.push_back(source->GetFrameData());
    }
  }
  std::move(callback).Run(std::move(frame_data));
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
