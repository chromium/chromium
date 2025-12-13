// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"

#include "content/public/test/xr_test_utils.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
  DCHECK(!can_signal_wait_loop_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  target_frame_count_ = total_count;

  // No need to wait if we've already had at least the requested number of
  // frames submitted.
  if (frame_count_ >= target_frame_count_) {
    return;
  }
  wait_loop_ = std::make_unique<base::RunLoop>(
      base::RunLoop::Type::kNestableTasksAllowed);
  can_signal_wait_loop_ = true;

  wait_loop_->Run();

  can_signal_wait_loop_ = false;
  wait_loop_.reset();
}

void MockXRDeviceHookBase::OnFrameSubmitted(
    const std::vector<device::ViewData>& views,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  frame_count_++;
  ProcessSubmittedFrameUnlocked(std::move(views));
  if (can_signal_wait_loop_ && frame_count_ >= target_frame_count_) {
    wait_loop_->Quit();
    can_signal_wait_loop_ = false;
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
  std::move(callback).Run(gfx::Transform());
}

void MockXRDeviceHookBase::WaitGetMagicWindowPose(
    device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  std::move(callback).Run(gfx::Transform());
}

void MockXRDeviceHookBase::WaitGetControllerRoleForTrackedDeviceIndex(
    uint32_t index,
    device_test::mojom::XRTestHook::
        WaitGetControllerRoleForTrackedDeviceIndexCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  device::ControllerRole role = device::ControllerRole::kControllerRoleInvalid;
  {
    base::AutoLock lock(lock_);
    auto iter = controller_data_map_.find(index);
    if (iter != controller_data_map_.end()) {
      role = iter->second.role;
    }
  }

  std::move(callback).Run(role);
}

void MockXRDeviceHookBase::WaitGetControllerData(
    uint32_t index,
    device_test::mojom::XRTestHook::WaitGetControllerDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  device::ControllerFrameData data;
  {
    base::AutoLock lock(lock_);
    auto iter = controller_data_map_.find(index);
    if (iter != controller_data_map_.end()) {
      data = iter->second;
    } else {
      // Default to not being valid so that controllers aren't connected unless
      // a test specifically enables it.
      data =
          CreateValidController(device::ControllerRole::kControllerRoleInvalid);
      data.is_valid = false;
    }
  }
  std::move(callback).Run(std::move(data));
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

uint32_t MockXRDeviceHookBase::ConnectController(
    const device::ControllerFrameData& initial_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  auto index = next_controller_id_++;
  CHECK_LT(index, device::kMaxControllers);
  controller_data_map_.insert_or_assign(index, initial_data);
  return index;
}

void MockXRDeviceHookBase::TerminateDeviceServiceProcessForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
  service_test_hook_->TerminateDeviceServiceProcessForTesting();
}

void MockXRDeviceHookBase::UpdateController(
    uint32_t index,
    const device::ControllerFrameData& updated_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  auto iter = controller_data_map_.find(index);
  CHECK(iter != controller_data_map_.end());
  iter->second = updated_data;
}

void MockXRDeviceHookBase::DisconnectController(uint32_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  auto iter = controller_data_map_.find(index);
  CHECK(iter != controller_data_map_.end());
  controller_data_map_.erase(iter);
}

device::ControllerFrameData MockXRDeviceHookBase::CreateValidController(
    device::ControllerRole role) {
  // Stateless helper may be called on any sequence.
  device::ControllerFrameData ret;
  // Because why shouldn't a 64 button controller exist?
  ret.supported_buttons = UINT64_MAX;
  std::ranges::fill(ret.axis_data, device::ControllerAxisData{});
  ret.role = role;
  ret.is_valid = true;
  ret.pose_data = gfx::Transform();
  return ret;
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
    std::optional<device::VisibilityMaskData> mask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_);
  base::AutoLock lock(lock_);
  visibility_masks_[view_index] = std::move(mask);
}

void MockXRDeviceHookBase::WaitGetVisibilityMask(
    uint32_t view_index,
    device_test::mojom::XRTestHook::WaitGetVisibilityMaskCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  std::optional<device::VisibilityMaskData> mask;
  {
    base::AutoLock lock(lock_);
    if (visibility_masks_.contains(view_index)) {
      mask = visibility_masks_[view_index];
    }
  }

  std::move(callback).Run(std::move(mask));
}
