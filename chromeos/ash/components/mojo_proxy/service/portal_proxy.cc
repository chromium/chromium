// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/service/portal_proxy.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/buffer.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/platform_handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/trap.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/types.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/platform/platform_handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/message_pipe.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/platform_handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/trap.h"
#include "chromeos/ash/components/mojo_proxy/service/node_proxy.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy {

using mojo::core::ScopedIpczHandle;

PortalProxy::PortalProxy(const raw_ref<const IpczAPI> ipcz,
                         NodeProxy& node_proxy,
                         ScopedIpczHandle portal,
                         mojo_legacy::ScopedMessagePipeHandle pipe)
    : ipcz_(ipcz),
      node_proxy_(node_proxy),
      portal_(std::move(portal)),
      pipe_(std::move(pipe)) {
  CHECK_EQ(mojo_legacy::CreateTrap(&OnMojoPipeActivity, &pipe_trap_),
           mojo_legacy::MOJO_LEGACY_RESULT_OK);
  const mojo_legacy::MojoResult add_trigger_result =
      mojo_legacy::MojoAddTrigger(
          pipe_trap_->value(), pipe_->value(),
          mojo_legacy::MOJO_LEGACY_HANDLE_SIGNAL_READABLE,
          mojo_legacy::MOJO_LEGACY_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          trap_context(), nullptr);
  CHECK_EQ(add_trigger_result, mojo_legacy::MOJO_LEGACY_RESULT_OK);
}

PortalProxy::~PortalProxy() = default;

void PortalProxy::Start() {
  CHECK(!disconnected_);
  CHECK(!watching_portal_);
  CHECK(!watching_pipe_);

  Flush();
}

void PortalProxy::Flush() {
  CHECK(!in_flush_);
  in_flush_ = true;
  while (!disconnected_ && (!watching_portal_ || !watching_pipe_)) {
    if (!disconnected_ && !watching_portal_) {
      FlushAndWatchPortal();
    }
    if (!disconnected_ && !watching_pipe_) {
      FlushAndWatchPipe();
    }
  }
  in_flush_ = false;

  if (disconnected_) {
    // Deletes `this`.
    Die();
  }
}

void PortalProxy::FlushAndWatchPortal() {
  for (;;) {
    std::vector<uint8_t> data;
    size_t num_bytes = 0;
    std::vector<IpczHandle> handles;
    size_t num_handles = 0;
    IpczResult result =
        ipcz_->Get(portal_.get(), IPCZ_NO_FLAGS, nullptr, nullptr, &num_bytes,
                   nullptr, &num_handles, nullptr);
    if (result == IPCZ_RESULT_OK) {
      mojo_legacy::WriteMessageRaw(pipe_.get(), nullptr, 0, nullptr, 0,
                                   MOJO_LEGACY_WRITE_MESSAGE_FLAG_NONE);
      continue;
    }

    if (result == IPCZ_RESULT_UNAVAILABLE) {
      break;
    }

    if (result != IPCZ_RESULT_RESOURCE_EXHAUSTED) {
      disconnected_ = true;
      return;
    }

    data.resize(num_bytes);
    handles.resize(num_handles);
    result = ipcz_->Get(portal_.get(), IPCZ_NO_FLAGS, nullptr, data.data(),
                        &num_bytes, handles.data(), &num_handles, nullptr);
    CHECK_EQ(result, IPCZ_RESULT_OK);

    std::vector<mojo_legacy::MojoHandle> mojo_handles;
    mojo_handles.reserve(handles.size());
    for (IpczHandle handle : handles) {
      mojo_handles.push_back(TranslateIpczToMojoHandle(ScopedIpczHandle(handle))
                                 .release()
                                 .value());
    }

    mojo_legacy::WriteMessageRaw(pipe_.get(), data.data(), data.size(),
                                 mojo_handles.data(), mojo_handles.size(),
                                 MOJO_LEGACY_WRITE_MESSAGE_FLAG_NONE);
  }

  IpczTrapConditionFlags flags;
  const IpczTrapConditions trap_conditions{
      .size = sizeof(trap_conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS | IPCZ_TRAP_DEAD,
      .min_local_parcels = 0,
  };
  const IpczResult trap_result =
      ipcz_->Trap(portal_.get(), &trap_conditions, &OnIpczPortalActivity,
                  trap_context(), IPCZ_NO_FLAGS, nullptr, &flags, nullptr);
  if (trap_result == IPCZ_RESULT_OK) {
    watching_portal_ = true;
    return;
  }

  CHECK_EQ(trap_result, IPCZ_RESULT_FAILED_PRECONDITION);
  if (flags & IPCZ_TRAP_DEAD) {
    disconnected_ = true;
  }
}

void PortalProxy::FlushAndWatchPipe() {
  for (;;) {
    std::vector<uint8_t> data;
    std::vector<mojo_legacy::ScopedHandle> handles;
    const mojo_legacy::MojoResult result = mojo_legacy::ReadMessageRaw(
        pipe_.get(), &data, &handles, MOJO_LEGACY_READ_MESSAGE_FLAG_NONE);
    if (result == mojo_legacy::MOJO_LEGACY_RESULT_SHOULD_WAIT) {
      break;
    }

    if (result != mojo_legacy::MOJO_LEGACY_RESULT_OK) {
      disconnected_ = true;
      return;
    }

    std::vector<IpczHandle> ipcz_handles;
    ipcz_handles.reserve(handles.size());
    for (mojo_legacy::ScopedHandle& handle : handles) {
      ipcz_handles.push_back(
          TranslateMojoToIpczHandle(std::move(handle)).release());
    }

    const IpczResult put_result = ipcz_->Put(
        portal_.get(), data.size() ? data.data() : nullptr, data.size(),
        ipcz_handles.size() ? ipcz_handles.data() : nullptr,
        ipcz_handles.size(), IPCZ_NO_FLAGS, nullptr);
    if (put_result != IPCZ_RESULT_OK) {
      disconnected_ = true;
      return;
    }
  }

  uint32_t num_events = 1;
  mojo_legacy::MojoTrapEvent event{.struct_size = sizeof(event)};
  const mojo_legacy::MojoResult result = mojo_legacy::MojoArmTrap(
      pipe_trap_->value(), nullptr, &num_events, &event);
  if (result == mojo_legacy::MOJO_LEGACY_RESULT_OK) {
    watching_pipe_ = true;
    return;
  }

  CHECK_EQ(result, mojo_legacy::MOJO_LEGACY_RESULT_FAILED_PRECONDITION);
  CHECK_EQ(num_events, 1u);
  if (event.result == mojo_legacy::MOJO_LEGACY_RESULT_FAILED_PRECONDITION) {
    disconnected_ = true;
  }
}

ScopedIpczHandle PortalProxy::TranslateMojoToIpczHandle(
    mojo_legacy::ScopedHandle handle) {
  // We don't know what kind of handle is in `handle`, but we can find out.
  // First try to unwrap it as a generic platform handle.
  mojo_legacy::MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  const mojo_legacy::MojoResult unwrap_result =
      mojo_legacy::MojoUnwrapPlatformHandle(handle->value(), nullptr,
                                            &platform_handle);
  if (unwrap_result == mojo_legacy::MOJO_LEGACY_RESULT_OK) {
    std::ignore = handle.release();
    // Platform handles in ipcz are transmitted as boxed driver objects.
    return ScopedIpczHandle(
        mojo::core::ipcz_driver::WrappedPlatformHandle::MakeBoxed(
            mojo::PlatformHandle(
                mojo_legacy::PlatformHandle::FromMojoPlatformHandle(
                    &platform_handle)
                    .TakeFD())));
  }

  // We can non-destructively probe for a shared buffer handle by calling
  // MojoGetBufferInfo().
  mojo_legacy::MojoSharedBufferInfo info = {.struct_size = sizeof(info)};
  const mojo_legacy::MojoResult info_result =
      mojo_legacy::MojoGetBufferInfo(handle->value(), nullptr, &info);
  if (info_result == mojo_legacy::MOJO_LEGACY_RESULT_OK) {
    auto region = mojo_legacy::UnwrapPlatformSharedMemoryRegion(
        mojo_legacy::ScopedSharedBufferHandle{
            mojo_legacy::SharedBufferHandle{handle.release().value()}});
    return ScopedIpczHandle(
        mojo::core::ipcz_driver::SharedBuffer::MakeBoxed(std::move(region)));
  }

  // Since data pipe handles are never used on Chrome OS IPC boundaries outside
  // the browser, we can assume that any other handles are message pipes.
  IpczHandle portal_to_proxy, portal_to_host;
  ipcz_->OpenPortals(mojo::core::GetIpczNode(), IPCZ_NO_FLAGS, nullptr,
                     &portal_to_proxy, &portal_to_host);
  node_proxy_->AddPortalProxy(
      ScopedIpczHandle{portal_to_proxy},
      mojo_legacy::ScopedMessagePipeHandle{
          mojo_legacy::MessagePipeHandle{handle.release().value()}});
  return ScopedIpczHandle(portal_to_host);
}

mojo_legacy::ScopedHandle PortalProxy::TranslateIpczToMojoHandle(
    ScopedIpczHandle handle) {
  // Attempt a QueryPortalStatus() call. If this succeeds, we have a portal.
  IpczPortalStatus status = {.size = sizeof(status)};
  const IpczResult query_result =
      ipcz_->QueryPortalStatus(handle.get(), IPCZ_NO_FLAGS, nullptr, &status);
  if (query_result == IPCZ_RESULT_OK) {
    // Create a new Mojo message pipe to proxy through. One end is bound to a
    // new PortalProxy with the input `handle`; the other is returned to be
    // forwarded to the legacy client.
    mojo_legacy::MessagePipe pipe;
    node_proxy_->AddPortalProxy(std::move(handle), std::move(pipe.handle0));
    return mojo_legacy::ScopedHandle{
        mojo_legacy::Handle{pipe.handle1.release().value()}};
  }

  // Otherwise assume it's a boxed driver object. If it's not, something has
  // gone horribly wrong, so just crash.
  auto* object = mojo::core::ipcz_driver::ObjectBase::FromBox(handle.get());
  CHECK(object);
  switch (object->type()) {
    case mojo::core::ipcz_driver::ObjectBase::Type::kWrappedPlatformHandle: {
      auto wrapped_handle =
          mojo::core::ipcz_driver::WrappedPlatformHandle::Unbox(
              handle.release());
      return mojo_legacy::WrapPlatformHandle(
          mojo_legacy::PlatformHandle(wrapped_handle->TakeHandle().TakeFD()));
    }

    case mojo::core::ipcz_driver::ObjectBase::Type::kSharedBuffer: {
      auto buffer =
          mojo::core::ipcz_driver::SharedBuffer::Unbox(handle.release());
      auto mojo_buffer = mojo_legacy::WrapPlatformSharedMemoryRegion(
          std::move(buffer->region()));
      return mojo_legacy::ScopedHandle{
          mojo_legacy::Handle{mojo_buffer.release().value()}};
    }

    default:
      // No other types of driver objects are supported by the proxy.
      NOTREACHED();
  }
}

void PortalProxy::HandlePortalActivity(IpczTrapConditionFlags flags) {
  if (flags & IPCZ_TRAP_REMOVED) {
    // Proxy is being shut down. Do nothing.
    return;
  }

  watching_portal_ = false;
  if (flags & IPCZ_TRAP_DEAD) {
    disconnected_ = true;
    if (!in_flush_) {
      // Deletes `this`.
      Die();
      return;
    }
  } else if (!in_flush_) {
    Flush();
  }
}

void PortalProxy::HandlePipeActivity(mojo_legacy::MojoResult result) {
  if (result == mojo_legacy::MOJO_LEGACY_RESULT_CANCELLED) {
    // Proxy is being shut down. Do nothing.
    return;
  }

  watching_pipe_ = false;
  if (result == mojo_legacy::MOJO_LEGACY_RESULT_FAILED_PRECONDITION) {
    disconnected_ = true;
    if (!in_flush_) {
      // Deletes `this`.
      Die();
      return;
    }
  } else if (!in_flush_) {
    Flush();
  }
}

void PortalProxy::Die() {
  CHECK(!in_flush_);
  CHECK(disconnected_);

  // Deletes `this`.
  node_proxy_->RemovePortalProxy(this);
}

}  // namespace mojo_proxy
