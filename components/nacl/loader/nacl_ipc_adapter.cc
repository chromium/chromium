// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/nacl/loader/nacl_ipc_adapter.h"

#include <limits.h>
#include <string.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/single_thread_task_runner.h"
#include "base/tuple.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_platform_file.h"
#include "native_client/src/public/nacl_desc.h"
#include "native_client/src/public/nacl_desc_custom.h"
#include "native_client/src/trusted/desc/nacl_desc_quota.h"
#include "native_client/src/trusted/desc/nacl_desc_quota_interface.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_handle.h"

using ppapi::proxy::NaClMessageScanner;

namespace {

enum BufferSizeStatus {
  // The buffer contains a full message with no extra bytes.
  MESSAGE_IS_COMPLETE,

  // The message doesn't fit and the buffer contains only some of it.
  MESSAGE_IS_TRUNCATED,

  // The buffer contains a full message + extra data.
  MESSAGE_HAS_EXTRA_DATA
};

BufferSizeStatus GetBufferStatus(const char* data, size_t len) {
  if (len < sizeof(NaClIPCAdapter::NaClMessageHeader))
    return MESSAGE_IS_TRUNCATED;

  const NaClIPCAdapter::NaClMessageHeader* header =
      reinterpret_cast<const NaClIPCAdapter::NaClMessageHeader*>(data);
  uint32_t message_size =
      sizeof(NaClIPCAdapter::NaClMessageHeader) + header->payload_size;

  if (len == message_size)
    return MESSAGE_IS_COMPLETE;
  if (len > message_size)
    return MESSAGE_HAS_EXTRA_DATA;
  return MESSAGE_IS_TRUNCATED;
}

//------------------------------------------------------------------------------
// This object allows the NaClDesc to hold a reference to a NaClIPCAdapter and
// forward calls to it.
struct DescThunker {
  explicit DescThunker(NaClIPCAdapter* adapter_arg)
      : adapter(adapter_arg) {
  }

  DescThunker(const DescThunker&) = delete;
  DescThunker& operator=(const DescThunker&) = delete;

  ~DescThunker() { adapter->CloseChannel(); }

  scoped_refptr<NaClIPCAdapter> adapter;
};

NaClIPCAdapter* ToAdapter(void* handle) {
  return static_cast<DescThunker*>(handle)->adapter.get();
}

// NaClDescCustom implementation.
void NaClDescCustomDestroy(void* handle) {
  delete static_cast<DescThunker*>(handle);
}

ssize_t NaClDescCustomSendMsg(void* handle, const NaClImcTypedMsgHdr* msg,
                              int /* flags */) {
  return static_cast<ssize_t>(ToAdapter(handle)->Send(msg));
}

ssize_t NaClDescCustomRecvMsg(void* handle, NaClImcTypedMsgHdr* msg,
                              int /* flags */) {
  return static_cast<ssize_t>(ToAdapter(handle)->BlockingReceive(msg));
}

NaClDesc* MakeNaClDescCustom(NaClIPCAdapter* adapter) {
  NaClDescCustomFuncs funcs = NACL_DESC_CUSTOM_FUNCS_INITIALIZER;
  funcs.Destroy = NaClDescCustomDestroy;
  funcs.SendMsg = NaClDescCustomSendMsg;
  funcs.RecvMsg = NaClDescCustomRecvMsg;
  // NaClDescMakeCustomDesc gives us a reference on the returned NaClDesc.
  return NaClDescMakeCustomDesc(new DescThunker(adapter), &funcs);
}

//------------------------------------------------------------------------------
// This object is passed to a NaClDescQuota to intercept writes and forward them
// to the NaClIPCAdapter, which checks quota. This is a NaCl-style struct. Don't
// add non-trivial fields or virtual methods. Construction should use malloc,
// because this is owned by the NaClDesc, and the NaCl Dtor code will call free.
struct QuotaInterface {
  // The "base" struct must be first. NaCl code expects a NaCl style ref-counted
  // object, so the "vtable" and other base class fields must be first.
  struct NaClDescQuotaInterface base NACL_IS_REFCOUNT_SUBCLASS;

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION NaClMessageScanner::FileIO* file_io;
};

static void QuotaInterfaceDtor(NaClRefCount* nrcp) {
  // Trivial class, just pass through to the "base" struct Dtor.
  nrcp->vtbl = reinterpret_cast<NaClRefCountVtbl*>(
      const_cast<NaClDescQuotaInterfaceVtbl*>(&kNaClDescQuotaInterfaceVtbl));
  (*nrcp->vtbl->Dtor)(nrcp);
}

static int64_t QuotaInterfaceWriteRequest(NaClDescQuotaInterface* ndqi,
                                          const uint8_t* /* unused_id */,
                                          int64_t offset,
                                          int64_t length) {
  if (offset < 0 || length < 0)
    return 0;
  if (std::numeric_limits<int64_t>::max() - length < offset)
    return 0;  // offset + length would overflow.
  int64_t max_offset = offset + length;
  if (max_offset < 0)
    return 0;

  QuotaInterface* quota_interface = reinterpret_cast<QuotaInterface*>(ndqi);
  NaClMessageScanner::FileIO* file_io = quota_interface->file_io;
  int64_t increase = max_offset - file_io->max_written_offset();
  if (increase <= 0 || file_io->Grow(increase))
    return length;

  return 0;
}

static int64_t QuotaInterfaceFtruncateRequest(NaClDescQuotaInterface* ndqi,
                                              const uint8_t* /* unused_id */,
                                              int64_t length) {
  // We can't implement SetLength on the plugin side due to sandbox limitations.
  // See crbug.com/156077.
  NOTREACHED_IN_MIGRATION();
  return 0;
}

static const struct NaClDescQuotaInterfaceVtbl kQuotaInterfaceVtbl = {
  {
    QuotaInterfaceDtor
  },
  QuotaInterfaceWriteRequest,
  QuotaInterfaceFtruncateRequest
};

NaClDesc* MakeNaClDescQuota(
    NaClMessageScanner::FileIO* file_io,
    NaClDesc* wrapped_desc) {
  // Create the QuotaInterface.
  QuotaInterface* quota_interface =
      static_cast<QuotaInterface*>(malloc(sizeof *quota_interface));
  if (quota_interface && NaClDescQuotaInterfaceCtor(&quota_interface->base)) {
    quota_interface->base.base.vtbl =
        (struct NaClRefCountVtbl *)(&kQuotaInterfaceVtbl);
    // QuotaInterface is a trivial class, so skip the ctor.
    quota_interface->file_io = file_io;
    // Create the NaClDescQuota.
    NaClDescQuota* desc = static_cast<NaClDescQuota*>(malloc(sizeof *desc));
    uint8_t unused_id[NACL_DESC_QUOTA_FILE_ID_LEN] = {0};
    if (desc && NaClDescQuotaCtor(desc,
                                  wrapped_desc,
                                  unused_id,
                                  &quota_interface->base)) {
      return &desc->base;
    }
    if (desc)
      NaClDescUnref(reinterpret_cast<NaClDesc*>(desc));
  }

  if (quota_interface)
    NaClDescQuotaInterfaceUnref(&quota_interface->base);

  return NULL;
}

//------------------------------------------------------------------------------

void DeleteChannel(IPC::Channel* channel) {
  delete channel;
}

// Translates Pepper's read/write open flags into the NaCl equivalents.
// Since the host has already opened the file, flags such as O_CREAT, O_TRUNC,
// and O_EXCL don't make sense, so we filter those out. If no read or write
// flags are set, the function returns NACL_ABI_O_RDONLY as a safe fallback.
int TranslatePepperFileReadWriteOpenFlags(int32_t pp_open_flags) {
  bool read = (pp_open_flags & PP_FILEOPENFLAG_READ) != 0;
  bool write = (pp_open_flags & PP_FILEOPENFLAG_WRITE) != 0;
  bool append = (pp_open_flags & PP_FILEOPENFLAG_APPEND) != 0;

  int nacl_open_flag = NACL_ABI_O_RDONLY;  // NACL_ABI_O_RDONLY == 0.
  if (read && (write || append)) {
    nacl_open_flag = NACL_ABI_O_RDWR;
  } else if (write || append) {
    nacl_open_flag = NACL_ABI_O_WRONLY;
  } else if (!read) {
    DLOG(WARNING) << "One of PP_FILEOPENFLAG_READ, PP_FILEOPENFLAG_WRITE, "
                  << "or PP_FILEOPENFLAG_APPEND should be set.";
  }
  if (append)
    nacl_open_flag |= NACL_ABI_O_APPEND;

  return nacl_open_flag;
}

class NaClDescWrapper {
 public:
  explicit NaClDescWrapper(NaClDesc* desc): desc_(desc) {}

  NaClDescWrapper(const NaClDescWrapper&) = delete;
  NaClDescWrapper& operator=(const NaClDescWrapper&) = delete;

  ~NaClDescWrapper() {
    NaClDescUnref(desc_);
  }

  NaClDesc* desc() { return desc_; }

 private:
  raw_ptr<NaClDesc> desc_;
};

std::unique_ptr<NaClDescWrapper> MakeShmRegionNaClDesc(
    base::subtle::PlatformSharedMemoryRegion region) {
  // Writable regions are not supported in NaCl.
  DCHECK_NE(region.GetMode(),
            base::subtle::PlatformSharedMemoryRegion::Mode::kWritable);
  size_t size = region.GetSize();
  base::subtle::ScopedPlatformSharedMemoryHandle handle =
      region.PassPlatformHandle();
  return std::make_unique<NaClDescWrapper>(
      NaClDescImcShmMake(handle.fd.release(),
                             size));
}

}  // namespace

class NaClIPCAdapter::RewrittenMessage {
 public:
  RewrittenMessage();
  ~RewrittenMessage() = default;

  bool is_consumed() const { return data_read_cursor_ == data_len_; }

  void SetData(const NaClIPCAdapter::NaClMessageHeader& header,
               const void* payload, size_t payload_length);

  int Read(NaClImcTypedMsgHdr* msg);

  void AddDescriptor(std::unique_ptr<NaClDescWrapper> desc) {
    descs_.push_back(std::move(desc));
  }

  size_t desc_count() const { return descs_.size(); }

 private:
  std::unique_ptr<char[]> data_;
  size_t data_len_;

  // Offset into data where the next read will happen. This will be equal to
  // data_len_ when all data has been consumed.
  size_t data_read_cursor_;

  // Wrapped descriptors for transfer to untrusted code.
  std::vector<std::unique_ptr<NaClDescWrapper>> descs_;
};

NaClIPCAdapter::RewrittenMessage::RewrittenMessage()
    : data_len_(0),
      data_read_cursor_(0) {
}

void NaClIPCAdapter::RewrittenMessage::SetData(
    const NaClIPCAdapter::NaClMessageHeader& header,
    const void* payload,
    size_t payload_length) {
  DCHECK(!data_.get() && data_len_ == 0);
  size_t header_len = sizeof(NaClIPCAdapter::NaClMessageHeader);
  data_len_ = header_len + payload_length;
  data_.reset(new char[data_len_]);

  memcpy(data_.get(), &header, sizeof(NaClIPCAdapter::NaClMessageHeader));
  memcpy(&data_[header_len], payload, payload_length);
}

int NaClIPCAdapter::RewrittenMessage::Read(NaClImcTypedMsgHdr* msg) {
  CHECK(data_len_ >= data_read_cursor_);
  char* dest_buffer = static_cast<char*>(msg->iov[0].base);
  size_t dest_buffer_size = msg->iov[0].length;
  size_t bytes_to_write = std::min(dest_buffer_size,
                                   data_len_ - data_read_cursor_);
  if (bytes_to_write == 0)
    return 0;

  memcpy(dest_buffer, &data_[data_read_cursor_], bytes_to_write);
  data_read_cursor_ += bytes_to_write;

  // Once all data has been consumed, transfer any file descriptors.
  if (is_consumed()) {
    nacl_abi_size_t desc_count = static_cast<nacl_abi_size_t>(descs_.size());
    CHECK(desc_count <= msg->ndesc_length);
    msg->ndesc_length = desc_count;
    for (nacl_abi_size_t i = 0; i < desc_count; i++) {
      // Copy the NaClDesc to the buffer and add a ref so it won't be freed
      // when we clear our vector.
      msg->ndescv[i] = descs_[i]->desc();
      NaClDescRef(descs_[i]->desc());
    }
    descs_.clear();
  } else {
    msg->ndesc_length = 0;
  }
  return static_cast<int>(bytes_to_write);
}

NaClIPCAdapter::LockedData::LockedData()
    : channel_closed_(false) {
}

NaClIPCAdapter::LockedData::~LockedData() {
}

NaClIPCAdapter::IOThreadData::IOThreadData() {
}

NaClIPCAdapter::IOThreadData::~IOThreadData() {
}

NaClIPCAdapter::NaClIPCAdapter(
    const IPC::ChannelHandle& handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& runner,
    ResolveFileTokenCallback resolve_file_token_cb,
    OpenResourceCallback open_resource_cb)
    : lock_(),
      cond_var_(&lock_),
      task_runner_(runner),
      resolve_file_token_cb_(std::move(resolve_file_token_cb)),
      open_resource_cb_(std::move(open_resource_cb)),
      locked_data_() {
  io_thread_data_.channel_ = IPC::Channel::CreateServer(handle, this, runner);
  // Note, we can not PostTask for ConnectChannelOnIOThread here. If we did,
  // and that task ran before this constructor completes, the reference count
  // would go to 1 and then to 0 because of the Task, before we've been returned
  // to the owning scoped_refptr, which is supposed to give us our first
  // ref-count.
}

NaClIPCAdapter::NaClIPCAdapter(std::unique_ptr<IPC::Channel> channel,
                               base::TaskRunner* runner)
    : lock_(), cond_var_(&lock_), task_runner_(runner), locked_data_() {
  io_thread_data_.channel_ = std::move(channel);
}

void NaClIPCAdapter::ConnectChannel() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NaClIPCAdapter::ConnectChannelOnIOThread, this));
}

// Note that this message is controlled by the untrusted code. So we should be
// skeptical of anything it contains and quick to give up if anything is fishy.
int NaClIPCAdapter::Send(const NaClImcTypedMsgHdr* msg) {
  if (msg->iov_length != 1)
    return -1;

  base::AutoLock lock(lock_);

  const char* input_data = static_cast<char*>(msg->iov[0].base);
  size_t input_data_len = msg->iov[0].length;
  if (input_data_len > IPC::Channel::kMaximumMessageSize) {
    ClearToBeSent();
    return -1;
  }

  // current_message[_len] refers to the total input data received so far.
  const char* current_message;
  size_t current_message_len;
  bool did_append_input_data;
  if (locked_data_.to_be_sent_.empty()) {
    // No accumulated data, we can avoid a copy by referring to the input
    // buffer (the entire message fitting in one call is the common case).
    current_message = input_data;
    current_message_len = input_data_len;
    did_append_input_data = false;
  } else {
    // We've already accumulated some data, accumulate this new data and
    // point to the beginning of the buffer.

    // Make sure our accumulated message size doesn't overflow our max. Since
    // we know that data_len < max size (checked above) and our current
    // accumulated value is also < max size, we just need to make sure that
    // 2x max size can never overflow.
    static_assert(IPC::Channel::kMaximumMessageSize < (UINT_MAX / 2),
                  "kMaximumMessageSize is too large, and may overflow");
    size_t new_size = locked_data_.to_be_sent_.size() + input_data_len;
    if (new_size > IPC::Channel::kMaximumMessageSize) {
      ClearToBeSent();
      return -1;
    }

    locked_data_.to_be_sent_.append(input_data, input_data_len);
    current_message = &locked_data_.to_be_sent_[0];
    current_message_len = locked_data_.to_be_sent_.size();
    did_append_input_data = true;
  }

  // Check the total data we've accumulated so far to see if it contains a full
  // message.
  switch (GetBufferStatus(current_message, current_message_len)) {
    case MESSAGE_IS_COMPLETE: {
      // Got a complete message, can send it out. This will be the common case.
      bool success = SendCompleteMessage(current_message, current_message_len);
      ClearToBeSent();
      return success ? static_cast<int>(input_data_len) : -1;
    }
    case MESSAGE_IS_TRUNCATED:
      // For truncated messages, just accumulate the new data (if we didn't
      // already do so above) and go back to waiting for more.
      if (!did_append_input_data)
        locked_data_.to_be_sent_.append(input_data, input_data_len);
      return static_cast<int>(input_data_len);
    case MESSAGE_HAS_EXTRA_DATA:
    default:
      // When the plugin gives us too much data, it's an error.
      ClearToBeSent();
      return -1;
  }
}

int NaClIPCAdapter::BlockingReceive(NaClImcTypedMsgHdr* msg) {
  if (msg->iov_length != 1)
    return -1;

  int retval = 0;
  {
    base::AutoLock lock(lock_);
    while (locked_data_.to_be_received_.empty() &&
           !locked_data_.channel_closed_)
      cond_var_.Wait();
    if (locked_data_.channel_closed_) {
      retval = -1;
    } else {
      retval = LockedReceive(msg);
      DCHECK(retval > 0);
    }
    cond_var_.Signal();
  }
  return retval;
}

void NaClIPCAdapter::CloseChannel() {
  {
    base::AutoLock lock(lock_);
    locked_data_.channel_closed_ = true;
    cond_var_.Signal();
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NaClIPCAdapter::CloseChannelOnIOThread, this));
}

NaClDesc* NaClIPCAdapter::MakeNaClDesc() {
  return MakeNaClDescCustom(this);
}

bool NaClIPCAdapter::OnMessageReceived(const IPC::Message& msg) {
  uint32_t type = msg.type();

  if (type == IPC_REPLY_ID) {
    int id = IPC::SyncMessage::GetMessageId(msg);
    auto it = io_thread_data_.pending_sync_msgs_.find(id);
    if (it != io_thread_data_.pending_sync_msgs_.end()) {
      type = it->second;
      io_thread_data_.pending_sync_msgs_.erase(it);
    }
  }
  // Handle PpapiHostMsg_OpenResource outside the lock as it requires sending
  // IPC to handle properly.
  if (type == PpapiHostMsg_OpenResource::ID) {
    base::PickleIterator iter = IPC::SyncMessage::GetDataIterator(&msg);
    uint64_t token_lo;
    uint64_t token_hi;
    if (!IPC::ReadParam(&msg, &iter, &token_lo) ||
        !IPC::ReadParam(&msg, &iter, &token_hi)) {
      return false;
    }

    if (token_lo != 0 || token_hi != 0) {
      // We've received a valid file token. Instead of using the file
      // descriptor received, we send the file token to the browser in
      // exchange for a new file descriptor and file path information.
      // That file descriptor can be used to construct a NaClDesc with
      // identity-based validation caching.
      //
      // We do not use file descriptors from the renderer with validation
      // caching; a compromised renderer should not be able to run
      // arbitrary code in a plugin process.
      //
      // We intentionally avoid deserializing the next parameter, which is an
      // instance of SerializedHandle, since doing so takes ownership from the
      // IPC stack. If we fail to get a resource from the file token, we will
      // still need to read the original parameter in SaveOpenResourceMessage().
      DCHECK(!resolve_file_token_cb_.is_null());

      // resolve_file_token_cb_ must be invoked from the I/O thread.
      resolve_file_token_cb_.Run(
          token_lo, token_hi,
          base::BindOnce(&NaClIPCAdapter::SaveOpenResourceMessage, this, msg));

      // In this case, we don't release the message to NaCl untrusted code
      // immediately. We defer it until we get an async message back from the
      // browser process.
      return true;
    }
  }
  return RewriteMessage(msg, type);
}

bool NaClIPCAdapter::RewriteMessage(const IPC::Message& msg, uint32_t type) {
  {
    base::AutoLock lock(lock_);
    std::unique_ptr<RewrittenMessage> rewritten_msg(new RewrittenMessage);

    typedef std::vector<ppapi::proxy::SerializedHandle> Handles;
    Handles handles;
    std::unique_ptr<IPC::Message> new_msg;

    if (!locked_data_.nacl_msg_scanner_.ScanMessage(
            msg, type, &handles, &new_msg))
      return false;

    // Now add any descriptors we found to rewritten_msg. |handles| is usually
    // empty, unless we read a message containing a FD or handle.
    for (ppapi::proxy::SerializedHandle& handle : handles) {
      std::unique_ptr<NaClDescWrapper> nacl_desc;
      switch (handle.type()) {
        case ppapi::proxy::SerializedHandle::SHARED_MEMORY_REGION: {
          nacl_desc = MakeShmRegionNaClDesc(handle.TakeSharedMemoryRegion());
          break;
        }
        case ppapi::proxy::SerializedHandle::SOCKET: {
          nacl_desc = std::make_unique<NaClDescWrapper>(NaClDescSyncSocketMake(
              handle.descriptor().fd
                  ));
          break;
        }
        case ppapi::proxy::SerializedHandle::FILE: {
          // Create the NaClDesc for the file descriptor. If quota checking is
          // required, wrap it in a NaClDescQuota.
          NaClDesc* desc = NaClDescIoMakeFromHandle(
              handle.descriptor().fd,
              TranslatePepperFileReadWriteOpenFlags(handle.open_flags()));
          if (desc && handle.file_io()) {
            desc = MakeNaClDescQuota(
                locked_data_.nacl_msg_scanner_.GetFile(handle.file_io()), desc);
          }
          if (desc)
            nacl_desc = std::make_unique<NaClDescWrapper>(desc);
          break;
        }

        case ppapi::proxy::SerializedHandle::INVALID: {
          // Nothing to do.
          break;
        }
        // No default, so the compiler will warn us if new types get added.
      }
      if (nacl_desc.get())
        rewritten_msg->AddDescriptor(std::move(nacl_desc));
    }
    if (new_msg)
      SaveMessage(*new_msg, std::move(rewritten_msg));
    else
      SaveMessage(msg, std::move(rewritten_msg));
    cond_var_.Signal();
  }
  return true;
}

std::unique_ptr<IPC::Message> CreateOpenResourceReply(
    const IPC::Message& orig_msg,
    ppapi::proxy::SerializedHandle sh) {
  // The creation of new_msg must be kept in sync with
  // SyncMessage::WriteSyncHeader.
  std::unique_ptr<IPC::Message> new_msg(new IPC::Message(
      orig_msg.routing_id(), orig_msg.type(), IPC::Message::PRIORITY_NORMAL));
  new_msg->set_reply();
  new_msg->WriteInt(IPC::SyncMessage::GetMessageId(orig_msg));

  // Write empty file tokens.
  new_msg->WriteUInt64(0);  // token_lo
  new_msg->WriteUInt64(0);  // token_hi

  ppapi::proxy::SerializedHandle::WriteHeader(sh.header(),
                                              new_msg.get());
  new_msg->WriteBool(true);  // valid == true
  // The file descriptor is at index 0. There's only ever one file
  // descriptor provided for this message type, so this will be correct.
  new_msg->WriteInt(0);

  return new_msg;
}

void NaClIPCAdapter::SaveOpenResourceMessage(
    const IPC::Message& orig_msg,
    IPC::PlatformFileForTransit ipc_fd,
    base::FilePath file_path) {
  // The path where an invalid ipc_fd is returned isn't currently
  // covered by any tests.
  if (ipc_fd == IPC::InvalidPlatformFileForTransit()) {
    base::PickleIterator iter = IPC::SyncMessage::GetDataIterator(&orig_msg);
    uint64_t token_lo;
    uint64_t token_hi;
    ppapi::proxy::SerializedHandle orig_sh;

    // These CHECKs could fail if the renderer sends this process a malformed
    // message, but that's OK because in general the renderer can cause the NaCl
    // loader process to exit.
    CHECK(IPC::ReadParam(&orig_msg, &iter, &token_lo));
    CHECK(IPC::ReadParam(&orig_msg, &iter, &token_hi));
    CHECK(IPC::ReadParam(&orig_msg, &iter, &orig_sh));
    CHECK(orig_sh.IsHandleValid());

    std::unique_ptr<NaClDescWrapper> desc_wrapper(
        new NaClDescWrapper(NaClDescIoMakeFromHandle(
            orig_sh.descriptor().fd,
            NACL_ABI_O_RDONLY)));

    // The file token didn't resolve successfully, so we give the
    // original FD to the client without making a validated NaClDesc.
    // However, we must rewrite the message to clear the file tokens.
    std::unique_ptr<IPC::Message> new_msg =
        CreateOpenResourceReply(orig_msg, std::move(orig_sh));

    std::unique_ptr<RewrittenMessage> rewritten_msg(new RewrittenMessage);
    rewritten_msg->AddDescriptor(std::move(desc_wrapper));
    {
      base::AutoLock lock(lock_);
      SaveMessage(*new_msg, std::move(rewritten_msg));
      cond_var_.Signal();
    }
    return;
  }

  // The file token was successfully resolved.
  std::string file_path_str = file_path.AsUTF8Unsafe();
  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(ipc_fd);

  ppapi::proxy::SerializedHandle sh;
  sh.set_file_handle(ipc_fd, PP_FILEOPENFLAG_READ, 0);
  std::unique_ptr<IPC::Message> new_msg =
      CreateOpenResourceReply(orig_msg, std::move(sh));
  std::unique_ptr<RewrittenMessage> rewritten_msg(new RewrittenMessage);

  struct NaClDesc* desc =
      NaClDescCreateWithFilePathMetadata(handle, file_path_str.c_str());
  rewritten_msg->AddDescriptor(std::make_unique<NaClDescWrapper>(desc));
  {
    base::AutoLock lock(lock_);
    SaveMessage(*new_msg, std::move(rewritten_msg));
    cond_var_.Signal();
  }
}

void NaClIPCAdapter::OnChannelConnected(int32_t peer_pid) {}

void NaClIPCAdapter::OnChannelError() {
  CloseChannel();
}

NaClIPCAdapter::~NaClIPCAdapter() {
  // Make sure the channel is deleted on the IO thread.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteChannel, io_thread_data_.channel_.release()));
}

int NaClIPCAdapter::LockedReceive(NaClImcTypedMsgHdr* msg) {
  lock_.AssertAcquired();

  if (locked_data_.to_be_received_.empty())
    return 0;
  RewrittenMessage& current = *locked_data_.to_be_received_.front();

  int retval = current.Read(msg);

  // When a message is entirely consumed, remove it from the waiting queue.
  if (current.is_consumed())
    locked_data_.to_be_received_.pop();

  return retval;
}

bool NaClIPCAdapter::SendCompleteMessage(const char* buffer,
                                         size_t buffer_len) {
  lock_.AssertAcquired();
  // The message will have already been validated, so we know it's large enough
  // for our header.
  const NaClMessageHeader* header =
      reinterpret_cast<const NaClMessageHeader*>(buffer);

  // Length of the message not including the body. The data passed to us by the
  // plugin should match that in the message header. This should have already
  // been validated by GetBufferStatus.
  size_t body_len = buffer_len - sizeof(NaClMessageHeader);
  CHECK(body_len == header->payload_size);

  // We actually discard the flags and only copy the ones we care about. This
  // is just because message doesn't have a constructor that takes raw flags.
  std::unique_ptr<IPC::Message> msg(new IPC::Message(
      header->routing, header->type, IPC::Message::PRIORITY_NORMAL));
  if (header->flags & IPC::Message::SYNC_BIT)
    msg->set_sync();
  if (header->flags & IPC::Message::REPLY_BIT)
    msg->set_reply();
  if (header->flags & IPC::Message::REPLY_ERROR_BIT)
    msg->set_reply_error();
  if (header->flags & IPC::Message::UNBLOCK_BIT)
    msg->set_unblock(true);

  msg->WriteBytes(&buffer[sizeof(NaClMessageHeader)], body_len);

  // Technically we didn't have to do any of the previous work in the lock. But
  // sometimes our buffer will point to the to_be_sent_ string which is
  // protected by the lock, and it's messier to factor Send() such that it can
  // unlock for us. Holding the lock for the message construction, which is
  // just some memcpys, shouldn't be a big deal.
  lock_.AssertAcquired();
  if (locked_data_.channel_closed_) {
    // If we ever pass handles from the plugin to the host, we should close them
    // here before we drop the message.
    return false;
  }

  // Scan all untrusted messages.
  std::unique_ptr<IPC::Message> new_msg;
  locked_data_.nacl_msg_scanner_.ScanUntrustedMessage(*msg, &new_msg);
  if (new_msg)
    msg = std::move(new_msg);

  // Actual send must be done on the I/O thread.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NaClIPCAdapter::SendMessageOnIOThread, this,
                                std::move(msg)));
  return true;
}

void NaClIPCAdapter::ClearToBeSent() {
  lock_.AssertAcquired();

  // Don't let the string keep its buffer behind our back.
  std::string empty;
  locked_data_.to_be_sent_.swap(empty);
}

void NaClIPCAdapter::ConnectChannelOnIOThread() {
  if (!io_thread_data_.channel_->Connect())
    NOTREACHED_IN_MIGRATION();
}

void NaClIPCAdapter::CloseChannelOnIOThread() {
  io_thread_data_.channel_->Close();
}

void NaClIPCAdapter::SendMessageOnIOThread(
    std::unique_ptr<IPC::Message> message) {
  int id = IPC::SyncMessage::GetMessageId(*message.get());
  DCHECK(io_thread_data_.pending_sync_msgs_.find(id) ==
         io_thread_data_.pending_sync_msgs_.end());

  // Handle PpapiHostMsg_OpenResource locally without sending an IPC to the
  // renderer when possible.
  PpapiHostMsg_OpenResource::Schema::SendParam send_params;
  if (!open_resource_cb_.is_null() &&
      message->type() == PpapiHostMsg_OpenResource::ID &&
      PpapiHostMsg_OpenResource::ReadSendParam(message.get(), &send_params)) {
    const std::string key = std::get<0>(send_params);
    // Both open_resource_cb_ and SaveOpenResourceMessage must be invoked
    // from the I/O thread.
    if (open_resource_cb_.Run(
            *message.get(), key,
            base::BindOnce(&NaClIPCAdapter::SaveOpenResourceMessage, this))) {
      // The callback sent a reply to the untrusted side.
      return;
    }
  }

  if (message->is_sync())
    io_thread_data_.pending_sync_msgs_[id] = message->type();
  io_thread_data_.channel_->Send(message.release());
}

void NaClIPCAdapter::SaveMessage(
    const IPC::Message& msg,
    std::unique_ptr<RewrittenMessage> rewritten_msg) {
  lock_.AssertAcquired();
  // There is some padding in this structure (the "padding" member is 16
  // bits but this then gets padded to 32 bits). We want to be sure not to
  // leak data to the untrusted plugin, so zero everything out first.
  NaClMessageHeader header;
  memset(&header, 0, sizeof(NaClMessageHeader));

  header.payload_size = static_cast<uint32_t>(msg.payload_size());
  header.routing = msg.routing_id();
  header.type = msg.type();
  header.flags = msg.flags();
  header.num_fds = static_cast<uint16_t>(rewritten_msg->desc_count());

  rewritten_msg->SetData(header, msg.payload_bytes().data(),
                         msg.payload_bytes().size());
  locked_data_.to_be_received_.push(std::move(rewritten_msg));
}

int TranslatePepperFileReadWriteOpenFlagsForTesting(int32_t pp_open_flags) {
  return TranslatePepperFileReadWriteOpenFlags(pp_open_flags);
}
